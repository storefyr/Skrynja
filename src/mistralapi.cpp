#include "mistralapi.h"
#include "categories.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHttpMultiPart>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QDebug>
#include <QStringList>
#include <algorithm>

static const int REQUEST_TIMEOUT_MS = 60000;

// Free tiers are strict: Mistral answers one request per second and turns the
// rest away with a 429. Space every call out rather than discovering the limit
// by hitting it.
static const int MIN_REQUEST_INTERVAL_MS = 1200;
static const int MAX_RATE_LIMIT_RETRIES = 3;
// Doubled on each attempt: 2s, 4s, 8s.
static const int RATE_LIMIT_BACKOFF_MS = 2000;
static const int RATE_LIMIT_MAX_WAIT_MS = 30000;

// Keep an inlined image bounded: the request body would otherwise carry the
// gallery original, base64-inflated by a third.
static const int MAX_IMAGE_DIMENSION = 1024;
static const int JPEG_QUALITY = 85;

static const char * const MISTRAL_BASE_URL = "https://api.mistral.ai/v1";

// Statuses that mean "not now" rather than "no": the same request sent again
// a moment later usually goes through.
static bool isTransientStatus(int status)
{
    return status == 429      // rate limited
            || status == 502  // bad gateway, seen when a provider recycles a node
            || status == 503  // capacity exhausted
            || status == 529; // overloaded
}

// How long the provider wants us to wait. Retry-After is either a number of
// seconds or an HTTP date; only the first form is worth honouring here, the
// backoff covers the rest.
static int retryDelayMs(QNetworkReply *reply, int attempt)
{
    if (reply) {
        bool ok = false;
        const int seconds = reply->rawHeader("Retry-After").trimmed().toInt(&ok);
        if (ok && seconds > 0) {
            return qBound(1000, seconds * 1000, RATE_LIMIT_MAX_WAIT_MS);
        }
    }
    return qMin(RATE_LIMIT_BACKOFF_MS << attempt, RATE_LIMIT_MAX_WAIT_MS);
}

// Require a verified peer and refuse the legacy TLS versions rather than
// inheriting the platform default.
static void applyTransportSecurity(QNetworkRequest &request)
{
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    request.setSslConfiguration(ssl);

    // Do not let a redirect move the request (and its bearer token) elsewhere.
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
}

