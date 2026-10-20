#ifndef PROVIDERS_H
#define PROVIDERS_H

#include <QString>
#include <QStringList>

// Mistral AI is the only provider: the Conversations API (/v1/conversations)
// exists nowhere else. What remains here are the constants the rest of the
// app needs and the catalogue-entry filters used when /v1/models answers.
namespace Providers {

extern const QString ID;
extern const QString NAME;
extern const QString BASE_URL;
extern const QString KEY_URL;
extern const QString REGION;

// Used until /v1/models has been fetched once.
QStringList fallbackModels();
QString defaultModel();

// True for catalogue entries that are not chat models (embeddings, speech,
// image generation, moderation). Providers rarely tag them, so this goes by
// name; a chat model that trips it would only be missing from the picker.
bool isChatModelId(const QString &modelId);

// Best guess at image support from the model name, for catalogues that do not
// declare capabilities. False on doubt: the attachment button stays hidden
// rather than sending a request the model will reject.
bool looksLikeVisionModel(const QString &modelId);

}

#endif // PROVIDERS_H
