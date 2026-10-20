# Skrynja 🐱⛵

<p align="center">
  <img src="icons/172x172/harbour-skrynja.png" alt="Skrynja Logo" width="172"/>
</p>

**Skrynja** is an elegant AI chat client, specifically designed for **Sailfish OS**. Talk to **Mistral AI** directly from your Sailfish device, with live **web search** powered by Mistral's Conversations API.

Skrynja is a fork of [SailCat](https://github.com/nicosouv/harbour-sailcat), rewritten to use Mistral's `/v1/conversations` endpoint instead of `/v1/chat/completions`.

> 🤖 **Skrynja is developed with the help of AI** — Mistral Vibe. Features are specified in [docs/features/](docs/features/), implemented together by a human developer and AI agents, validated by CI on three architectures and covered by unit tests before every release.

## ✨ Features

- 🌐 **Web search** — The models can search the internet mid-conversation, with sources
- 🇫🇷 **Mistral AI only** — Built on the Conversations API (`/v1/conversations`)
- 🆓 **Free tier supported** — Mistral's Experiment plan works out of the box
- 🔑 **Your own key** — Bring your own API key, stored scrambled, never sent anywhere else
- ⚡ **Real-time streaming** — Instant and smooth responses
- 🖼️ **Image support (vision)** — Attach a photo and let Pixtral analyze it
- 🧠 **Live model list** — Fetched from Mistral, with a quick-switch button next to the input
- 🎭 **System prompt & personas** — Presets (Concise, Translator, Code assistant) or your own instruction
- 🎛️ **Generation settings** — Temperature and response length
- 💬 **Conversation history** — Search, per-conversation details, automatic titles and categories
- 📌 **Message actions** — Pin and copy code blocks
- 📤 **Markdown export** — To Documents or clipboard
- 📊 **Statistics & badges** — Activity charts, real token tracking (per day/month/conversation), question categories, fun badges and top AI words
- 🎨 **Native Sailfish interface** — Swipe to history, bottom pulley menu, animations, live cover with the latest answer
- 🌍 **Multilingual** — English, French, German, Spanish, Finnish, Italian, Norwegian, Swedish

## 🚀 Installation

### Prerequisites

- Sailfish OS 3.0+ or higher
- Internet connection
- A Mistral AI API key (free tier available)

### Build from source

```bash
# Clone the repo
git clone https://github.com/storefyr/Skrynja.git
cd harbour-skrynja

# Build with Sailfish SDK
sfdk build

# Install the generated RPM
sfdk deploy --manual
```

### RPM Installation

Download the `.rpm` file from [releases](https://github.com/storefyr/Skrynja/releases) and install it on your Sailfish device.

## 🔧 Configuration

1. Launch Skrynja
2. Access **Settings** via the pulley menu
3. Paste your **Mistral API key**
4. Choose your preferred model
5. Optionally enable **web search**
6. Save and start chatting!

## 📖 Usage

### Start a conversation

1. Open Skrynja
2. Type your message in the input field
3. Press the send button or Enter
4. Watch the response appear in real-time thanks to streaming

### Web search

Enable **Web search** in Settings (or per conversation) and the model will search the internet when it needs up-to-date information. While it searches, Skrynja shows a "Searching the web…" indicator, and the answer includes its sources.

### New conversation

Use the pulley menu and select **"New conversation"** to start fresh.

### Available models

The model list is fetched live from Mistral and cached for offline use. The model is chosen when a conversation is created; start a new conversation to switch models.

## 🏗️ Technical Architecture

### Qt C++ Backend

- **MistralAPI** — HTTP request management with SSE (Server-Sent Events) streaming against `/v1/conversations`
- **ConversationModel** — QAbstractListModel for message display
- **ConversationManager** — Local conversation persistence and remote conversation-id tracking
- **SettingsManager** — Settings persistence with QSettings

### QML Frontend

- **ChatPage** — Main conversation interface with SilicaListView
- **SettingsPage** — API configuration and model selection
- **ConversationHistoryPage** — Browse past conversations
- **CoverPage** — Active cover with statistics

### Technologies used

- Qt 5.6 (QtCore, QtNetwork, QtQuick, QtQml)
- Sailfish Silica UI Components
- Mistral Conversations API (REST + SSE streaming)
- QML + JavaScript for the interface

## 🎯 Mistral Conversations API

Skrynja talks exclusively to Mistral's Conversations API, which is what makes web search possible (the `web_search` tool only exists on this API, not on Chat Completions).

### Endpoints used

```
POST https://api.mistral.ai/v1/conversations#stream          (create a conversation)
POST https://api.mistral.ai/v1/conversations/{id}#stream     (append a message)
POST https://api.mistral.ai/v1/conversations/{id}/restart#stream  (after a cancelled answer)
GET  https://api.mistral.ai/v1/conversations/{id}/messages   (reconcile before retrying)
DELETE https://api.mistral.ai/v1/conversations/{id}          (delete a conversation)
```

### Request format

```json
{
  "model": "mistral-small-latest",
  "instructions": "You are a helpful assistant.",
  "tools": [{"type": "web_search"}],
  "inputs": [{"object": "entry", "type": "message.input", "role": "user", "content": "Hello!"}],
  "completion_args": {"temperature": 0.7}
}
```

### Design decisions (vs. SailCat)

| Topic | Skrynja |
|---|---|
| Providers | Mistral AI only |
| Server-side history | Yes — the API remembers the conversation |
| Context limit trimming | Removed (the server keeps the full history) |
| Model switching | Fixed per conversation; start a new one to switch |
| Edit / regenerate | Removed (needs entry-level restart, deferred) |
| Retry after ambiguous failure | Reconciles with `GET .../messages` first |
| Cancelled answer | Marked dirty; next message restarts from before it |
| Existing conversations | Start fresh server-side; old messages stay local |
| Privacy | Conversation history is stored on Mistral's servers |

## 🔒 Security & Privacy

- ✅ API keys are stored locally with QSettings, isolated by Sailjail sandboxing
- ✅ No telemetry or analytics
- ✅ Direct communication with Mistral (HTTPS), no intermediary
- ✅ No intermediate server
- ✅ Local copy of conversations on your device
- ⚠️ **With the Conversations API, the conversation history also lives on Mistral's servers** — that is the price of server-side memory and web search
- ⚠️ On Mistral's free "Experiment" plan, API inputs and outputs go into their model training programs by default. Opt out in the Admin Console privacy settings if that matters to you
- ⚠️ Your API key gives access to your Mistral account - keep it secret

## 🚀 Releases & CI/CD

Skrynja uses GitHub Actions to automatically build and publish releases.

### Automatic build

Each `vX.Y.Z` tag triggers a multi-architecture build:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The **build-docker.yml** workflow:
- ✅ Builds for armv7hl, aarch64, and i486
- ✅ Generates changelog from commits
- ✅ Creates GitHub release with RPMs
- ✅ Publishes automatically

Compiled RPMs are available in [Releases](https://github.com/storefyr/Skrynja/releases).

### PR Validation

Pull Requests are automatically validated with the **pr-build.yml** workflow that builds for armv7hl and runs the unit test suite.

### Tests

Backend classes are covered by a QtTest suite (`tests/`) that runs in CI on every build and blocks releases on failure:

```bash
cd tests && qmake tests.pro && make && ./tst_skrynja
```

### For maintainers

See [RELEASE.md](RELEASE.md) for the complete release guide.

## 🤝 Contributing

Contributions are welcome! Here's how to participate:

1. Fork the project
2. Create a branch for your feature (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📝 TODO / Roadmap

The detailed feature roadmap with per-feature specs lives in [docs/ROADMAP.md](docs/ROADMAP.md).

- [x] Web search via the Conversations API
- [x] Conversations API rewrite (create/append/restart/reconcile/delete)
- [x] Persistent conversation saving
- [x] Conversation export (text, markdown)
- [x] Multiple simultaneous conversations
- [x] Advanced settings (temperature, max_tokens, system prompt)
- [x] Translations
- [x] Statistics, token tracking, badges
- [ ] Edit & regenerate via entry-level restart
- [ ] Mistral agents support
- [ ] Custom color themes

## 🐛 Known Issues

- Free tier rate limits can be restrictive for intensive usage
- Streaming can sometimes be slow depending on network connection
- No offline support (requires Internet connection)

## 📄 License

MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Mistral AI** for their excellent API, the Conversations API and the generous free tier
- **Jolla** for Sailfish OS and the Silica framework
- **The Sailfish community** for their support and feedback
- **[SailCat](https://github.com/nicosouv/harbour-sailcat)** — Skrynja started as a fork

## 📧 Contact

[@storefyr](https://github.com/storefyr)

Project link: [https://github.com/storefyr/Skrynja](https://github.com/storefyr/Skrynja)

---

<p align="center">
  Made with ❤️ for Sailfish OS
</p>
