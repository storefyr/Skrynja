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
#include <QFile>
#include <QSettings>
#include <unistd.h>
#include <QProcess>
#include <QTcpSocket>

#ifndef APP_VERSION
#define APP_VERSION "0.9.8"
#endif

// Alpha-diagnostik: spejl alle Qt-loglinjer til fil, så grid-start (hvor
// invoker ikke sender stderr til en terminal) kan inspiceres i
// /tmp/skrynja-app.log. Målt behov 2026-11-02 (runde 6f).
static QFile g_logFile(QStringLiteral("/tmp/skrynja-app.log"));

int main(int argc, char *argv[])
{
    g_logFile.open(QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg) {
        if (g_logFile.isOpen()) {
            g_logFile.write(msg.toUtf8());
            g_logFile.write("\n");
            g_logFile.flush();
        }
        fprintf(stderr, "%s\n", msg.toUtf8().constData());
        fflush(stderr);
    });
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
    // A2: sessionId gemmes lokalt, så app-genstart kan session/load (loadSession
    // målt true 2026-11-02); ved SessionNotFound falder vi tilbage til session/new.
    QSettings acpSettings(QStringLiteral("org.skrynja"), QStringLiteral("Skrynja"));
    qint64 loadId = -1, newId = -1;

    QObject::connect(&acpClient, &AcpClient::messageReceived, [&](const QJsonObject &msg) {
        qWarning() << "[acp][in]" << QJsonDocument(msg).toJson(QJsonDocument::Compact);
        ConversationModel *model = conversationManager.currentConversation();
        // Svar har id men aldrig method; requests/notifications har method
        // (m\u00e5lt: session/request_permission kommer med B\u00d3DE id 0/1 OG method),
        // s\u00e5 id-grenen skal ekskludere method, ellers sluques anmodningen og
        // promptet h\u00e6nger i ask-mode.
        if (msg.contains(QStringLiteral("id")) && !msg.contains(QStringLiteral("method"))) {
            const qint64 id = qint64(msg.value(QStringLiteral("id")).toDouble());
            if (msg.contains(QStringLiteral("error"))) {
                const QJsonObject err = msg.value(QStringLiteral("error")).toObject();
                qWarning() << "[acp][err] id" << id << err.value(QStringLiteral("code")).toInt()
                           << err.value(QStringLiteral("message")).toString();
                acpClient.setLastError(QStringLiteral("%1: %2").arg(err.value(QStringLiteral("code")).toInt())
                                      .arg(err.value(QStringLiteral("message")).toString()));
                if (id == loadId) {
                    // A2 fallback: session/load fejlede (f.eks. SessionNotFound) -> nyt session
                    newId = acpClient.sendSessionNew(QDir::homePath());
                    qWarning() << "[acp] session/load failed, sent session/new id" << newId;
                }
                return;
            }
            const QJsonObject result = msg.value(QStringLiteral("result")).toObject();
            // configOptions kommer i svar på session/new, session/load og
            // session/set_config_option og erstatter hele listen (målt 2026-11-02).
            if (result.contains(QStringLiteral("configOptions")))
                acpClient.setConfigOptions(result.value(QStringLiteral("configOptions")).toArray().toVariantList());
            if (id == 1) {
                // initialize answered: prøv at genåbne gemt session, ellers nyt.
                const QString saved = acpSettings.value(QStringLiteral("acp/sessionId")).toString();
                if (!saved.isEmpty()) {
                    loadId = acpClient.sendSessionLoad(saved, QDir::homePath());
                    qWarning() << "[acp] sent session/load id" << loadId << "sid" << saved;
                } else {
                    newId = acpClient.sendSessionNew(QDir::homePath());
                    qWarning() << "[acp] sent session/new, id" << newId;
                }
            } else if (id == loadId || id == newId) {
                const QString sid = result.value(QStringLiteral("sessionId")).toString();
                acpClient.setSessionId(sid);
                acpSettings.setValue(QStringLiteral("acp/sessionId"), sid);
                qWarning() << "[acp] session ready:" << sid;
            } else {
                qWarning() << "[acp] result id" << id << "stopReason"
                           << result.value(QStringLiteral("stopReason")).toString();
                acpClient.setActivity(QString());
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
            } else if (kind == QStringLiteral("tool_call") || kind == QStringLiteral("tool_call_update")) {
                const QString title = upd.value(QStringLiteral("title")).toString();
                const QString status = upd.value(QStringLiteral("status")).toString();
                acpClient.setActivity(status.isEmpty() ? title : title + QStringLiteral(" (") + status + QStringLiteral(")"));
                qWarning() << "[acp][update]" << kind << title << status;
            } else if (kind == QStringLiteral("agent_thought_chunk")) {
                acpClient.setActivity(QStringLiteral("tænker..."));
                qWarning() << "[acp][update]" << kind;
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
                       << "options" << opts << "-> vist i UI";
            // Vis i dialog i stedet for auto-cancel; svaret går via
            // acpClient.answerPermission(optionId) / dismissPermission() fra QML.
            QVariantMap pm;
            pm.insert(QStringLiteral("id"), double(reqId));
            pm.insert(QStringLiteral("title"), tc.value(QStringLiteral("title")).toString());
            QVariantList options;
            for (const QJsonValue &v : arr) {
                QVariantMap o;
                o.insert(QStringLiteral("optionId"), v.toObject().value(QStringLiteral("optionId")).toString());
                o.insert(QStringLiteral("name"), v.toObject().value(QStringLiteral("name")).toString());
                options.append(o);
            }
            pm.insert(QStringLiteral("options"), options);
            acpClient.setPendingPermission(pm);
        }
    });

    const QString agentPath = AcpClient::defaultAgentPath();
    // Diagnostik (runde 6f): grid-start så "agent not found", terminal-start ikke.
    qWarning() << "[acp][diag] uid" << getuid() << "home" << QDir::homePath()
               << "cwd" << QDir::currentPath()
               << "exists" << QFileInfo::exists(agentPath)
               << "isSymLink" << QFileInfo(agentPath).isSymLink()
               << "canonical" << QFileInfo(agentPath).canonicalFilePath()
               << "dotlocal_exists" << QFileInfo(QStringLiteral("/home/defaultuser/.local")).exists()
               << "bin_list" << QDir(QStringLiteral("/home/defaultuser/.local/bin")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
               << "home_list" << QDir(QStringLiteral("/home/defaultuser")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).mid(0, 12);
    QFile probe(agentPath);
    const bool opened = probe.open(QIODevice::ReadOnly);
    qWarning() << "[acp][diag] open_test" << opened << probe.errorString();
    // a) exec fra den monterede mappe: start en kopieret busybox derfra
    {
        QProcess t;
        t.start(QStringLiteral("/home/defaultuser/.local/share/harbour-skrynja/bin/busybox"),
                QStringList{QStringLiteral("echo"), QStringLiteral("jail-exec-ok")});
        const bool ok = t.waitForFinished(5000);
        qWarning() << "[acp][diag] exec_from_mounted" << ok << t.exitCode() << t.readAllStandardOutput().trimmed() << t.errorString();
    }
    // b) netværk inde fra jailen: TCP til 1.1.1.1:443
    {
        QTcpSocket sock;
        sock.connectToHost(QStringLiteral("1.1.1.1"), 443);
        const bool ok = sock.waitForConnected(5000);
        qWarning() << "[acp][diag] net_1.1.1.1:443" << ok << sock.state() << sock.errorString();
    }
    // c) projektets rodmappe synlig?
    qWarning() << "[acp][diag] projectroot" << QFileInfo(QStringLiteral("/home/defaultuser/Skrynja")).exists()
               << QDir(QStringLiteral("/home/defaultuser/Skrynja")).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).count();
    // d) monteret mappe skrivbar?
    {
        QFile w(QStringLiteral("/home/defaultuser/.local/share/harbour-skrynja/diag-write-test"));
        const bool ok = w.open(QIODevice::WriteOnly);
        if (ok) { w.write("x"); w.close(); QFile::remove(QStringLiteral("/home/defaultuser/.local/share/harbour-skrynja/diag-write-test")); }
        qWarning() << "[acp][diag] mounted_writable" << ok << w.errorString();
    }
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
