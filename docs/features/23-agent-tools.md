# 23 — Agent tools: local function calling (Skrynja develops itself)

**Status:** in progress
**Depends on:** F2 (Conversations API rewrite), F3 (web_search tool), F13 (unit tests)
**Touches:** `src/agenttools.{h,cpp}` (new), `src/mistralapi.{h,cpp}`, `src/harbour-skrynja.cpp`, `src/settingsmanager.{h,cpp}`, `qml/pages/ChatPage.qml`, `qml/pages/SettingsPage.qml`, `harbour-skrynja.pro`, `tests/tst_main.cpp`, `docs/ROADMAP.md`

## Rationale

Skrynja is developed with Mistral Vibe, an AI coding agent. The same capability — an AI
that reads files, edits code, searches a codebase and runs builds — should be available
*inside* Skrynja on the phone. Mistral's Conversations API supports user-defined function
tools executed client-side: the model emits a `function.call` entry, the client runs the
function locally, appends a `function.result` entry, and the model continues. This is the
same protocol Vibe uses, transplanted into the app.

The goal is narrow and concrete: **continue developing Skrynja from within Skrynja**. The
tools operate on a project directory the user picks explicitly. There is no implicit access
to the whole filesystem.

## Tool set

Five functions, each with a JSON schema declared at conversation creation:

| Function | Parameters | Returns | Purpose |
|---|---|---|---|
| `read_file` | `path` (string, relative to project root) | file content as text (truncated to 2000 lines / 100 KB) | Read source code, QML, docs |
| `list_files` | `path` (string, optional, relative dir), `pattern` (string, optional glob like `*.qml`) | newline-separated relative paths, capped at 500 entries | Explore the project structure |
| `search_code` | `query` (string), `path` (string, optional subdir) | matching lines with `path:line: content`, capped at 100 matches | Find symbols, usages, strings |
| `edit_file` | `path` (string), `old_text` (string), `new_text` (string) | success message with the number of replacements, or an error | Apply a precise edit (exact-match replace, like Vibe's `search_replace`) |
| `run_command` | `command` (string) | stdout + stderr (truncated to 4000 chars), exit code | Run `qmake`, `make`, tests, `git status`, etc. |

All paths are **relative to the project root** and are resolved with a canonicalization
check: the resolved absolute path must start with the canonical project root path. Any
`..` escape, symlink escape, or absolute path outside the root is rejected with an error
string (never an exception — the model reads the error and can correct itself).

`run_command` is the only tool that can have side effects beyond the project directory. It
is gated behind a per-conversation setting (default **off**): when disabled, the tool is
not even declared in the `tools` array, so the model cannot call it.

## Where the project root lives

The user picks a project directory once in Settings (a `ValueButton` that opens the
Sailfish file picker — `Sailfish.Pickers` is Harbour-allowed). The path is stored in
`SettingsManager` as `agent/projectRoot`. The picker returns a `file://` URL; the app
converts it to a local path and stores the canonical form.

SailJail note: the app's sandbox has `Documents` and `Pictures` permissions. A project
under `~/Documents/` is reachable. Cloned repos under `~/` outside the sandbox-visible
dirs need the user to move them into Documents, or the user grants broader permissions
manually. The Settings UI says this plainly.

## Wire protocol (Conversations API function calling)

The `tools` array at conversation creation gains `{"type": "function", "function": {...}}`
entries alongside `web_search`:

```json
"tools": [
  {"type": "web_search"},
  {"type": "function", "function": {
    "name": "read_file",
    "description": "Read a file from the project. Path is relative to the project root.",
    "parameters": {"type": "object", "properties": {
      "path": {"type": "string", "description": "Relative path, e.g. src/mistralapi.cpp"}
    }, "required": ["path"]}
  }}
]
```

When the model wants a tool, the SSE stream emits `event: function.call.delta` events.
Arguments arrive as **deltas** (JSON string fragments) that must be accumulated by
`tool_call_id` until the arguments parse as complete JSON. Each delta carries:
`id`, `name`, `tool_call_id`, `arguments` (a string fragment), `type: "function.call.delta"`.

After the last delta for a call, the client:

1. Parses the accumulated `arguments` JSON.
2. Dispatches to the matching `AgentTools` method.
3. Appends a `FunctionResultEntry` to the conversation:

```json
POST /v1/conversations/{id}  {"stream": true, "inputs": [
  {"object": "entry", "type": "function.result",
   "tool_call_id": "<id>", "result": "<JSON-stringified tool output>"}
]}
```

4. Streams the model's follow-up (which may be another `function.call.delta` chain or
   the final text answer).

