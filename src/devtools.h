#ifndef DEVTOOLS_H
#define DEVTOOLS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

// DevTools — F19: developer tools for the AI inside Skrynja (EXPERIMENTAL).
//
// Gives the model local tools so the user can continue developing Skrynja
// from inside Skrynja. Enabled PER CONVERSATION (see ConversationManager).
//
// The model emits tool calls as XML-ish blocks in its answer:
//
//   <tool name="read_file"><path>src/main.cpp</path></tool>
//   <tool name="run_command"><command>git status</command></tool>
//
// ChatPage parses these blocks (parseToolCalls), runs them via this class,
// and feeds the results back to the model as a new user turn.
//
// SECURITY: every path is resolved against the workspace root and rejected
// when it escapes it (path traversal). Commands run inside the workspace
// with a timeout. The workspace lives in the app's private data directory
// (SailJail-sandboxed), so nothing outside the app's own files is touched.
//
// The six tools:
//   read_file    — read a UTF-8 file (max 200 KB)
//   list_dir     — list a directory (max 500 entries)
//   search       — grep-like search in the workspace (max 100 matches)
//   edit_file    — exact search/replace in a file (one occurrence)
//   write_file   — create/overwrite a file (max 1 MB)
//   run_command  — run a shell command in the workspace (60 s timeout)
class DevTools : public QObject
{
    Q_OBJECT

public:
    explicit DevTools(QObject *parent = 0);

    // The workspace root inside the app's private data directory. Created
    // (and the repo cloned into it) on first use — see ensureWorkspace().
    Q_INVOKABLE QString workspaceRoot() const;

    // True after ensureWorkspace() succeeded. ChatPage uses this to decide
    // whether to inject the tool instructions into the system prompt.
    Q_INVOKABLE bool workspaceReady() const;

    // Clones https://github.com/storefyr/Skrynja into the workspace on
    // first use. Safe to call repeatedly: a no-op when the workspace
    // already exists. Returns false when git is missing or the clone
    // failed — read lastError() for the reason.
    // NOTE: no QString* out-parameter: QML cannot pass one (same reason
    // VoiceAnonymizer exposes anonymizeFile() returning the error).
    Q_INVOKABLE bool ensureWorkspace();
    // The error from the last failed ensureWorkspace() call, empty when the
    // last call succeeded.
    Q_INVOKABLE QString lastError() const;

    // Parse <tool name="...">...</tool> blocks from a model answer.
    // Returns a list of {name, args} maps; args maps parameter names to
    // their text content. Malformed blocks are skipped, not fatal.
    Q_INVOKABLE static QVariantList parseToolCalls(const QString &text);

    // Strip tool-call blocks from a text (for displaying the answer without
    // the raw XML). Returns the remaining text, trimmed.
    Q_INVOKABLE static QString stripToolCalls(const QString &text);

    // Run a tool by name. args is a map with the parameter names documented
    // per tool. Returns a map: {ok: bool, output: string} on success,
    // {ok: false, error: string} on failure. Never throws.
    Q_INVOKABLE QVariantMap runTool(const QString &name,
                                    const QVariantMap &args);

signals:
    // Emitted while a tool runs, so the UI can show what is happening.
    void toolStarted(const QString &name);
    void toolFinished(const QString &name, bool ok);

private:
    QVariantMap readFile(const QString &relPath);
    QVariantMap listDir(const QString &relPath);
    QVariantMap search(const QString &pattern, const QString &relDir);
    QVariantMap editFile(const QString &relPath, const QString &oldText,
                         const QString &newText);
    QVariantMap writeFile(const QString &relPath, const QString &content);
    QVariantMap runCommand(const QString &command);

    // Resolve a workspace-relative path to an absolute path inside the
    // workspace. Returns an empty string when the path escapes the
    // workspace (absolute paths, ".." traversal).
    QString resolvePath(const QString &relPath) const;

    // Recursive grep-like search. Returns matches as "path:line: text".
    void searchRecursive(const QString &dir, const QString &pattern,
                         QStringList &results, int &budget) const;

    bool m_workspaceReady;
    QString m_lastError;
};

#endif // DEVTOOLS_H