// Downscale and encode an attached image as a data URL, or an empty string.
static QString imageToDataUrl(const QString &filePath)
{
    QString localPath = filePath;
    if (localPath.startsWith("file:")) {
        localPath = QUrl(localPath).toLocalFile();
    }
    QImage img(localPath);
    if (img.isNull()) {
        qWarning() << "Cannot load image:" << localPath;
        return QString();
    }
    if (img.width() > MAX_IMAGE_DIMENSION || img.height() > MAX_IMAGE_DIMENSION) {
        img = img.scaled(MAX_IMAGE_DIMENSION, MAX_IMAGE_DIMENSION,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPEG", JPEG_QUALITY);
    buffer.close();
    if (bytes.isEmpty()) {
        return QString();
    }
    return QString("data:image/jpeg;base64,") + QString::fromLatin1(bytes.toBase64());
}

MistralAPI::MistralAPI(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_requestTimer(new QTimer(this))
    , m_isBusy(false)
    , m_timedOut(false)
    , m_keyRequired(true)
    , m_minRequestIntervalMs(MIN_REQUEST_INTERVAL_MS)
    , m_retryCount(0)
    , m_streamStarted(false)
    , m_streamDone(false)
    , m_reconciling(false)
    , m_reconcileReply(nullptr)
{
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(REQUEST_TIMEOUT_MS);
    connect(m_timeoutTimer, &QTimer::timeout, this, &MistralAPI::onTimeout);

    m_requestTimer->setSingleShot(true);
    connect(m_requestTimer, &QTimer::timeout,
            this, &MistralAPI::sendPendingChatRequest);

    // The only provider: Mistral AI. The base URL is fixed; a custom endpoint
    // speaks the OpenAI dialect and cannot serve /v1/conversations.
    m_baseUrl = QString::fromLatin1(MISTRAL_BASE_URL);
}

int MistralAPI::throttleDelayMs() const
{
    if (m_minRequestIntervalMs <= 0 || !m_lastRequestAt.isValid()) {
        return 0;
    }
    const qint64 elapsed = m_lastRequestAt.elapsed();
    if (elapsed >= m_minRequestIntervalMs) {
        return 0;
    }
    return int(m_minRequestIntervalMs - elapsed);
}

void MistralAPI::noteRequestSent()
{
    m_lastRequestAt.start();
}

QUrl MistralAPI::endpoint(const QString &path) const
{
    if (m_baseUrl.isEmpty()) {
        return QUrl();
    }
    return QUrl(m_baseUrl + path);
}

void MistralAPI::prepareRequest(QNetworkRequest &request, const QString &apiKey) const
{
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // A provider that answers anonymously gets no header at all: an empty
    // bearer token is worse than none.
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    }
    applyTransportSecurity(request);
}

bool MistralAPI::isBusy() const
{
    return m_isBusy;
}

QString MistralAPI::error() const
{
    return m_error;
}

QString MistralAPI::turnPath(const QString &remoteConversationId) const
{
    if (remoteConversationId.isEmpty()) {
        return QString("/conversations");
    }
    return QString("/conversations/%1").arg(remoteConversationId);
}

QByteArray MistralAPI::buildTurnBody(const QString &modelName,
                                     const QString &systemPrompt,
                                     const QString &userContent,
                                     const QString &imagePath,
                                     bool webSearch,
                                     bool isCreate,
                                     double temperature,
                                     int maxTokens,
                                     const QString &imageFileId) const
{
    // The user message as a MessageInputEntry. content is a plain string for
    // text, a chunk list when an image is attached.
    QJsonObject input;
    input["object"] = "entry";
    input["type"] = "message.input";
    input["role"] = "user";

    // An uploaded file is referenced by id (F14); an inline image falls back
    // to a base64 data URL. The file chunk keeps the request body small: the
    // image is stored server-side once, not resent on every turn.
    const QString dataUrl = imagePath.isEmpty() ? QString() : imageToDataUrl(imagePath);
    if (!imageFileId.isEmpty()) {
        QJsonArray chunks;
        if (!userContent.isEmpty()) {
            QJsonObject textChunk;
            textChunk["type"] = "text";
            textChunk["text"] = userContent;
            chunks.append(textChunk);
        }
        QJsonObject fileChunk;
        fileChunk["type"] = "file";
        fileChunk["file_id"] = imageFileId;
        chunks.append(fileChunk);
        input["content"] = chunks;
    } else if (!dataUrl.isEmpty()) {
        QJsonArray chunks;
        if (!userContent.isEmpty()) {
            QJsonObject textChunk;
            textChunk["type"] = "text";
            textChunk["text"] = userContent;
            chunks.append(textChunk);
        }
        QJsonObject imageChunk;
        imageChunk["type"] = "image_url";
        imageChunk["image_url"] = dataUrl;
        chunks.append(imageChunk);
        input["content"] = chunks;
    } else {
        input["content"] = userContent;
    }

    QJsonObject requestBody;
    QJsonArray inputs;
    inputs.append(input);
    requestBody["inputs"] = inputs;
    // Streaming is a body parameter, not a URL fragment: Mistral's docs
    // show "#stream" as endpoint notation, but the wire path is plain
    // /v1/conversations with "stream": true in the JSON body.
    requestBody["stream"] = true;

    if (isCreate) {
        // Create-time only: model, instructions and tools are locked here.
        requestBody["model"] = modelName;
        if (!systemPrompt.trimmed().isEmpty()) {
            requestBody["instructions"] = systemPrompt.trimmed();
        }
        if (webSearch) {
            QJsonArray tools;
            QJsonObject webSearchTool;
            webSearchTool["type"] = "web_search";
            tools.append(webSearchTool);
            requestBody["tools"] = tools;
        }
    }

    // Omit unset parameters so the API applies its own defaults.
    QJsonObject completionArgs;
    if (temperature >= 0.0) {
        completionArgs["temperature"] = temperature;
    }
    if (maxTokens > 0) {
        completionArgs["max_tokens"] = maxTokens;
    }
    if (!completionArgs.isEmpty()) {
        requestBody["completion_args"] = completionArgs;
    }

    return QJsonDocument(requestBody).toJson(QJsonDocument::Compact);
}

void MistralAPI::sendMessage(const QString &apiKey,
                               const QString &modelName,
                               const QString &systemPrompt,
                               const QString &userContent,
                               const QString &imagePath,
                               bool webSearch,
                               const QString &remoteConversationId,
                               double temperature,
                               int maxTokens,
                               const QString &imageFileId)
{
    if (m_isBusy) {
        qWarning() << "Request already in progress";
        return;
    }

    if (apiKey.isEmpty() && m_keyRequired) {
        setError(tr("Missing API key. Please configure your API key in settings."));
        return;
    }

    if (m_baseUrl.isEmpty()) {
        setError(tr("No endpoint configured. Please pick a provider in settings."));
        return;
    }

    if (userContent.trimmed().isEmpty() && imagePath.isEmpty()) {
        qWarning() << "Empty user turn";
        setError(tr("Failed to prepare messages for API"));
        return;
    }

    const bool isCreate = remoteConversationId.isEmpty();

    setIsBusy(true);
    setError(QString());
    m_streamBuffer.clear();
    m_currentEvent.clear();
    m_timedOut = false;
    m_retryCount = 0;
    m_streamStarted = false;
    m_streamDone = false;
    m_pendingSources.clear();
    m_pendingThinking.clear();

    // Kept whole: a rate-limited request has to go out again byte for byte,
    // and the caller must not have to know that it ever came back.
    m_pendingApiKey = apiKey;
    m_pendingBody = buildTurnBody(modelName, systemPrompt, userContent,
                                  imagePath, webSearch, isCreate,
                                  temperature, maxTokens, imageFileId);
    m_pendingRemoteId = remoteConversationId;
    m_pendingUserContent = userContent;
    m_pendingImageFileId = imageFileId;

    // Announced here rather than once the POST leaves: the request may be held
    // back for a moment, and the empty assistant bubble has to belong to the
    // conversation that asked, not to whichever one is open a second later.
    // Every reason to refuse has been checked above; what can still fail from
    // here on ends in responseCompleted(), which clears the bubble.
    emit messageSent();

    queueChatRequest(0);
}

void MistralAPI::queueChatRequest(int extraDelayMs)
{
    const int delay = qMax(extraDelayMs, throttleDelayMs());
    if (delay <= 0) {
        sendPendingChatRequest();
        return;
    }
    m_requestTimer->start(delay);
}

void MistralAPI::sendPendingChatRequest()
{
    if (m_pendingBody.isEmpty()) {
        return;
    }

    QNetworkRequest request(endpoint(turnPath(m_pendingRemoteId)));
    request.setRawHeader("Accept", "text/event-stream");
    prepareRequest(request, m_pendingApiKey);

    noteRequestSent();
    m_currentReply = m_networkManager->post(request, m_pendingBody);

    connect(m_currentReply, &QNetworkReply::readyRead,
            this, &MistralAPI::onReadyRead);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &MistralAPI::onFinished);
    connect(m_currentReply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(onError(QNetworkReply::NetworkError)));

    m_timeoutTimer->start();
}

void MistralAPI::forgetPendingRequest()
{
    m_pendingApiKey.clear();
    m_pendingBody.clear();
    m_pendingRemoteId.clear();
    m_pendingUserContent.clear();
    m_pendingImageFileId.clear();
    m_retryCount = 0;
}

void MistralAPI::generateTitle(const QString &apiKey,
                                 const QString &modelName,
                                 const QString &conversationText,
                                 const QString &targetId)
{
    if ((apiKey.isEmpty() && m_keyRequired) || m_baseUrl.isEmpty()
            || conversationText.trimmed().isEmpty()) {
        emit titleGenerationFailed(targetId);
        return;
    }

    // Spaced out like any other call: a title request sent on the heels of
    // the answer it summarises is exactly what trips a one-per-second limit.
    const int wait = throttleDelayMs();
    if (wait > 0) {
        const QString key = apiKey;
        const QString model = modelName;
        const QString text = conversationText;
        const QString id = targetId;
        QTimer::singleShot(wait, this, [this, key, model, text, id]() {
            generateTitle(key, model, text, id);
        });
        return;
    }

    // A title is a one-shot completion: create a conversation, ask, and ask
    // the server not to store it. A plain string input keeps the body small.
    QJsonObject requestBody;
    requestBody["model"] = modelName;
    requestBody["store"] = false;
    requestBody["inputs"] = QString(
                "Analyze the conversation excerpt sent by the user and reply with ONLY a "
                "compact JSON object, no explanation, no code fences: "
                "{\"title\":\"short conversation title, max 50 characters, same language as "
                "the conversation\",\"category\":\"one of the labels below\"}. "
                "Pick the single most specific matching category from this list: %1. "
                "Use \"other\" only when no other label fits at all. The excerpt follows.\n\n"
                + conversationText)
            .arg(Categories::all().join(", "));

    QByteArray jsonData = QJsonDocument(requestBody).toJson(QJsonDocument::Compact);

    // Non-streaming: the response is a single conversation.response JSON.
    QNetworkRequest request(endpoint("/conversations"));
    prepareRequest(request, apiKey);

    noteRequestSent();
    QNetworkReply *reply = m_networkManager->post(request, jsonData);
    // Carried on the reply so the result can be routed back to the right
    // conversation: several of these can be in flight at once.
    reply->setProperty("targetId", targetId);

    connect(reply, &QNetworkReply::finished,
            this, &MistralAPI::onTitleGenerationFinished);
}

void MistralAPI::fetchModels(const QString &apiKey)
{
    if ((apiKey.isEmpty() && m_keyRequired) || m_baseUrl.isEmpty()) {
        emit modelsFetchFailed();
        return;
    }

    const int wait = throttleDelayMs();
    if (wait > 0) {
        const QString key = apiKey;
        QTimer::singleShot(wait, this, [this, key]() { fetchModels(key); });
        return;
    }

    QNetworkRequest request(endpoint("/models"));
    prepareRequest(request, apiKey);

    noteRequestSent();
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished,
            this, &MistralAPI::onModelsFetchFinished);
}

