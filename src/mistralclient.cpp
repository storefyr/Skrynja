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
    m_isLoading = false;
    m_model = "mistral-tiny";
    
    // Available models - hardcoded da vi ikke bruger dynamisk API endpoint
    m_availableModels = {
        "mistral-tiny",
        "mistral-small", 
        "mistral-medium",
        "mistral-large"
    };
    
    // Load settings - use fixed organization for Sailfish OS compatibility
    m_settings = new QSettings("skrynja", "settings", this);
    loadSettings();
    
    // Set timeout for network requests
    m_networkManager->setTransferTimeout(60000); // 60 seconds
}

MistralClient::~MistralClient()
{
    if (m_currentReply) {
        m_currentReply->deleteLater();
    }
    m_networkManager->deleteLater();
    m_settings->deleteLater();
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





QStringList MistralClient::availableModels() const { return m_availableModels; }
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
    if (m_currentReply) {
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
    request.setTransferTimeout(60000); // 60 seconds timeout

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
    json["temperature"] = qBound(0.0, 0.7, 1.0); // Clamp to valid range
    json["max_tokens"] = qBound(1, 2000, 4096); // Clamp to valid range
    json["stream"] = false; // We handle streaming manually for better control

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    m_currentReply = m_networkManager->post(request, data);
    connect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
    connect(m_currentReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onReplyError(QNetworkReply::NetworkError)));
    // Note: readyRead connection fjernet - vi bruger ikke streaming endnu
}



void MistralClient::onReplyFinished()
{
    if (!m_currentReply) return;

    if (m_currentReply->error() != QNetworkReply::NoError) {
        m_lastError = m_currentReply->errorString();
        emit lastErrorChanged();
        cleanupRequest();
        return;
    }

    QByteArray response = m_currentReply->readAll();
    
    
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
                if (message.contains("content")) {
                    QString content = message["content"].toString();
                    emit messageReceived(content);
                }
            }
        }
    } else if (root.contains("message") && root["message"].isObject()) {
        // Handle alternative response format
        QJsonObject message = root["message"].toObject();
        if (message.contains("content")) {
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
    if (!m_currentReply) return;
    
    m_lastError = m_currentReply->errorString();
    
    // Add more detailed error information
    if (error == QNetworkReply::TimeoutError) {
        m_lastError = "Forespørgsel timeout - tjek din internetforbindelse";
    } else if (error == QNetworkReply::ConnectionRefusedError) {
        m_lastError = "Kunne ikke forbinde til server - tjek din internetforbindelse";
    } else if (error == QNetworkReply::SslHandshakeFailedError) {
        m_lastError = "SSL fejl - tjek din internetforbindelse eller servercertifikat";
    }
    
    emit lastErrorChanged();
    cleanupRequest();
}

void MistralClient::cleanupRequest()
{
    if (m_currentReply) {
        // Fjern connections FØR sletning for at undgå race conditions
        disconnect(m_currentReply, &QNetworkReply::finished, this, &MistralClient::onReplyFinished);
        disconnect(m_currentReply, SIGNAL(error(QNetworkReply::NetworkError)), 
                   this, SLOT(onReplyError(QNetworkReply::NetworkError)));
        
        // Abort requesten med det samme for at stoppe network operation
        m_currentReply->abort();
        
        // Gem pointer og nulstil med det samme
        QNetworkReply *oldReply = m_currentReply;
        m_currentReply = nullptr;
        
        oldReply->deleteLater();  // Slet den gamle asynkront
    }
    
    if (m_isLoading) {
        m_isLoading = false;
        emit isLoadingChanged();
    }
}
