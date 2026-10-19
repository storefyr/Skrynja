#include "mistralclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>

MistralClient::MistralClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(0)
    , m_timeoutTimer(0)
    , m_isLoading(false)
    , m_model("mistral-small-latest")
{
    m_availableModels << "mistral-small-latest"
                      << "mistral-medium-latest"
                      << "mistral-large-latest"
                      << "ministral-8b-latest"
                      << "ministral-3b-latest";

    m_settings = new QSettings("Harbour", "skrynja", this);
    loadSettings();
}

MistralClient::~MistralClient()
{
    cancelRequest();
}

QString MistralClient::apiKey() const
{
    return m_apiKey;
}

void MistralClient::setApiKey(const QString &apiKey)
{
    if (m_apiKey != apiKey) {
        m_apiKey = apiKey;
        saveSettings();
        emit apiKeyChanged();
    }
}

QString MistralClient::model() const
{
    return m_model;
}

void MistralClient::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        saveSettings();
        emit modelChanged();
    }
}

bool MistralClient::isLoading() const
{
    return m_isLoading;
}

QString MistralClient::lastError() const
{
    return m_lastError;
}

QStringList MistralClient::availableModels() const
{
    return m_availableModels;
}

void MistralClient::loadSettings()
{
    m_apiKey = m_settings->value("apiKey", "").toString();
    m_model = m_settings->value("model", "mistral-small-latest").toString();
    if (!m_availableModels.contains(m_model)) {
        m_model = "mistral-small-latest";
    }
}

void MistralClient::saveSettings()
{
    m_settings->setValue("apiKey", m_apiKey);
    m_settings->setValue("model", m_model);
    m_settings->sync();
}

void MistralClient::sendMessage(const QString &message)
{
    cancelRequest();

    if (message.trimmed().isEmpty()) {
        return;
    }

    if (m_apiKey.isEmpty()) {
        m_lastError = tr("API-nøgle er påkrævet");
        emit lastErrorChanged();
        return;
    }

    m_isLoading = true;
    emit isLoadingChanged();
    m_lastError.clear();
    emit lastErrorChanged();

    QNetworkRequest request(QUrl("https://api.mistral.ai/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = message;

    QJsonObject json;
    json["model"] = m_model;
    json["messages"] = QJsonArray() << userMessage;
    json["temperature"] = 0.7;
    json["max_tokens"] = 2000;

    m_currentReply = m_networkManager->post(request, QJsonDocument(json).toJson(QJsonDocument::Compact));
    connect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
    connect(m_currentReply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(onReplyError(QNetworkReply::NetworkError)));

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &MistralClient::onTimeout);
    m_timeoutTimer->start(60000);
}

void MistralClient::cancelRequest()
{
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = 0;
    }

    if (m_currentReply) {
        m_currentReply->disconnect(this);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = 0;
    }

    if (m_isLoading) {
        m_isLoading = false;
        emit isLoadingChanged();
    }
}

void MistralClient::onTimeout()
{
    m_lastError = tr("Forespørgsel timeout (60s) — tjek din internetforbindelse");
    emit lastErrorChanged();
    cancelRequest();
}

void MistralClient::onReplyFinished()
{
    if (!m_currentReply) {
        return;
    }

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = 0;
    }

    QNetworkReply *reply = m_currentReply;
    m_currentReply = 0;
    reply->disconnect(this);

    const QByteArray response = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || (httpStatus != 0 && httpStatus != 200)) {
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject errorObj = doc.object().value("error").toObject();
            if (errorObj.contains("message")) {
                m_lastError = errorObj.value("message").toString();
                emit lastErrorChanged();
                reply->deleteLater();
                m_isLoading = false;
                emit isLoadingChanged();
                return;
            }
        }
        m_lastError = reply->errorString();
        emit lastErrorChanged();
        reply->deleteLater();
        m_isLoading = false;
        emit isLoadingChanged();
        return;
    }

    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        m_lastError = tr("Ugyldigt svar fra server");
        emit lastErrorChanged();
        m_isLoading = false;
        emit isLoadingChanged();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray choices = root.value("choices").toArray();
    if (choices.isEmpty()) {
        m_lastError = tr("Intet svar modtaget fra API");
        emit lastErrorChanged();
        m_isLoading = false;
        emit isLoadingChanged();
        return;
    }

    QString content = choices.first().toObject()
            .value("message").toObject()
            .value("content").toString();

    m_isLoading = false;
    emit isLoadingChanged();

    if (content.isEmpty()) {
        m_lastError = tr("Tomt svar fra API");
        emit lastErrorChanged();
    } else {
        emit messageReceived(content);
    }
}

void MistralClient::onReplyError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)
    if (!m_currentReply) {
        return;
    }

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = 0;
    }

    QNetworkReply *reply = m_currentReply;
    m_currentReply = 0;
    reply->disconnect(this);

    const QByteArray response = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isObject()) {
        QJsonObject errorObj = doc.object().value("error").toObject();
        if (errorObj.contains("message")) {
            m_lastError = errorObj.value("message").toString();
            emit lastErrorChanged();
            m_isLoading = false;
            emit isLoadingChanged();
            return;
        }
    }

    m_lastError = tr("Netværksfejl: %1").arg(error == QNetworkReply::TimeoutError
                                           ? tr("timeout") : QStringLiteral("forbindelse fejlede"));
    emit lastErrorChanged();
    m_isLoading = false;
    emit isLoadingChanged();
}
