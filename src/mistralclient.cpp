#include "mistralclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QCoreApplication>

MistralClient::MistralClient(QObject *parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_currentReply = nullptr;
    m_timeoutTimer = nullptr;
    m_isLoading = false;
    m_model = "mistral-tiny";
    
    // Available models
    m_availableModels = {
        "mistral-tiny",
        "mistral-small", 
        "mistral-medium",
        "mistral-large"
    };
    
    // Load settings - use parent for consistent QSettings storage
    m_settings = new QSettings(this);
    loadSettings();
    
    // Emit availableModelsChanged after initialization
    emit availableModelsChanged();
}

MistralClient::~MistralClient()
{
    // Ensure all requests and timers are cleaned up properly
    cancelRequest();
    
    // QObject children will be cleaned up automatically by Qt's parent-child system
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
    m_model = m_settings->value("model", "mistral-tiny").toString();
}

void MistralClient::saveSettings()
{
    m_settings->setValue("apiKey", m_apiKey);
    m_settings->setValue("model", m_model);
    m_settings->sync();
}

void MistralClient::cancelRequest()
{
    // Clean up timeout timer first
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }
    
    if (m_currentReply) {
        // Disconnect signals before aborting
        disconnect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
        disconnect(m_currentReply, &QNetworkReply::errorOccurred, this, &MistralClient::onReplyError);
        
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    if (m_isLoading) {
        m_isLoading = false;
        emit isLoadingChanged();
    }
}

void MistralClient::sendMessage(const QString &message, const QVariantList &history)
{
    // Cancel any existing request
    cancelRequest();
    
    if (m_apiKey.isEmpty()) {
        m_lastError = "API nøgle er påkrævet";
        emit lastErrorChanged();
        return;
    }

    m_isLoading = true;
    emit isLoadingChanged();
    m_lastError.clear();
    emit lastErrorChanged();

    QUrl url("https://api.mistral.ai/v1/chat/completions");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    request.setRawHeader("User-Agent", "Skrynja/1.0 (Sailfish OS)");

    QJsonObject json;
    json["model"] = m_model;
    
    QJsonArray messagesArray;
    
    // Add history - both user and assistant messages with type safety
    for (int i = 0; i < history.size(); i++) {
        if (history[i].type() == QVariant::Map) {
            QVariantMap histItem = history[i].toMap();
            
            if (histItem.contains("role") && 
                histItem["role"].type() == QVariant::String &&
                histItem.contains("content") && 
                histItem["content"].type() == QVariant::String) {
                
                QJsonObject histObj;
                histObj["role"] = histItem["role"].toString();
                histObj["content"] = histItem["content"].toString();
                messagesArray.append(histObj);
            }
        }
    }
    
    // Add current user message
    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = message;
    messagesArray.append(userMessage);
    
    json["messages"] = messagesArray;
    json["temperature"] = 0.7;
    json["max_tokens"] = 2000;
    json["stream"] = false;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    // Debug log request (without API key)
    qDebug() << "Sending request to:" << url.toString();
    qDebug() << "Request data:" << QString(data).left(200) << "...";

    m_currentReply = m_networkManager->post(request, data);
    
    // Use new-style signal-slot connections
    connect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &MistralClient::onReplyError);
    
    // Set up timeout timer (60 seconds)
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &MistralClient::onTimeout);
    m_timeoutTimer->start(60000);
}

void MistralClient::onTimeout()
{
    if (m_currentReply) {
        m_lastError = "Forespørgsel timeout (60s) - tjek din internetforbindelse";
        emit lastErrorChanged();
        cleanupRequest();
    }
}