// ------------------------------------------------------- remote conversations

void MistralAPI::fetchRemoteConversations(const QString &apiKey)
{
    if (apiKey.isEmpty() && m_keyRequired) {
        emit remoteConversationsFetchFailed();
        return;
    }
    if (m_baseUrl.isEmpty()) {
        emit remoteConversationsFetchFailed();
        return;
    }

    QNetworkRequest request(endpoint("/conversations"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    prepareRequest(request, apiKey);

    noteRequestSent();
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished,
            this, &MistralAPI::onRemoteConversationsFinished);
}

void MistralAPI::fetchRemoteMessages(const QString &apiKey,
                                     const QString &remoteConversationId)
{
    if (apiKey.isEmpty() && m_keyRequired) {
        emit remoteMessagesFetchFailed(remoteConversationId);
        return;
    }
    if (m_baseUrl.isEmpty() || remoteConversationId.isEmpty()) {
        emit remoteMessagesFetchFailed(remoteConversationId);
        return;
    }

    QNetworkRequest request(endpoint(
                QString("/conversations/%1/messages").arg(remoteConversationId)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    prepareRequest(request, apiKey);

    noteRequestSent();
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("remoteConversationId", remoteConversationId);

    connect(reply, &QNetworkReply::finished,
            this, &MistralAPI::onRemoteMessagesFinished);
}

void MistralAPI::onRemoteConversationsFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    const int httpStatus = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Remote conversations fetch failed: HTTP" << httpStatus
                   << reply->errorString() << "body:" << body.left(500);
        emit remoteConversationsFetchFailed();
        return;
    }

    // The SDK returns List[ModelConversation|AgentConversation]: a bare JSON
    // array at the top level, not wrapped in {"data": [...]}.
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isArray()) {
        QJsonObject wrapper;
        wrapper["data"] = doc.array();
        const QVariantList conversations = parseRemoteConversations(wrapper);
        emit remoteConversationsFetched(conversations);
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "Remote conversations: unexpected response shape, HTTP"
                   << httpStatus << "body:" << body.left(500);
        emit remoteConversationsFetchFailed();
        return;
    }

    const QVariantList conversations = parseRemoteConversations(doc.object());
    emit remoteConversationsFetched(conversations);
}

void MistralAPI::onRemoteMessagesFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    const QString remoteId = reply->property("remoteConversationId").toString();
    const int httpStatus = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Remote messages fetch failed: HTTP" << httpStatus
                   << reply->errorString() << "body:" << body.left(500);
        emit remoteMessagesFetchFailed(remoteId);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        qWarning() << "Remote messages: unexpected response shape, HTTP"
                   << httpStatus << "body:" << body.left(500);
        emit remoteMessagesFetchFailed(remoteId);
        return;
    }

    const QVariantList messages = parseRemoteMessages(doc.object());
    emit remoteMessagesFetched(remoteId, messages);
}

