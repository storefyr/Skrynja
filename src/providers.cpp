#include "providers.h"
#include <QObject>

namespace {

// Substrings that mark a catalogue entry as something other than a chat model.
const char * const NON_CHAT[] = {
    "embed", "bge-", "rerank", "whisper", "-tts", "tts-", "voice", "audio",
    "stable-diffusion", "flux", "-image", "image-", "dall-e", "moderation",
    "guard", "-ocr", "ocr-"
};

const int NON_CHAT_COUNT = int(sizeof(NON_CHAT) / sizeof(NON_CHAT[0]));

const char * const VISION_HINTS[] = {
    "pixtral", "llava", "vision", "-vl-", "vl-", "-vl", "gemma-3", "gpt-4o"
};

const int VISION_HINT_COUNT = int(sizeof(VISION_HINTS) / sizeof(VISION_HINTS[0]));

}

namespace Providers {

const QString ID = QStringLiteral("mistral");
const QString NAME = QObject::tr("Mistral AI");
const QString BASE_URL = QStringLiteral("https://api.mistral.ai/v1");
const QString KEY_URL = QStringLiteral("console.mistral.ai");
const QString REGION = QObject::tr("France (EU)");

QStringList fallbackModels()
{
    return QStringList() << "mistral-small-latest"
                         << "mistral-large-latest"
                         << "pixtral-12b-latest";
}

QString defaultModel()
{
    return fallbackModels().first();
}

bool isChatModelId(const QString &modelId)
{
    const QString id = modelId.toLower();
    if (id.isEmpty()) {
        return false;
    }

    for (int i = 0; i < NON_CHAT_COUNT; ++i) {
        if (id.contains(QString::fromLatin1(NON_CHAT[i]))) {
            return false;
        }
    }
    return true;
}

bool looksLikeVisionModel(const QString &modelId)
{
    const QString id = modelId.toLower();
    for (int i = 0; i < VISION_HINT_COUNT; ++i) {
        if (id.contains(QString::fromLatin1(VISION_HINTS[i]))) {
            return true;
        }
    }
    return false;
}

}