void MistralClient::onReplyFinished()
{
    // Clean up timeout timer
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }
    
    if (!m_currentReply) return;

    // Disconnect signals to prevent double handling
    disconnect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
    disconnect(m_currentReply, &QNetworkReply::errorOccurred, this, &MistralClient::onReplyError);

    // Read response ONCE and store it
    QByteArray response = m_currentReply->readAll();
    
    // Check HTTP status code for non-2xx responses
    int httpStatus = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus != 200 && httpStatus != 0) {
        qDebug() << "HTTP" << httpStatus << "response:" << QString(response).left(200);
    }

    if (m_currentReply->error() != QNetworkReply::NoError) {
        m_lastError = m_currentReply->errorString();
        emit lastErrorChanged();
        cleanupRequest();
        return;
    }

    // Debug log response (truncated)
    qDebug() << "API response received:" << QString(response).left(200) << "...";
    
    QJsonDocument doc = QJsonDocument::fromJson(response);
    
    if (!doc.isObject()) {
        m_lastError = QString("Ugyldigt JSON svar: %1").arg(QString(response).left(200));
        emit lastErrorChanged();
        cleanupRequest();
        return;
    }

    QJsonObject root = doc.object();
    
    // Check for API errors
    if (root.contains("error") && root["error"].isObject()) {
        QJsonObject error = root["error"].toObject();
        if (error.contains("message")) {
            m_lastError = error["message"].toString();
        } else if (error.contains("type")) {
            m_lastError = QString("API fejl: %1").arg(error["type"].toString());
        } else {
            m_lastError = "Ukendt API fejl";
        }
        emit lastErrorChanged();
        cleanupRequest();
        return;
    }
    
    // Check for choices (standard response)
    if (root.contains("choices") && root["choices"].isArray()) {
        QJsonArray choices = root["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices.first().toObject();
            if (firstChoice.contains("message") && firstChoice["message"].isObject()) {
                QJsonObject message = firstChoice["message"].toObject();
                if (message.contains("content") && message["content"].isString()) {
                    QString content = message["content"].toString();
                    emit messageReceived(content);
                }
            }
        }
    } else if (root.contains("message") && root["message"].isObject()) {
        // Handle alternative response format
        QJsonObject message = root["message"].toObject();
        if (message.contains("content") && message["content"].isString()) {
            QString content = message["content"].toString();
            emit messageReceived(content);
        }
    } else {
        m_lastError = QString("Ukendt svar format: %1").arg(QString(response).left(200));
        emit lastErrorChanged();
    }

    cleanupRequest();
}

void MistralClient::onReplyError(QNetworkReply::NetworkError error)
{
    // Clean up timeout timer
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }
    
    if (!m_currentReply) return;
    
    // Disconnect signals to prevent double handling
    disconnect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
    disconnect(m_currentReply, &QNetworkReply::errorOccurred, this, &MistralClient::onReplyError);
    
    // Check HTTP status code for more detailed errors
    int httpStatus = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    // Add more detailed error information
    if (error == QNetworkReply::TimeoutError) {
        m_lastError = "Forespørgsel timeout - tjek din internetforbindelse";
    } else if (error == QNetworkReply::ConnectionRefusedError) {
        m_lastError = "Kunne ikke forbinde til server - tjek din internetforbindelse";
    } else if (error == QNetworkReply::SslHandshakeFailedError) {
        m_lastError = "SSL fejl - tjek din internetforbindelse eller servercertifikat";
    } else if (error == QNetworkReply::RemoteHostClosedError) {
        m_lastError = "Forbindelsen blev lukket af serveren";
    } else if (httpStatus >= 400) {
        m_lastError = QString("Server fejl %1: %2").arg(httpStatus).arg(m_currentReply->errorString());
    } else {
        m_lastError = QString("Netværksfejl (%1): %2").arg(error).arg(m_currentReply->errorString());
    }
    
    emit lastErrorChanged();
    cleanupRequest();
}

void MistralClient::cleanupRequest()
{
    // Clean up timeout timer
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }
    
    if (m_currentReply) {
        // Disconnect signals before aborting
        disconnect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
        disconnect(m_currentReply, &QNetworkReply::errorOccurred, this, &MistralClient::onReplyError);
        
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    if (m_isLoading) {
        m_isLoading = false;
        emit isLoadingChanged();
    }
}