void MistralAPI::deleteRemoteConversation(const QString &apiKey,
                                          const QString &remoteConversationId)
{
    if (apiKey.isEmpty() && m_keyRequired) {
        emit remoteConversationDeleteFailed(remoteConversationId);
        return;
    }
    if (m_baseUrl.isEmpty() || remoteConversationId.isEmpty()) {
        emit remoteConversationDeleteFailed(remoteConversationId);
        return;
    }

    QNetworkRequest request(endpoint(
                QString("/conversations/%1").arg(remoteConversationId)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    prepareRequest(request, apiKey);

    noteRequestSent();
    QNetworkReply *reply = m_networkManager->deleteResource(request);
    reply->setProperty("remoteConversationId", remoteConversationId);

    connect(reply, &QNetworkReply::finished,
            this, &MistralAPI::onRemoteDeleteFinished);
}

void MistralAPI::onRemoteDeleteFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    const QString remoteId = reply->property("remoteConversationId").toString();
    const int httpStatus = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    // 404 means the conversation is already gone server-side — that counts
    // as success for our purposes.
    if (reply->error() == QNetworkReply::NoError || httpStatus == 404) {
        emit remoteConversationDeleted(remoteId);
        return;
    }

    qWarning() << "Remote conversation delete failed: HTTP" << httpStatus
               << reply->errorString() << "body:" << body.left(500);
    emit remoteConversationDeleteFailed(remoteId);
}

QVariantList MistralAPI::parseRemoteConversations(const QJsonObject &doc)
{
    // GET /v1/conversations returns {"data": [...], "has_more": bool, ...}.
    // Older API versions returned a bare array; accept both.
    QJsonArray items;
    if (doc.contains("data") && doc["data"].isArray()) {
        items = doc["data"].toArray();
    } else if (doc.contains("conversations") && doc["conversations"].isArray()) {
        items = doc["conversations"].toArray();
    }

    QVariantList result;
    QSet<QString> seenIds;
    for (const QJsonValue &value : items) {
        const QJsonObject obj = value.toObject();
        const QString id = obj["id"].toString();
        // The API can return the same conversation more than once (e.g.
        // when pagination overlaps); dedupe by id so the UI shows each
        // conversation exactly once.
        if (id.isEmpty() || seenIds.contains(id)) {
            continue;
        }
        seenIds.insert(id);
        QVariantMap entry;
        entry["id"] = id;
        entry["name"] = obj["name"].toString();
        entry["model"] = obj["model"].toString();
        // Timestamps come as ISO 8601 strings; pass them through as-is so
        // the QML layer can format them.
        entry["created_at"] = obj["created_at"].toString();
        entry["updated_at"] = obj["updated_at"].toString();
        result.append(entry);
    }
    return result;
}

QVariantList MistralAPI::parseRemoteMessages(const QJsonObject &doc)
{
    // GET /v1/conversations/{id}/messages returns
    // {"conversation_id": "...", "messages": [...], "object": "conversation.messages"}.
    // Each message is a MessageInputEntry (role + content + type="message.input")
    // or MessageOutputEntry (content + type="message.output" + role="assistant").
    // Older API versions returned {"object": "list", "data": [...]}; accept both.
    QJsonArray items;
    if (doc.contains("messages") && doc["messages"].isArray()) {
        items = doc["messages"].toArray();
    } else if (doc.contains("data") && doc["data"].isArray()) {
        items = doc["data"].toArray();
    }

    QVariantList result;
    for (const QJsonValue &value : items) {
        const QJsonObject obj = value.toObject();
        const QString type = obj["type"].toString();
        const QString role = obj["role"].toString();

        // message.input → user (or assistant in some edge cases)
        // message.output → assistant
        // Legacy shape: role field directly
        QString effectiveRole;
        if (type == "message.input") {
            effectiveRole = role; // "user" or "assistant"
        } else if (type == "message.output") {
            effectiveRole = "assistant";
        } else {
            effectiveRole = role; // legacy: bare role field
        }

        if (effectiveRole != "user" && effectiveRole != "assistant") {
            continue;
        }

        QVariantMap entry;
        entry["role"] = effectiveRole;
        // Content is directly on the entry (string or chunk list), not
        // nested under "outputs".
        entry["content"] = messageContentToText(obj["content"]);

        // created_at is ISO 8601; parse to epoch ms for local storage.
        const QString createdAt = obj["created_at"].toString();
        const QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
        entry["timestamp"] = dt.isValid()
                ? dt.toMSecsSinceEpoch()
                : QDateTime::currentMSecsSinceEpoch();

        // Capture the model on assistant messages for statistics.
        if (effectiveRole == "assistant") {
            const QString model = obj["model"].toString();
            if (!model.isEmpty()) {
                entry["model"] = model;
            }
        }

        result.append(entry);
    }
    return result;
}

QString MistralAPI::messageContentToText(const QJsonValue &content)
{
    // Message content: a plain string, or a chunk list when the turn carried
    // an image (text + image_url/file chunks) or the model reasoned (thinking
    // + text chunks). Only text chunks are returned; thinking is dropped.
    if (content.isString()) {
        return content.toString();
    }
    if (content.isArray()) {
        QString text;
        const QJsonArray chunks = content.toArray();
        for (const QJsonValue &chunk : chunks) {
            const QJsonObject obj = chunk.toObject();
            if (obj["type"].toString() == "text") {
                text += obj["text"].toString();
            }
        }
        return text;
    }
    return QString();
}

// ---------------------------------------------------------------- image upload

void MistralAPI::uploadImage(const QString &apiKey, const QString &imagePath)
{
    if ((apiKey.isEmpty() && m_keyRequired) || m_baseUrl.isEmpty()) {
        emit imageUploadFailed();
        return;
    }

    QString localPath = imagePath;
    if (localPath.startsWith("file:")) {
        localPath = QUrl(localPath).toLocalFile();
    }

    // Downscale and re-encode before upload: the request body would otherwise
    // carry the gallery original, and the file id is what the conversation
    // keeps, so the stored copy should be the bounded one.
    QImage img(localPath);
    if (img.isNull()) {
        qWarning() << "Cannot load image for upload:" << localPath;
        emit imageUploadFailed();
        return;
    }
    if (img.width() > MAX_IMAGE_DIMENSION || img.height() > MAX_IMAGE_DIMENSION) {
        img = img.scaled(MAX_IMAGE_DIMENSION, MAX_IMAGE_DIMENSION,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPEG", JPEG_QUALITY);
    buffer.close();
    if (bytes.isEmpty()) {
        emit imageUploadFailed();
        return;
    }

    // Multipart upload to the Files API. The purpose is "ocr" because that
    // is the only purpose the API documents for general file storage; the
    // file id is what conversations reference.
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("image/jpeg"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\"; filename=\"image.jpg\""));
    filePart.setBody(bytes);
    multiPart->append(filePart);

    QHttpPart purposePart;
    purposePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"purpose\""));
    purposePart.setBody("ocr");
    multiPart->append(purposePart);

    QNetworkRequest request(endpoint("/files"));
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization",
                             QString("Bearer %1").arg(apiKey).toUtf8());
    }
    applyTransportSecurity(request);

    noteRequestSent();
    QNetworkReply *reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply);  // deleted with the reply

    connect(reply, &QNetworkReply::finished,
            this, &MistralAPI::onImageUploadFinished);
}

