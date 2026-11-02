#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTranslator>
#include <QLocale>

#include "mistralapi.h"
#include "conversationmodel.h"
#include "conversationmanager.h"
#include "settingsmanager.h"
#include "audiorecorder.h"
#include "voiceanonymizer.h"
#include "acpclient.h"
#include <QtQml>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>

#ifndef APP_VERSION
#define APP_VERSION "0.9.8"
#endif

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    // Must match OrganizationName/ApplicationName in the [X-Sailjail] section
    // of the desktop file so SailJail whitelists the right data/config dirs.
    app->setOrganizationName("org.skrynja");
    app->setApplicationName("Skrynja");

    QQuickView *view = SailfishApp::createView();

    // Create translator instance
    QTranslator *translator = new QTranslator(app);

    // Create instances of C++ classes
    MistralAPI mistralApi;
    ConversationManager conversationManager;
    SettingsManager settingsManager;
    AudioRecorder audioRecorder;
    VoiceAnonymizer voiceAnonymizer;

    qmlRegisterType<AudioRecorder>("harbour.skrynja", 1, 0, "AudioRecorder");

    // The manager owns the streaming lifecycle so an answer survives the chat
    // page being popped off the stack mid-response.
    conversationManager.bindApi(&mistralApi);

    // Mistral AI is the only provider: the Conversations API exists nowhere
    // else. The endpoint is fixed in MistralAPI's constructor; nothing here
    // reconfigures it.

    // Load initial translation based on settings
    QString language = settingsManager.language();
    QString translationFile = QString("harbour-skrynja-%1").arg(language);
    if (translator->load(translationFile, SailfishApp::pathTo("translations").toLocalFile())) {
        app->installTranslator(translator);
    }

    // Lambda to switch language dynamically. The translator swap alone makes
    // qsTr() re-evaluate, which sends LanguageChange events to every live QML
    // item — no view reload. Reloading the view here crashed: the reload ran
    // inside the ComboBox's onCurrentItemChanged handler while that item was
    // being destroyed (binding loop on TextField width, then segfault).
    auto switchLanguage = [app, translator](const QString &lang) {
        app->removeTranslator(translator);
        QString translationFile = QString("harbour-skrynja-%1").arg(lang);
        if (translator->load(translationFile, SailfishApp::pathTo("translations").toLocalFile())) {
            app->installTranslator(translator);
        }
    };

    // Connect language change signal
    QObject::connect(&settingsManager, &SettingsManager::languageChanged, [&settingsManager, switchLanguage]() {
        switchLanguage(settingsManager.language());
    });

    // Expose objects to QML context
    QQmlContext *context = view->rootContext();
    context->setContextProperty("mistralApi", &mistralApi);
    context->setContextProperty("conversationManager", &conversationManager);
    context->setContextProperty("conversationModel", conversationManager.currentConversation());
    context->setContextProperty("settingsManager", &settingsManager);
    context->setContextProperty("globalAudioRecorder", &audioRecorder);
    context->setContextProperty("voiceAnonymizer", &voiceAnonymizer);

    // Load main QML
    view->setSource(SailfishApp::pathTo("qml/harbour-skrynja.qml"));
    view->show();

    // --- ACP agent (alpha): spawn vibe-acp and run the initialize handshake.
    // All wire frames and internal log events go to stderr as [acp]..., so a
    // terminal start shows the handshake live:
    //   harbour-skrynja 2>&1 | tee /tmp/skrynja-start.log
    AcpClient acpClient;
    QObject::connect(&acpClient, &AcpClient::frameWritten, [](const QByteArray &frame) {
        qWarning() << "[acp][out]" << QString::fromUtf8(frame);
    });
    QObject::connect(&acpClient, &AcpClient::messageReceived, [](const QJsonObject &msg) {
        qWarning() << "[acp][in]" << QJsonDocument(msg).toJson(QJsonDocument::Compact);
    });
    QObject::connect(&acpClient, &AcpClient::haleReceived, [](const QJsonObject &msg) {
        qWarning() << "[acp][hale]" << QJsonDocument(msg).toJson(QJsonDocument::Compact);
    });
    QObject::connect(&acpClient, &AcpClient::lineOverCap, [](qint64 received, qint64 cap) {
        qWarning() << "[acp][L1] line dropped, received" << received << "cap" << cap;
    });
    QObject::connect(&acpClient, &AcpClient::logEvent, [](int code, const QString &detail) {
        qWarning() << "[acp][log" << code << "]" << detail;
    });
    QObject::connect(&acpClient, &AcpClient::started, []() {
        qWarning() << "[acp] agent started";
    });
    QObject::connect(&acpClient, &AcpClient::finished, [](int exitCode, QProcess::ExitStatus status) {
        qWarning() << "[acp] agent finished, exitCode" << exitCode << "status" << int(status);
    });

    const QString agentPath = AcpClient::defaultAgentPath();
    if (QFileInfo::exists(agentPath)) {
        acpClient.start(agentPath, QDir::homePath());
        // initialize handshake: the agent negotiates only protocolVersion 1
        // (measured M0.1/M0.1b on the målmaskine; phone SDK 0.11.0 answers 1).
        QJsonObject clientInfo;
        clientInfo.insert(QStringLiteral("name"), QStringLiteral("Skrynja"));
        clientInfo.insert(QStringLiteral("version"), QString::fromLatin1(APP_VERSION));
        QJsonObject params;
        params.insert(QStringLiteral("protocolVersion"), 1);
        params.insert(QStringLiteral("clientCapabilities"), QJsonObject());
        params.insert(QStringLiteral("clientInfo"), clientInfo);
        const qint64 initId = acpClient.sendRequest(QStringLiteral("initialize"), params);
        qWarning() << "[acp] sent initialize, id" << initId;
    } else {
        qWarning() << "[acp] agent not found at" << agentPath << "- ACP stays off";
    }

    return app->exec();
}
