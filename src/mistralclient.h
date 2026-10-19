#ifndef MISTRALCLIENT_H
#define MISTRALCLIENT_H

#include <QObject>
#include <QNetworkReply>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QTimer;
class QSettings;

class MistralClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)

public:
    explicit MistralClient(QObject *parent = 0);
    ~MistralClient();

    QString apiKey() const;
    void setApiKey(const QString &apiKey);

    QString model() const;
    void setModel(const QString &model);

    bool isLoading() const;
    QString lastError() const;
    QStringList availableModels() const;

    Q_INVOKABLE void sendMessage(const QString &message);

public slots:
    void cancelRequest();

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

private:
    void loadSettings();
    void saveSettings();

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    QTimer *m_timeoutTimer;
    QSettings *m_settings;
    QString m_apiKey;
    QString m_model;
    bool m_isLoading;
    QString m_lastError;
    QStringList m_availableModels;
};

#endif // MISTRALCLIENT_H