void MistralAPI::onImageUploadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Image upload failed:" << reply->errorString();
        emit imageUploadFailed();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QString fileId = doc.object()["id"].toString();
    if (fileId.isEmpty()) {
        qWarning() << "Image upload returned no file id";
        emit imageUploadFailed();
        return;
    }

    emit imageUploaded(fileId);
}

// ---------------------------------------------------------------- F18 voice input

QString MistralAPI::tempAudioPath() const
{
    // A unique path inside the app's cache directory for the next recording.
    // Ensure the directory exists (SailJail sandbox may not have created it).
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    return cacheDir + "/skrynja-recording-"
            + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav";
}

void MistralAPI::transcribeAudio(const QString &apiKey, const QString &audioPath)
{
    if ((apiKey.isEmpty() && m_keyRequired) || m_baseUrl.isEmpty()) {
        emit transcriptionFailed();
        return;
    }

    QString localPath = audioPath;
    if (localPath.startsWith("file:")) {
        localPath = QUrl(localPath).toLocalFile();
    }

    QFile audioFile(localPath);
    if (!audioFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open audio file for transcription:" << localPath;
        emit transcriptionFailed();
        return;
    }
    const QByteArray audioData = audioFile.readAll();
    audioFile.close();
    if (audioData.isEmpty()) {
        emit transcriptionFailed();
        return;
    }

    // Multipart upload to the Audio Transcriptions API.
    // POST /v1/audio/transcriptions with model=voxtral-mini-latest.
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("audio/wav"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\"; filename=\"recording.wav\""));
    filePart.setBody(audioData);
    multiPart->append(filePart);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant("form-data; name=\"model\""));
    modelPart.setBody("voxtral-mini-latest");
    multiPart->append(modelPart);

    QNetworkRequest request(endpoint("/audio/transcriptions"));
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization",
                             QString("Bearer %1").arg(apiKey).toUtf8());
    }
    applyTransportSecurity(request);

    noteRequestSent();
    QNetworkReply *reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply);  // deleted with the reply

    connect(reply, &QNetworkReply::finished,
            this, &MistralAPI::onTranscriptionFinished);
}

void MistralAPI::onTranscriptionFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Transcription failed:" << reply->errorString()
                   << "body:" << body.left(500);
        emit transcriptionFailed();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    const QString text = doc.object()["text"].toString();
    if (text.isEmpty()) {
        qWarning() << "Transcription returned no text, body:" << body.left(500);
        emit transcriptionFailed();
        return;
    }

    emit transcriptionReady(text);
}

void MistralAPI::cancelRequest()
{
    // Cancelled while the request is still held back: there is no reply to
    // abort, but the UI is busy and the exchange has to be closed anyway.
    if (m_requestTimer->isActive()) {
        m_requestTimer->stop();
        forgetPendingRequest();
        setIsBusy(false);
        emit responseIncomplete();
        emit responseCompleted();
        return;
    }

    if (m_currentReply) {
        // A cancelled request must never come back as a retry.
        forgetPendingRequest();
        m_currentReply->abort();
    }
}

void MistralAPI::clearError()
{
    setError(QString());
}

void MistralAPI::onReadyRead()
{
    if (!m_currentReply)
        return;

    // Restart timeout on activity so long streams are not aborted
    m_timeoutTimer->start();
    processStreamData(m_currentReply->readAll());
}

bool MistralAPI::shouldRetry(int httpStatus) const
{
    return isTransientStatus(httpStatus)
            && !m_pendingBody.isEmpty()
            && !m_streamStarted   // half an answer is already on screen
            && !m_timedOut
            && m_retryCount < MAX_RATE_LIMIT_RETRIES;
}