The loop is bounded: **max 10 tool calls per user turn**. After the limit, the client
appends a synthetic result `{"error": "tool call limit reached"}` so the model stops
and answers.

## Architecture

```
ChatPage (QML)
  └─ mistralApi.sendMessage(..., agentMode=true)
       └─ MistralAPI::sendMessage — adds function tools to create body
            └─ SSE stream
                 ├─ message.output.delta → streamingResponse (as today)
                 ├─ function.call.delta → accumulate by tool_call_id
                 └─ conversation.response.done → if pending tool calls: execute + append
                      └─ AgentTools::execute(name, argsJson) → result string
                           └─ POST function.result entries → stream continues
```

`AgentTools` is a pure C++ class (no Silica dependency) so it is unit-testable on plain
Ubuntu Qt like the rest of `src/`:

- `AgentTools(QObject *parent)` — constructed with the project root set later.
- `setProjectRoot(path)` / `projectRoot()` — canonicalized; empty means tools disabled.
- `Q_INVOKABLE QString execute(const QString &name, const QString &argumentsJson)` —
  dispatches to the right method, returns a JSON-stringified result
  (`{"ok": true, ...}` or `{"error": "..."}`). Never throws.
- `static QJsonArray toolSchemas()` — the `tools` array fragment for `buildTurnBody`.
- Path guard: `resolvePath(relative)` returns the canonical absolute path or an empty
  string when the path escapes the root.

`MistralAPI` gains:

- `sendMessage(..., bool agentMode, const QString &projectRoot)` — when `agentMode` and
  the project root is non-empty, `buildTurnBody` appends `AgentTools::toolSchemas()` to
  the `tools` array (create-time only, like `web_search`).
- SSE parsing: `function.call.delta` events accumulate into a map
  `tool_call_id → {name, arguments}`. On `conversation.response.done`, if the map is
  non-empty, the pending calls are handed to a callback.
- A new signal `functionCallRequested(QVariantList calls)` where each call is
  `{tool_call_id, name, arguments}` (arguments already parsed to a QVariantMap).
  `ConversationManager` (or a new `AgentRunner`) connects to it, executes each call via
  `AgentTools`, and calls `mistralApi.appendFunctionResults(apiKey, remoteId, results)`.
- `appendFunctionResults(apiKey, remoteConversationId, QVariantList results)` — builds the
  `function.result` entries body and POSTs it with `"stream": true`, reusing the same
  SSE pipeline. The follow-up stream may produce more `function.call.delta` events, so
  the loop is recursive up to the call limit.

To keep the change small, the execution loop lives in `ConversationManager::bindApi()`:
it already owns the streaming lifecycle. When `functionCallRequested` fires, it runs the
tools synchronously (they are fast local file ops; `run_command` uses `QProcess` with a
30 s timeout), then calls `appendFunctionResults`. The UI shows tool activity via the
existing `toolActivity(name)` signal — `AgentTools` names are emitted as
`read_file: src/...` so the chat shows what the agent is doing.

## Settings & UI

- `SettingsManager`: `agentEnabled` (bool, default false), `agentProjectRoot` (string),
  `agentRunCommand` (bool, default false).
- SettingsPage: an "Agent mode" section, visible only when a project root is set:
  - project root picker (ValueButton + FilePicker from `Sailfish.Pickers`)
  - "Enable agent tools" TextSwitch
  - "Allow run_command" TextSwitch (only meaningful when agent tools are on)
- ChatPage: when the active conversation was created in agent mode, tool calls show as
  a collapsible "🔧 read_file src/mistralapi.cpp" line above the answer, reusing the
  existing tool-activity indicator pattern from F3.

A conversation is created in agent mode when `agentEnabled` is on at creation time. The
mode is fixed per conversation (like the model), because `tools` is create-time only.
The conversation settings page shows whether agent mode is active.

