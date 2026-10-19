#ifndef MISTRALCLIENT_H
#define MISTRALCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QJsonArray>
#include <QVariant>
#include <QTimer>

class MistralClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)

public:
    explicit MistralClient(QObject *parent = nullptr);
    ~MistralClient();

    QString apiKey() const;
    void setApiKey(const QString &apiKey);

    QString model() const;
    void setModel(const QString &model);

    bool isLoading() const;
    QString lastError() const;
    QStringList availableModels() const;

    Q_INVOKABLE void sendMessage(const QString &message, const QVariantList &history);
    Q_INVOKABLE void cancelRequest();

signals:
    void apiKeyChanged();
    void modelChanged();
    void isLoadingChanged();
    void lastErrorChanged();
    void messageReceived(const QString &response);
    void availableModelsChanged();

private slots:
    void onReplyFinished();
    void onReplyError(QNetworkReply::NetworkError error);
    void onTimeout();
    void loadSettings();
    void saveSettings();

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    QTimer *m_timeoutTimer;
    QString m_apiKey;
    QString m_model;
    bool m_isLoading;
    QString m_lastError;
    QStringList m_availableModels;
    QSettings *m_settings;
};

#endif // MISTRALCLIENT_H