void MistralAPI::onFinished()
{
    if (!m_currentReply)
        return;

    m_timeoutTimer->stop();

    // Process remaining data
    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray remaining = m_currentReply->readAll();
        if (!remaining.isEmpty()) {
            processStreamData(remaining);
        }
    } else {
        const int status = m_currentReply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (shouldRetry(status)) {
            // An append that may have reached the server must not be sent
            // again blindly: it would duplicate the user's message in the
            // remote history. Reconcile first (D5).
            if (!m_pendingRemoteId.isEmpty() && !m_reconciling) {
                m_currentReply->deleteLater();
                m_currentReply = nullptr;
                startReconcile();
                return;
            }

            const int wait = retryDelayMs(m_currentReply, m_retryCount);
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
            ++m_retryCount;

            // Deliberately still busy and still the same exchange: the empty
            // assistant bubble stays, the user is told to hold on, and nothing
            // downstream has to know the provider said no.
            emit rateLimited(qMax(1, (wait + 500) / 1000),
                             m_retryCount, MAX_RATE_LIMIT_RETRIES);
            queueChatRequest(wait);
            return;
        }
    }

    const bool complete = m_streamDone;

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    forgetPendingRequest();
    setIsBusy(false);

    // A stream that ended without conversation.response.done means the answer
    // was cut off (cancel, network drop): anything already shown is partial.
    if (!complete) {
        emit responseIncomplete();
    }

    emit responseCompleted();
}

void MistralAPI::onError(QNetworkReply::NetworkError error)
{
    if (!m_currentReply)
        return;

    // User cancellation is not an error; timeout gets its own message
    if (error == QNetworkReply::OperationCanceledError) {
        if (m_timedOut) {
            setError(tr("Request timed out. Please check your connection."));
        }
        return;
    }

    const int status = m_currentReply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Nothing to report yet: onFinished() is about to send the same request
    // again, and an error banner flashing between attempts is pure noise.
    if (shouldRetry(status)) {
        return;
    }

    if (status == 429) {
        setError(tr("Rate limit reached. Free tiers allow about one request "
                    "per second - wait a few seconds before sending again."));
        qWarning() << "Rate limited, gave up after" << m_retryCount << "retries";
        return;
    }

    QString errorString = m_currentReply->errorString();
    QByteArray responseData = m_currentReply->readAll();

    // Try to extract a more detailed error message from the JSON body
    if (!responseData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error")) {
                QJsonValue errorValue = obj["error"];
                if (errorValue.isObject()) {
                    QString message = errorValue.toObject()["message"].toString();
                    if (!message.isEmpty()) {
                        errorString = message;
                    }
                } else if (errorValue.isString()) {
                    errorString = errorValue.toString();
                }
            }
        }
    }

    setError(tr("API Error: %1").arg(errorString));
    qWarning() << "API Error:" << errorString;
}

void MistralAPI::setIsBusy(bool busy)
{
    if (m_isBusy != busy) {
        m_isBusy = busy;
        emit isBusyChanged();
    }
}

void MistralAPI::setError(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        emit errorChanged();
    }
}

void MistralAPI::processStreamData(const QByteArray &data)
{
    // Buffer raw bytes: a multi-byte UTF-8 character can be split across
    // network chunks, so decode only complete lines
    m_streamBuffer.append(data);

    int newlinePos;
    while ((newlinePos = m_streamBuffer.indexOf('\n')) != -1) {
        QString line = QString::fromUtf8(m_streamBuffer.left(newlinePos)).trimmed();
        m_streamBuffer.remove(0, newlinePos + 1);

        if (!line.isEmpty()) {
            handleSseLine(line);
        }
    }
}

void MistralAPI::handleSseLine(const QString &line)
{
    // SSE field parsing: "field:value" or "field: value" (leading space after
    // the colon is optional per the SSE spec). The Conversations API sends
    // typed events: an "event: <name>" line names the next "data:" payload,
    // and there is no [DONE] marker.
    const int colon = line.indexOf(':');
    if (colon <= 0) {
        // Comment line (":...") or garbage: ignore.
        return;
    }
    const QString field = line.left(colon);
    QString value = line.mid(colon + 1);
    if (value.startsWith(' ')) {
        value = value.mid(1);
    }

    if (field == "event") {
        m_currentEvent = value.trimmed();
        return;
    }
    if (field != "data") {
        return;
    }

    // A blank line dispatches the event per the SSE spec, but the API does
    // not send blank lines between events; every data line is its own event.
    const QString jsonData = value;
    const QString event = m_currentEvent;
    m_currentEvent.clear();
    parseStreamLine(event, jsonData);
}

void MistralAPI::onTimeout()
{
    if (m_currentReply) {
        m_timedOut = true;
        m_currentReply->abort();
    }
}

// ---------------------------------------------------------------- reconcile

void MistralAPI::startReconcile()
{
    // An append whose fate is unknown (timeout, dropped connection) must not
    // be resent blindly: if it did reach the server, a second copy would land
    // in the remote history. Ask the server what it already has first.
    if (m_pendingRemoteId.isEmpty() || m_reconciling) {
        return;
    }

    m_reconciling = true;
    emit rateLimited(1, m_retryCount + 1, MAX_RATE_LIMIT_RETRIES);

    QNetworkRequest request(endpoint(
                QString("/conversations/%1/messages").arg(m_pendingRemoteId)));
    prepareRequest(request, m_pendingApiKey);

    noteRequestSent();
    m_reconcileReply = m_networkManager->get(request);
    connect(m_reconcileReply, &QNetworkReply::finished,
            this, &MistralAPI::onReconcileFinished);
}