## System prompt

When agent mode is on, the default instructions gain a short paragraph (appended to
whatever system prompt the user set, not replacing it):

> You are running inside Skrynja on a Sailfish OS phone, helping develop the Skrynja
> app itself. You have local tools: read_file, list_files, search_code, edit_file (and
> run_command if enabled). The project root is the Skrynja repository. Paths are relative
> to it. Prefer edit_file with exact old_text matches over rewriting whole files. After
> editing C++ or QML, suggest running the build or tests with run_command.

This is appended in `ConversationManager` when building the turn, so it does not
pollute the user's saved system prompt.

## Safety

- Project root is user-picked and canonicalized. All tool paths are resolved and checked
  to stay under the root. No absolute paths, no `..` escape, no symlink escape.
- `edit_file` requires an exact `old_text` match; it never does fuzzy or partial
  replacement. Zero matches → error result (the model retries). Multiple matches →
  error result asking for more context. This mirrors Vibe's `search_replace` semantics.
- `run_command` is opt-in, off by default, and runs with a 30 s timeout and the project
  root as working directory. No shell interpretation beyond what `QProcess` does with a
  single argument string split on whitespace (no pipes, no redirects, no `$()`).
- Tool results are truncated (file reads 100 KB, command output 4000 chars, file lists
  500 entries) so a runaway read does not blow the context window.
- The agent cannot see the API key: `AgentTools` has no access to `SettingsManager`.
  Results never include environment variables or config files outside the project root.
- A hard cap of 10 tool calls per user turn bounds the cost and the time.

## Acceptance criteria

- [ ] With agent mode off (default), nothing changes: no tools in the request body, no
      extra UI.
- [ ] With agent mode on and a project root set, a new conversation's create body
      includes the function tool schemas.
- [ ] `read_file` with a valid relative path returns file content; with `../../etc/passwd`
      returns an error result, no file access.
- [ ] `edit_file` with a unique `old_text` applies the edit; with zero or multiple
      matches returns an error.
- [ ] `search_code` finds a known symbol in the project and returns `path:line:` matches.
- [ ] `run_command` is absent from the tools array when `agentRunCommand` is off.
- [ ] A `function.call.delta` stream is reassembled by `tool_call_id` even when
      arguments arrive across multiple deltas.
- [ ] After a tool result is appended, the model's final text streams normally.
- [ ] 10 tool calls in one turn stops the loop with an error result.
- [ ] Unit tests cover: path guard (escape rejected), edit_file semantics, tool schema
      JSON shape, function.call.delta reassembly, function.result body shape.
- [ ] CI unit tests pass; armv7hl build passes.

## Open questions (resolved in implementation)

- **Parallel tool calls:** Mistral may emit several `function.call.delta` for different
  `tool_call_id`s before any result. The client executes them sequentially in arrival
  order and appends all results in one `function.result` batch. Simpler and correct.
- **Cancellation mid-tool-loop:** `cancelRequest()` aborts the in-flight HTTP request.
  Pending tool calls that have not been sent yet are dropped (the conversation is marked
  dirty as today). A tool call whose result was already appended is not retracted — the
  next user message continues from there.
- **Tool activity in the local model:** tool calls and results are not added to the local
  `ConversationModel` as messages; they appear only as transient activity indicators.
  The server-side history keeps them, so a re-attach (F17) shows them via
  `GET .../messages` — those entries are filtered to text messages only, as today.

## Notes

- The `function.call.delta` event name and the `function.result` entry shape come from the
  Mistral Python SDK's `ConversationEvents` / `FunctionCallEvent` / `FunctionResultEntry`
  models (`function.call.delta` SSE event; `{"object":"entry","type":"function.result",`
  `"tool_call_id":"...","result":"..."}` body entry).
- `AgentTools` deliberately mirrors the tool names of the Vibe agent (read, edit,
  search, list, run) so prompts and muscle memory transfer.
- This feature is the foundation for later work: MCP connectors (server-side tools) and
  Mistral Agents (`agent_id` instead of `model`) are both compatible with the same
  `tools` array and would slot in without changing `AgentTools`.
