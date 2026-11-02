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
    // Set in code (the desktop file has no [X-Sailjail] section anymore —
    // punkt 122, lukket [B] storefyr 2026-10-31, appen kører ikke indespærret).
    app->setOrganizationName("org.skrynja");
    app->setApplicationName("Skrynja");

    QQuickView *view = SailfishApp::createView();

    // Create translator instance
    QTranslator *translator = new QTranslator(app);

    // Create instances of C++ classes
    // ACP clean slate (storefyr, 2026-11-02): no MistralAPI / Conversations
    // API in this build. The only backend is the ACP agent over stdio.
    ConversationManager conversationManager;
    SettingsManager settingsManager;
    // Declared here (before setSource) so the QML context property exists
    // when the QML loads — Qt 5.6 evaluates bindings at load time.
    AcpClient acpClient;
    AudioRecorder audioRecorder;
    VoiceAnonymizer voiceAnonymizer;

    qmlRegisterType<AudioRecorder>("harbour.skrynja", 1, 0, "AudioRecorder");

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
    context->setContextProperty("conversationManager", &conversationManager);
    context->setContextProperty("conversationModel", conversationManager.currentConversation());
    context->setContextProperty("settingsManager", &settingsManager);
    context->setContextProperty("globalAudioRecorder", &audioRecorder);
    context->setContextProperty("voiceAnonymizer", &voiceAnonymizer);
    context->setContextProperty("acpClient", &acpClient);

    // Load main QML
    view->setSource(SailfishApp::pathTo("qml/harbour-skrynja.qml"));
    view->show();

    // --- ACP agent (alpha, clean slate): spawn vibe-acp, run the
    // initialize handshake, then open one session and route the agent's
    // session/update chunks into the current conversation model.
    // All wire frames and internal log events go to stderr as [acp]..., so a
    // terminal start shows the handshake live:
    //   harbour-skrynja 2>&1 | tee /tmp/skrynja-start.log

    // Streaming state for agent_message_chunk accumulation.
    QString streamBuf;
    bool assistantOpen = false;

    QObject::connect(&acpClient, &AcpClient::frameWritten, [](const QByteArray &frame) {
        qWarning() << "[acp][out]" << QString::fromUtf8(frame);
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
    QObject::connect(&acpClient, &AcpClient::promptSent, [&](qint64, const QString &) {
        streamBuf.clear();
        assistantOpen = false;
    });
    QObject::connect(&acpClient, &AcpClient::messageReceived, [&](const QJsonObject &msg) {
        qWarning() << "[acp][in]" << QJsonDocument(msg).toJson(QJsonDocument::Compact);
        ConversationModel *model = conversationManager.currentConversation();
        if (msg.contains(QStringLiteral("id"))) {
            const qint64 id = qint64(msg.value(QStringLiteral("id")).toDouble());
            if (msg.contains(QStringLiteral("error"))) {
                qWarning() << "[acp][err] id" << id << msg.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt()
                           << msg.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
                return;
            }
            const QJsonObject result = msg.value(QStringLiteral("result")).toObject();
            if (id == 1) {
                // initialize answered: open the session (measured on the phone
                // 2026-11-02: session/new needs no extra auth in accept-edits).
                const qint64 sid = acpClient.sendSessionNew(QDir::homePath());
                qWarning() << "[acp] sent session/new, id" << sid;
            } else if (id == 2) {
                acpClient.setSessionId(result.value(QStringLiteral("sessionId")).toString());
                qWarning() << "[acp] session ready:" << acpClient.sessionId();
            } else {
                qWarning() << "[acp] result id" << id << "stopReason"
                           << result.value(QStringLiteral("stopReason")).toString();
            }
            return;
        }
        const QString method = msg.value(QStringLiteral("method")).toString();
        if (method == QStringLiteral("session/update")) {
            const QJsonObject upd = msg.value(QStringLiteral("params")).toObject()
                    .value(QStringLiteral("update")).toObject();
            const QString kind = upd.value(QStringLiteral("sessionUpdate")).toString();
            if (kind == QStringLiteral("agent_message_chunk")) {
                const QString text = upd.value(QStringLiteral("content")).toObject()
                        .value(QStringLiteral("text")).toString();
                if (!text.isEmpty() && model) {
                    if (!assistantOpen) {
                        model->addAssistantMessage(QString());
                        assistantOpen = true;
                    }
                    streamBuf += text;
                    model->updateLastAssistantMessage(streamBuf);
                }
            } else {
                qWarning() << "[acp][update]" << kind;
            }
        } else if (method == QStringLiteral("session/request_permission")
                   || method == QStringLiteral("request_permission")) {
            // Målt 2026-11-02 på telefonen: anmodningen kommer namespaced
            // ("session/request_permission") med id 0/1 og params-nøglerne
            // options/sessionId/toolCall; options målt til allow_once,
            // allow_session_exact, allow_permanent_exact, reject_once.
            // Alpha: ingen dialog endnu — log og svar "cancelled" med det
            // samme, så et prompt i ask-mode ikke hænger.
            const QJsonObject pr = msg.value(QStringLiteral("params")).toObject();
            const QJsonObject tc = pr.value(QStringLiteral("toolCall")).toObject();
            QStringList opts;
            const QJsonArray arr = pr.value(QStringLiteral("options")).toArray();
            for (const QJsonValue &v : arr)
                opts << v.toObject().value(QStringLiteral("optionId")).toString();
            const qint64 reqId = qint64(msg.value(QStringLiteral("id")).toDouble());
            qWarning() << "[acp][permission] id" << reqId << "tool" << tc.value(QStringLiteral("title")).toString()
                       << "options" << opts << "-> auto-cancelled (alpha, ingen dialog endnu)";
            QJsonObject outcome;
            outcome.insert(QStringLiteral("outcome"), QStringLiteral("cancelled"));
            QJsonObject result;
            result.insert(QStringLiteral("outcome"), outcome);
            acpClient.sendResult(reqId, result);
        }
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