void MistralAPI::onReconcileFinished()
{
    if (!m_reconcileReply) {
        return;
    }

    QNetworkReply *reply = m_reconcileReply;
    m_reconcileReply = nullptr;
    m_reconciling = false;
    reply->deleteLater();

    bool alreadyThere = false;
    if (reply->error() == QNetworkReply::NoError) {
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        alreadyThere = remoteHasUserMessage(doc.object(), m_pendingUserContent);
    } else {
        qWarning() << "Reconcile failed:" << reply->errorString();
    }

    if (alreadyThere) {
        // The server has the turn but never answered it (or the answer was
        // lost): the remote history is intact, so the append can be retried
        // without duplicating anything.
        const int wait = retryDelayMs(nullptr, m_retryCount);
        ++m_retryCount;
        emit rateLimited(qMax(1, (wait + 500) / 1000),
                         m_retryCount, MAX_RATE_LIMIT_RETRIES);
        queueChatRequest(wait);
        return;
    }

    // Either the turn never arrived (safe to resend) or the reconcile itself
    // failed (resending is the only option left, and the stream guard still
    // prevents a mid-answer retry). If the reconcile failed we cannot know,
    // so surface it rather than risk a duplicate.
    if (reply->error() == QNetworkReply::NoError) {
        // Turn not present remotely: a plain retry is safe.
        const int wait = retryDelayMs(nullptr, m_retryCount);
        ++m_retryCount;
        emit rateLimited(qMax(1, (wait + 500) / 1000),
                         m_retryCount, MAX_RATE_LIMIT_RETRIES);
        queueChatRequest(wait);
    } else {
        // Reconcile failed: give up with a clear message. Resending could
        // duplicate the user's message in the remote history.
        setError(tr("Connection lost and the conversation could not be "
                    "verified. Check your connection and try again."));
        setIsBusy(false);
        forgetPendingRequest();
        emit responseIncomplete();
        emit responseCompleted();
    }
}

bool MistralAPI::remoteHasUserMessage(const QJsonObject &messagesResponse,
                                      const QString &userContent)
{
    const QJsonArray messages = messagesResponse["messages"].toArray();
    const QString needle = userContent.trimmed();
    if (needle.isEmpty()) {
        return false;
    }
    for (const QJsonValue &value : messages) {
        const QJsonObject msg = value.toObject();
        if (msg["role"].toString() != "user") {
            continue;
        }
        // content is a string, or a chunk list when the turn carried an
        // image (inline base64 or an uploaded file id).
        const QJsonValue content = msg["content"];
        if (content.isString() && content.toString().trimmed() == needle) {
            return true;
        }
        if (content.isArray()) {
            const QJsonArray chunks = content.toArray();
            for (const QJsonValue &chunk : chunks) {
                const QJsonObject obj = chunk.toObject();
                if (obj["type"].toString() == "text"
                        && obj["text"].toString().trimmed() == needle) {
                    return true;
                }
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------- titles

void MistralAPI::onTitleGenerationFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    const QString targetId = reply->property("targetId").toString();
    bool delivered = false;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        if (doc.isObject()) {
            QJsonObject obj = doc.object();

            // Titling is billed like any other call: report it so the token
            // counters match the invoice.
            if (obj.contains("usage") && obj["usage"].isObject()) {
                QJsonObject usage = obj["usage"].toObject();
                emit sideRequestUsage(usage["prompt_tokens"].toInt(),
                                      usage["completion_tokens"].toInt());
            }

            const QString content = outputsToText(obj["outputs"].toArray()).trimmed();

            QString title;
            QString category;

            // Extract the JSON object even if wrapped in fences or prose
            int start = content.indexOf('{');
            int end = content.lastIndexOf('}');
            if (start >= 0 && end > start) {
                QJsonDocument titleDoc = QJsonDocument::fromJson(
                            content.mid(start, end - start + 1).toUtf8());
                if (titleDoc.isObject()) {
                    title = titleDoc.object()["title"].toString().trimmed();
                    category = titleDoc.object()["category"].toString().trimmed().toLower();
                }
            }

            // Fallback: model ignored the JSON format, use raw content as title
            if (title.isEmpty()) {
                title = content;
                if (title.startsWith("\"") && title.endsWith("\"")) {
                    title = title.mid(1, title.length() - 2);
                }
            }

            if (!Categories::isValid(category)) {
                category = "other";
            }

            if (title.length() > 50) {
                title = title.left(47) + "...";
            }

            if (!title.isEmpty()) {
                delivered = true;
                emit titleGenerated(title, category, targetId);
            }
        }
    } else {
        qWarning() << "Title generation failed:" << reply->errorString();
    }

    if (!delivered) {
        emit titleGenerationFailed(targetId);
    }

    reply->deleteLater();
}

QString MistralAPI::outputsToText(const QJsonArray &outputs)
{
    // MessageOutputEntry content is a string, or a chunk list (thinking +
    // text) for reasoning models: concatenate the text chunks.
    QString text;
    for (const QJsonValue &value : outputs) {
        const QJsonObject entry = value.toObject();
        const QJsonValue content = entry["content"];
        if (content.isString()) {
            text += content.toString();
        } else if (content.isArray()) {
            const QJsonArray chunks = content.toArray();
            for (const QJsonValue &chunk : chunks) {
                const QJsonObject obj = chunk.toObject();
                if (obj["type"].toString() == "text") {
                    text += obj["text"].toString();
                }
            }
        }
    }
    return text;
}

// ---------------------------------------------------------------- catalogue

void MistralAPI::onModelsFetchFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Models fetch failed:" << reply->errorString();
        emit modelsFetchFailed();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit modelsFetchFailed();
        return;
    }

    QJsonArray data = doc.object()["data"].toArray();
    QStringList ids;
    QVariantList models;

    for (const QJsonValue &value : data) {
        QJsonObject modelObj = value.toObject();
        QString id = modelObj["id"].toString();
        QJsonObject caps = modelObj["capabilities"].toObject();

        // Keep only current chat models; dated snapshots add noise, but
        // some new models ship under an undated id with no -latest alias
        static const QRegularExpression datedSuffix("-\\d{4}$");
        if (!caps["completion_chat"].toBool() || id.contains(datedSuffix)) {
            continue;
        }

        if (ids.contains(id)) {
            continue;
        }
        ids.append(id);
        QVariantMap entry;
        entry["id"] = id;
        entry["vision"] = caps["vision"].toBool();
        models.append(entry);
    }

    if (models.isEmpty()) {
        emit modelsFetchFailed();
        return;
    }

    // Sort alphabetically by id
    std::sort(models.begin(), models.end(),
              [](const QVariant &a, const QVariant &b) {
        return a.toMap()["id"].toString() < b.toMap()["id"].toString();
    });

    emit modelsFetched(models);
}

// ---------------------------------------------------------------- SSE events

void MistralAPI::parseStreamLine(const QString &event, const QString &jsonData)
{
    // The Conversations API streams typed events; anything without a data
    // payload we recognise is metadata we do not need (heartbeats, pings).
    if (event == "conversation.response.started") {
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QString id = doc.object()["conversation_id"].toString();
        if (!id.isEmpty()) {
            m_remoteConversationId = id;
            emit conversationCreated(id);
        }
        return;
    }

    if (event == "conversation.response.error") {
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QString message = doc.object()["message"].toString();
        setError(message.isEmpty() ? tr("The API returned an error") : message);
        m_streamDone = true;
        return;
    }

    if (event == "tool.execution.started" || event == "tool.execution.delta") {
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QString name = doc.object()["name"].toString();
        if (!name.isEmpty()) {
            emit toolActivity(name);
        }
        return;
    }

    if (event == "tool.execution.done") {
        // web_search results arrive here as a list of source objects with a
        // "url" field. Collect them for the answer that is being built.
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QJsonObject data = doc.object()["data"].toObject();
        const QJsonArray results = data["output"].toArray();
        for (const QJsonValue &v : results) {
            const QString url = v.toObject()["url"].toString();
            if (!url.isEmpty() && !m_pendingSources.contains(url)) {
                m_pendingSources.append(url);
            }
        }
        return;
    }

    if (event == "message.output.delta") {
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QJsonValue content = doc.object()["content"];
        // Reasoning models send content as a chunk list (thinking + text);
        // only the text chunks belong on screen.
        if (content.isString()) {
            const QString text = content.toString();
            if (!text.isEmpty()) {
                m_streamStarted = true;
                emit streamingResponse(text);
            }
        } else if (content.isArray()) {
            const QJsonArray chunks = content.toArray();
            for (const QJsonValue &chunk : chunks) {
                const QJsonObject obj = chunk.toObject();
                const QString type = obj["type"].toString();
                if (type == "text") {
                    const QString text = obj["text"].toString();
                    if (!text.isEmpty()) {
                        m_streamStarted = true;
                        emit streamingResponse(text);
                    }
                } else if (type == "thinking") {
                    // Buffered, not streamed: the reasoning is shown
                    // collapsed under the answer once it is complete.
                    // ThinkChunk.thinking is a list of text/reference
                    // chunks, not a plain string — flatten it.
                    const QJsonValue thinkingVal = obj["thinking"];
                    if (thinkingVal.isString()) {
                        const QString thinking = thinkingVal.toString();
                        if (!thinking.isEmpty()) {
                            m_pendingThinking += thinking;
                        }
                    } else if (thinkingVal.isArray()) {
                        const QJsonArray thinkingChunks = thinkingVal.toArray();
                        for (const QJsonValue &tc : thinkingChunks) {
                            const QString thinking = tc.toObject()["text"].toString();
                            if (!thinking.isEmpty()) {
                                m_pendingThinking += thinking;
                            }
                        }
                    }
                }
            }
        }
        return;
    }

    if (event == "conversation.response.done") {
        m_streamDone = true;
        // Thinking is emitted first: it belongs to the same answer and the
        // manager attaches it alongside the sources.
        if (!m_pendingThinking.isEmpty()) {
            emit thinkingResponse(m_pendingThinking);
        }
        // Sources are emitted before usage so the manager can attach them to
        // the assistant message before the response is finalised.
        if (!m_pendingSources.isEmpty()) {
            emit sourcesReceived(m_pendingSources);
        }
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QJsonObject usage = doc.object()["usage"].toObject();
        if (!usage.isEmpty()) {
            emit usageReceived(usage["prompt_tokens"].toInt(),
                               usage["completion_tokens"].toInt());
        }
        return;
    }

    if (event == "error") {
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QString message = doc.object()["message"].toString();
        setError(tr("API Error: %1").arg(message.isEmpty()
                                         ? jsonData : message));
        return;
    }

    // Some servers send data-only SSE (no "event:" line, like Chat
    // Completions). The payload self-describes via its "type" field.
    if (event.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        const QJsonObject obj = doc.object();
        const QString type = obj["type"].toString();

        if (type == "message.output.delta") {
            parseStreamLine("message.output.delta", jsonData);
        } else if (type == "conversation.response.started") {
            parseStreamLine("conversation.response.started", jsonData);
        } else if (type == "conversation.response.done") {
            parseStreamLine("conversation.response.done", jsonData);
        } else if (type == "conversation.response.error") {
            parseStreamLine("conversation.response.error", jsonData);
        } else if (type == "tool.execution.started") {
            parseStreamLine("tool.execution.started", jsonData);
        } else if (type == "tool.execution.delta") {
            parseStreamLine("tool.execution.delta", jsonData);
        } else if (type == "tool.execution.done") {
            parseStreamLine("tool.execution.done", jsonData);
        } else if (type == "error") {
            parseStreamLine("error", jsonData);
        }
        // Unknown or untyped: ignore.
        return;
    }

    // An event we do not handle: ignore.
}
