#include <QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>
#include <QImage>
#include <QImageWriter>

#include "categories.h"
#include "conversationmodel.h"
#include "conversationmanager.h"
#include "mistralapi.h"
#include "providers.h"
#include "securestore.h"
#include "settingsmanager.h"

class TestConversationModel : public QObject
{
    Q_OBJECT

private slots:
    void addMessages()
    {
        ConversationModel model;
        QCOMPARE(model.rowCount(), 0);

        model.addUserMessage("hello");
        model.addAssistantMessage("hi there");
        QCOMPARE(model.rowCount(), 2);

        QJsonArray json = model.toJsonArray();
        QCOMPARE(json.at(0).toObject()["role"].toString(), QString("user"));
        QCOMPARE(json.at(0).toObject()["content"].toString(), QString("hello"));
        QCOMPARE(json.at(0).toObject()["pinned"].toBool(), false);
        QCOMPARE(json.at(1).toObject()["role"].toString(), QString("assistant"));
    }

    void updateLastAssistant()
    {
        ConversationModel model;
        model.addUserMessage("question");
        // No assistant message yet: update creates one
        model.updateLastAssistantMessage("partial");
        QCOMPARE(model.rowCount(), 2);
        model.updateLastAssistantMessage("partial answer");
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.getLastAssistantMessage(), QString("partial answer"));
    }

    void removeLastMessageIfEmpty()
    {
        ConversationModel model;
        model.addUserMessage("question");
        model.addAssistantMessage("");
        QCOMPARE(model.rowCount(), 2);
        model.removeLastMessageIfEmpty();
        QCOMPARE(model.rowCount(), 1);
        // Non-empty assistant message is kept
        model.addAssistantMessage("answer");
        model.removeLastMessageIfEmpty();
        QCOMPARE(model.rowCount(), 2);
    }

    void removeLastAssistantMessage()
    {
        ConversationModel model;
        model.addUserMessage("question");
        model.addAssistantMessage("answer");
        model.removeLastAssistantMessage();
        QCOMPARE(model.rowCount(), 1);
        // No-op when the last message is from the user
        model.removeLastAssistantMessage();
        QCOMPARE(model.rowCount(), 1);
    }

    void truncateFrom()
    {
        ConversationModel model;
        model.addUserMessage("one");
        model.addAssistantMessage("two");
        model.addUserMessage("three");
        model.addAssistantMessage("four");

        model.truncateFrom(10);  // out of range: no-op
        QCOMPARE(model.rowCount(), 4);
        model.truncateFrom(-1);
        QCOMPARE(model.rowCount(), 4);

        model.truncateFrom(2);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.getLastAssistantMessage(), QString("two"));
    }

    void togglePinned()
    {
        ConversationModel model;
        model.addUserMessage("pin me");
        model.togglePinned(0);
        QCOMPARE(model.toJsonArray().at(0).toObject()["pinned"].toBool(), true);
        model.togglePinned(0);
        QCOMPARE(model.toJsonArray().at(0).toObject()["pinned"].toBool(), false);
        model.togglePinned(42);  // out of range: no crash
    }

    void imagePathRoundTrip()
    {
        ConversationModel model;
        model.addUserMessage("look at this", "/tmp/photo.jpg");
        QCOMPARE(model.toJsonArray().at(0).toObject()["imagePath"].toString(),
                 QString("/tmp/photo.jpg"));
        QCOMPARE(model.getLastUserImagePath(), QString("/tmp/photo.jpg"));
    }

    void lastUserMessage()
    {
        ConversationModel model;
        QCOMPARE(model.getLastUserMessage(), QString());
        model.addUserMessage("first");
        model.addAssistantMessage("answer");
        model.addUserMessage("second");
        QCOMPARE(model.getLastUserMessage(), QString("second"));
    }

    void lastAssistantSkipsEmpty()
    {
        ConversationModel model;
        model.addUserMessage("q1");
        model.addAssistantMessage("first answer");
        model.addUserMessage("q2");
        model.addAssistantMessage("");
        QCOMPARE(model.getLastAssistantMessage(), QString("first answer"));
    }
};

class TestMistralAPI : public QObject
{
    Q_OBJECT

private slots:
    // The Conversations API streams typed events: "event: <name>" names the
    // next "data:" payload. A string content lands on screen as a delta.
    void streamingDelta()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));

        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":\"Hello\"}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Hello"));
    }

    // Reasoning models send content as a chunk list: only text chunks are
    // shown, thinking chunks are dropped.
    void reasoningChunksKeepOnlyText()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));

        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":[{\"type\":\"thinking\",\"thinking\":\"hmm\"},"
                              "{\"type\":\"text\",\"text\":\"answer\"}]}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("answer"));
    }

    // The done event carries usage and marks the stream complete.
    void doneEventReportsUsage()
    {
        MistralAPI api;
        QSignalSpy contentSpy(&api, SIGNAL(streamingResponse(QString)));
        QSignalSpy usageSpy(&api, SIGNAL(usageReceived(int,int)));

        api.processStreamData("event: conversation.response.done\n"
                              "data: {\"usage\":{\"prompt_tokens\":25,\"completion_tokens\":89}}\n");
        QCOMPARE(contentSpy.count(), 0);
        QCOMPARE(usageSpy.count(), 1);
        QCOMPARE(usageSpy.at(0).at(0).toInt(), 25);
        QCOMPARE(usageSpy.at(0).at(1).toInt(), 89);
        QVERIFY(api.m_streamDone);
    }

    // There is no [DONE] marker on this API: a stray one is ignored.
    void doneMarkerIgnored()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));
        api.processStreamData("data: [DONE]\n");
        QCOMPARE(spy.count(), 0);
        QVERIFY(!api.m_streamDone);
    }

    // Creating a conversation reports its id so the caller can persist it.
    void conversationCreatedEmitsId()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(conversationCreated(QString)));

        api.processStreamData("event: conversation.created\n"
                              "data: {\"conversation_id\":\"abc-123\"}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("abc-123"));
        QCOMPARE(api.m_remoteConversationId, QString("abc-123"));
    }

    // A server-side tool starting is surfaced so the UI can show activity.
    void toolExecutionAnnouncesName()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(toolActivity(QString)));

        api.processStreamData("event: tool.execution.started\n"
                              "data: {\"name\":\"web_search\"}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("web_search"));
    }

    // A stream event carrying an error message sets the error text.
    void errorEventSetsError()
    {
        MistralAPI api;
        api.processStreamData("event: error\n"
                              "data: {\"message\":\"model overloaded\"}\n");
        QVERIFY(api.error().contains(QString("model overloaded")));
    }

    void utf8SplitAcrossChunks()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));

        QByteArray line = QString::fromUtf8(
                    "event: message.output.delta\n"
                    "data: {\"content\":\"caf\xC3\xA9\"}\n").toUtf8();
        // Split in the middle of the two-byte e-acute sequence
        int cut = line.indexOf("\xC3") + 1;
        api.processStreamData(line.left(cut));
        QCOMPARE(spy.count(), 0);  // incomplete line buffered
        api.processStreamData(line.mid(cut));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString::fromUtf8("caf\xC3\xA9"));
    }

    void malformedLinesIgnored()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));
        api.processStreamData("garbage\n");
        api.processStreamData("data: {broken json\n");
        api.processStreamData(": comment\n");
        api.processStreamData("\n\n");
        // A data line without an event name is not a delta
        api.processStreamData("data: {\"content\":\"x\"}\n");
        QCOMPARE(spy.count(), 0);
    }

    void multipleLinesOneChunk()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));
        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":\"a\"}\n"
                              "event: message.output.delta\n"
                              "data: {\"content\":\"b\"}\n"
                              "event: conversation.response.done\n"
                              "data: {\"usage\":{}}\n");
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toString(), QString("a"));
        QCOMPARE(spy.at(1).at(0).toString(), QString("b"));
        QVERIFY(api.m_streamDone);
    }

    // "#stream" is a URL fragment: QUrl would strip a literal '#', so the
    // path must stay percent-encoded.
    void turnPathEncodesStreamFragment()
    {
        MistralAPI api;
        QCOMPARE(api.turnPath(QString()), QString("/conversations%23stream"));
        QCOMPARE(api.turnPath("abc-123"), QString("/conversations/abc-123%23stream"));

        const QUrl url = api.endpoint(api.turnPath("abc-123"));
        QVERIFY(url.isValid());
        QCOMPARE(url.path(), QString("/v1/conversations/abc-123#stream"));
        QVERIFY(!url.hasFragment() || url.fragment() == "stream");
        // The encoded form survives a round trip through the wire format
        QVERIFY(url.toEncoded().contains("%23stream"));
    }

    // Create carries model, instructions and tools; append carries only the
    // new turn plus completion args.
    void buildTurnBodyCreateVsAppend()
    {
        MistralAPI api;

        const QJsonDocument createDoc = QJsonDocument::fromJson(api.buildTurnBody(
                    "mistral-large-latest", "  be brief  ", "hello", QString(),
                    true, true, 0.7, 512));
        const QJsonObject create = createDoc.object();
        QCOMPARE(create["model"].toString(), QString("mistral-large-latest"));
        QCOMPARE(create["instructions"].toString(), QString("be brief"));
        QCOMPARE(create["tools"].toArray().count(), 1);
        QCOMPARE(create["tools"].toArray().at(0).toObject()["type"].toString(),
                 QString("web_search"));
        QCOMPARE(create["completion_args"].toObject()["temperature"].toDouble(), 0.7);
        QCOMPARE(create["completion_args"].toObject()["max_tokens"].toInt(), 512);

        const QJsonObject input = create["inputs"].toArray().at(0).toObject();
        QCOMPARE(input["object"].toString(), QString("entry"));
        QCOMPARE(input["type"].toString(), QString("message.input"));
        QCOMPARE(input["role"].toString(), QString("user"));
        QCOMPARE(input["content"].toString(), QString("hello"));

        // Append: no model, no instructions, no tools.
        const QJsonDocument appendDoc = QJsonDocument::fromJson(api.buildTurnBody(
                    "mistral-large-latest", "be brief", "second turn", QString(),
                    true, false, -1.0, 0));
        const QJsonObject append = appendDoc.object();
        QVERIFY(!append.contains("model"));
        QVERIFY(!append.contains("instructions"));
        QVERIFY(!append.contains("tools"));
        QVERIFY(!append.contains("completion_args"));  // defaults omitted
        QCOMPARE(append["inputs"].toArray().at(0).toObject()["content"].toString(),
                 QString("second turn"));

        // Without web search the create body has no tools array
        const QJsonDocument noToolsDoc = QJsonDocument::fromJson(api.buildTurnBody(
                    "m", QString(), "hi", QString(), false, true, -1.0, 0));
        QVERIFY(!noToolsDoc.object().contains("tools"));
        QVERIFY(!noToolsDoc.object().contains("instructions"));
    }

    // An image turn sends content as a chunk list with a data URL. Skipped
    // when the test host has no JPEG image plugin (the Sailfish SDK ships
    // one; a minimal desktop Qt may not).
    void buildTurnBodyWithImage()
    {
        if (!QImageWriter::supportedImageFormats().contains("jpeg")) {
            QSKIP("No JPEG image plugin on this host");
        }

        MistralAPI api;

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString imagePath = dir.filePath("pic.png");
        QImage img(4, 4, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(imagePath));

        const QJsonDocument doc = QJsonDocument::fromJson(api.buildTurnBody(
                    "pixtral-large-latest", QString(), "what is this?",
                    imagePath, false, true, -1.0, 0));
        const QJsonObject input = doc.object()["inputs"].toArray().at(0).toObject();
        const QJsonArray chunks = input["content"].toArray();
        QCOMPARE(chunks.count(), 2);
        QCOMPARE(chunks.at(0).toObject()["type"].toString(), QString("text"));
        QCOMPARE(chunks.at(0).toObject()["text"].toString(), QString("what is this?"));
        QCOMPARE(chunks.at(1).toObject()["type"].toString(), QString("image_url"));
        QVERIFY(chunks.at(1).toObject()["image_url"].toString()
                .startsWith(QString("data:image/jpeg;base64,")));
    }

    // Without a readable image the turn degrades to plain text instead of
    // failing: the user still gets an answer.
    void buildTurnBodyWithBrokenImageFallsBackToText()
    {
        MistralAPI api;

        const QJsonDocument doc = QJsonDocument::fromJson(api.buildTurnBody(
                    "m", QString(), "what is this?",
                    "/nonexistent/image.jpg", false, true, -1.0, 0));
        const QJsonObject input = doc.object()["inputs"].toArray().at(0).toObject();
        QCOMPARE(input["content"].toString(), QString("what is this?"));
    }

    // The reconcile check: does the remote history already contain the turn?
    void remoteHasUserMessageDetectsTurn()
    {
        QJsonObject response;
        QJsonArray messages;

        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = "  what is the weather?  ";
        messages.append(userMsg);

        QJsonObject assistantMsg;
        assistantMsg["role"] = "assistant";
        assistantMsg["content"] = "sunny";
        messages.append(assistantMsg);

        // An image turn: content is a chunk list, the text chunk must match
        QJsonObject imageMsg;
        imageMsg["role"] = "user";
        QJsonArray chunks;
        QJsonObject textChunk;
        textChunk["type"] = "text";
        textChunk["text"] = "describe this";
        chunks.append(textChunk);
        QJsonObject imageChunk;
        imageChunk["type"] = "image_url";
        imageChunk["image_url"] = "data:image/jpeg;base64,AAAA";
        chunks.append(imageChunk);
        imageMsg["content"] = chunks;
        messages.append(imageMsg);

        response["messages"] = messages;

        QVERIFY(MistralAPI::remoteHasUserMessage(response, "what is the weather?"));
        QVERIFY(MistralAPI::remoteHasUserMessage(response, "describe this"));
        QVERIFY(!MistralAPI::remoteHasUserMessage(response, "not sent yet"));
        QVERIFY(!MistralAPI::remoteHasUserMessage(response, QString()));
    }

    // The title answer is read from the non-streaming outputs array.
    void outputsToTextConcatenates()
    {
        QJsonArray outputs;

        QJsonObject plain;
        plain["content"] = "plain answer";
        outputs.append(plain);

        QJsonObject reasoned;
        QJsonArray chunks;
        QJsonObject thinking;
        thinking["type"] = "thinking";
        thinking["thinking"] = "let me think";
        chunks.append(thinking);
        QJsonObject text;
        text["type"] = "text";
        text["text"] = "real answer";
        chunks.append(text);
        reasoned["content"] = chunks;
        outputs.append(reasoned);

        QCOMPARE(MistralAPI::outputsToText(outputs),
                 QString("plain answerreal answer"));
        QCOMPARE(MistralAPI::outputsToText(QJsonArray()), QString());
    }

    // A free tier turns requests away routinely; the policy that decides
    // whether to send the same one again is worth pinning down.
    void retriesRateLimitedRequest()
    {
        MistralAPI api;
        api.m_pendingBody = "{}";

        QVERIFY(api.shouldRetry(429));
        QVERIFY(api.shouldRetry(503));
        QVERIFY(api.shouldRetry(529));
        QVERIFY(!api.shouldRetry(200));
        QVERIFY(!api.shouldRetry(401));  // a bad key stays bad

        // Nothing to resend, half an answer already on screen, or out of
        // attempts: all three mean the request is done for.
        api.m_pendingBody.clear();
        QVERIFY(!api.shouldRetry(429));

        api.m_pendingBody = "{}";
        api.m_streamStarted = true;
        QVERIFY(!api.shouldRetry(429));

        api.m_streamStarted = false;
        api.m_retryCount = 3;
        QVERIFY(!api.shouldRetry(429));

        // A timed-out request is not retried either: the fate of the turn on
        // the server is unknown, and the reconcile path handles appends.
        api.m_retryCount = 0;
        api.m_timedOut = true;
        QVERIFY(!api.shouldRetry(429));
    }

    void spacesRequestsOut()
    {
        MistralAPI api;

        // Nothing sent yet: the first request leaves immediately.
        QCOMPARE(api.throttleDelayMs(), 0);

        api.noteRequestSent();
        QVERIFY(api.throttleDelayMs() > 0);
    }
};

class TestConversationManager : public QObject
{
    Q_OBJECT

private slots:
    void saveAndReloadConversation()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString id = manager.currentConversationId();
        QVERIFY(!id.isEmpty());

        ConversationModel *model = manager.currentConversation();
        model->addUserMessage("hello", "/tmp/img.jpg");
        model->addAssistantMessage("hi");
        model->togglePinned(1);
        manager.saveCurrentConversation();
        manager.updateCurrentConversationTitle("My chat");
        manager.updateCurrentConversationCategory("code");

        // Switch away and back: content must survive the round trip
        manager.createNewConversation();
        manager.loadConversation(id);

        QCOMPARE(manager.currentConversation()->rowCount(), 2);
        QJsonArray json = manager.currentConversation()->toJsonArray();
        QCOMPARE(json.at(0).toObject()["imagePath"].toString(), QString("/tmp/img.jpg"));
        QCOMPARE(json.at(1).toObject()["pinned"].toBool(), true);

        QVariantMap details = manager.getConversationDetails(id).toMap();
        QCOMPARE(details["title"].toString(), QString("My chat"));

        QVariantMap stats = manager.getConversationStatistics(id);
        QCOMPARE(stats["category"].toString(), QString("code"));
        QCOMPARE(stats["messageCount"].toInt(), 2);
    }

    void answersRecordWhichModelProducedThem()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        manager.currentConversation()->addUserMessage("hello");
        manager.currentConversation()->addAssistantMessage(
                    "hi", "mistral-small-latest", "mistral");
        manager.saveCurrentConversation();

        const QString id = manager.currentConversationId();
        manager.createNewConversation();
        manager.loadConversation(id);
        const QJsonArray json = manager.currentConversation()->toJsonArray();
        // The user turn stays as it was, the answer carries its model
        QVERIFY(!json.at(0).toObject().contains("provider"));
        QCOMPARE(json.at(1).toObject()["provider"].toString(), QString("mistral"));
        QCOMPARE(json.at(1).toObject()["model"].toString(),
                 QString("mistral-small-latest"));
    }

    void preProviderConversationsAreStampedMistral()
    {
        ConversationManager manager;
        manager.purgeAllConversations();
        const QString dir = QStandardPaths::writableLocation(
                    QStandardPaths::AppDataLocation) + "/conversations";
        QVERIFY(QDir().mkpath(dir));
        // A file exactly as 2.2 wrote it: no provider anywhere
        const QString id = "11111111-2222-3333-4444-555555555555";
        QFile file(dir + "/" + id + ".json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"id\":\"" + id.toUtf8() + "\",\"title\":\"Old chat\","
                   "\"category\":\"code\",\"createdAt\":1000,\"updatedAt\":2000,"
                   "\"messages\":["
                   "{\"role\":\"user\",\"content\":\"q\",\"timestamp\":1000},"
                   "{\"role\":\"assistant\",\"content\":\"a\",\"timestamp\":1001}]}");
        file.close();
        ConversationManager reopened;
        reopened.loadConversation(id);
        const QJsonArray json = reopened.currentConversation()->toJsonArray();
        QCOMPARE(json.count(), 2);
        QVERIFY(!json.at(0).toObject().contains("provider"));
        // The answer is stamped mistral: the only provider that ever existed
        QCOMPARE(json.at(1).toObject()["provider"].toString(), QString("mistral"));
        QVERIFY(!json.at(1).toObject().contains("model"));
    }

    // The alternative to spending a request on a title: it has to produce
    // something usable on its own.
    void localLabellingNeedsNoRequest()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        const QString id = manager.currentConversationId();
        ConversationModel *model = manager.currentConversation();
        model->addUserMessage("The app crashed with a segfault, "
                              "how do I fix this traceback?");
        model->addAssistantMessage("Run it under a debugger.");
        manager.saveCurrentConversation();

        manager.autoLabelConversation(id);

        const QVariantMap labels = manager.getConversationOverrides(id);
        QVERIFY(!labels["title"].toString().isEmpty());
        // The local classifier had something to work with
        QCOMPARE(labels["category"].toString(), QString("debugging"));
    }

    void localLabellingKeepsAChosenCategory()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        const QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("A python traceback again");
        manager.saveCurrentConversation();
        manager.setConversationCategory(id, "cooking");

        manager.autoLabelConversation(id);

        QCOMPARE(manager.getConversationOverrides(id)["category"].toString(),
                 QString("cooking"));
    }

    void statisticsRankModels()
    {
        ConversationManager manager;
        manager.purgeAllConversations();
        ConversationModel *model = manager.currentConversation();
        model->addUserMessage("one");
        model->addAssistantMessage("a", "mistral-small-latest", "mistral");
        model->addUserMessage("two");
        model->addAssistantMessage("b", "mistral-small-latest", "mistral");
        model->addUserMessage("three");
        model->addAssistantMessage("c", "mistral-large-latest", "mistral");
        // An answer from before models were recorded: no model, no count
        model->addUserMessage("four");
        model->addAssistantMessage("d", QString(), "mistral");
        manager.saveCurrentConversation();
        const QVariantMap stats = manager.getStatistics();
        QVERIFY(!stats.contains("topProvider"));
        QVERIFY(!stats.contains("providerUsage"));
        QCOMPARE(stats["topModel"].toString(), QString("mistral-small-latest"));
        QCOMPARE(stats["topModelCount"].toInt(), 2);
        const QVariantList models = stats["modelAnswers"].toList();
        QCOMPARE(models.count(), 2);
        int total = 0;
        for (int i = 0; i < models.count(); ++i) {
            total += models.at(i).toMap()["count"].toInt();
        }
        QCOMPARE(total, 3);
    }

    void historyListReportsTheModelThatAnswered()
    {
        ConversationManager manager;
        manager.purgeAllConversations();
        const QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("hi");
        manager.currentConversation()->addAssistantMessage(
                    "hello", "mistral-small-latest", "mistral");
        manager.saveCurrentConversation();
        const QJsonArray list = manager.getConversationsList();
        bool found = false;
        for (int i = 0; i < list.count(); ++i) {
            const QJsonObject obj = list.at(i).toObject();
            if (obj["id"].toString() != id) {
                continue;
            }
            found = true;
            QCOMPARE(obj["lastModel"].toString(), QString("mistral-small-latest"));
            // A role called "model" would shadow the delegate's model object
            QVERIFY(!obj.contains("model"));
        }
        QVERIFY(found);
    }

    void markdownExport()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("What is Qt?");
        manager.currentConversation()->addAssistantMessage("A C++ framework.");
        manager.saveCurrentConversation();
        manager.updateCurrentConversationTitle("Qt question");

        QString md = manager.conversationToMarkdown(id);
        QVERIFY(md.startsWith("# Qt question"));
        QVERIFY(md.contains("## User"));
        QVERIFY(md.contains("What is Qt?"));
        QVERIFY(md.contains("## Assistant"));
        QVERIFY(md.contains("A C++ framework."));

        QCOMPARE(manager.conversationToMarkdown("no-such-id"), QString());
    }

    void pinnedMessagesAcrossConversations()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        manager.currentConversation()->addUserMessage("pin this");
        manager.currentConversation()->togglePinned(0);
        manager.saveCurrentConversation();

        QVariantList pins = manager.getPinnedMessages();
        QCOMPARE(pins.count(), 1);
        QCOMPARE(pins.at(0).toMap()["content"].toString(), QString("pin this"));
        QCOMPARE(pins.at(0).toMap()["messageIndex"].toInt(), 0);
    }

    void funStatsGhostAndLongest()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        // One abandoned conversation: single user message
        manager.currentConversation()->addUserMessage("abandoned question");
        manager.saveCurrentConversation();

        manager.createNewConversation();
        manager.currentConversation()->addUserMessage(QString(500, 'x'));
        manager.currentConversation()->addAssistantMessage("short reply");
        manager.currentConversation()->addUserMessage("thanks");
        manager.currentConversation()->addAssistantMessage("welcome welcome welcome welcome");
        manager.saveCurrentConversation();

        QVariantMap fun = manager.getFunStats();
        QCOMPARE(fun["ghostCount"].toInt(), 1);
        QCOMPARE(fun["longestUserChars"].toInt(), 500);
        QVERIFY(fun["topWords"].toList().count() > 0);
        QCOMPARE(fun["topWords"].toList().at(0).toMap()["word"].toString(),
                 QString("welcome"));
    }

    void tokenUsageAccumulates()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString id = manager.currentConversationId();
        qint64 before = manager.getStatistics()["totalTokens"].toLongLong();

        manager.addTokenUsage(10, 20);
        manager.addTokenUsage(5, 5);
        manager.addTokenUsage(0, 0);  // ignored

        QCOMPARE(manager.getStatistics()["totalTokens"].toLongLong(), before + 40);
        QCOMPARE(manager.getConversationStatistics(id)["totalTokens"].toLongLong(), qint64(40));

        QVariantList perDay = manager.getStatistics()["tokensPerDay"].toList();
        QCOMPARE(perDay.count(), 14);
        QVERIFY(perDay.last().toInt() >= 40);
    }

    void categoryCounting()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        manager.currentConversation()->addUserMessage("write me a poem");
        manager.saveCurrentConversation();
        manager.updateCurrentConversationCategory("writing");

        QVariantMap counts = manager.getStatistics()["categoryCounts"].toMap();
        QCOMPARE(counts["writing"].toInt(), 1);
    }

    void unknownCategoryFallsBackToClassifier()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage(
                    "my python script raises an exception, help me debug the stacktrace");
        manager.saveCurrentConversation();

        // The model answering "other" must not win over an obvious topic
        manager.updateCurrentConversationCategory("other");
        QString category = manager.getConversationStatistics(id)["category"].toString();
        QVERIFY(category != "other");

        // An unknown label is rejected the same way
        manager.updateCurrentConversationCategory("not-a-category");
        QVERIFY(manager.getConversationStatistics(id)["category"].toString() != "not-a-category");
    }

    void recategorizeOnlyTouchesUnlabelled()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString taggedId = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("a recipe for bread with a good sauce");
        manager.saveCurrentConversation();
        manager.updateCurrentConversationCategory("cooking");

        manager.createNewConversation();
        QString untaggedId = manager.currentConversationId();
        manager.currentConversation()->addUserMessage(
                    "book a flight and a hotel for my trip, I need a visa and an itinerary");
        manager.saveCurrentConversation();

        QCOMPARE(manager.recategorizeConversations(), 1);
        QCOMPARE(manager.getConversationStatistics(taggedId)["category"].toString(),
                 QString("cooking"));
        QCOMPARE(manager.getConversationStatistics(untaggedId)["category"].toString(),
                 QString("travel"));

        // Nothing left to do on a second pass
        QCOMPARE(manager.recategorizeConversations(), 0);
    }

    // The remote conversation id is persisted with the conversation and
    // survives a reload: it is what tells sendMessage to append rather than
    // create again.
    void remoteConversationIdRoundTrip()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        // Not created yet: empty, so the first turn creates the remote one
        QCOMPARE(manager.remoteConversationId(), QString());

        const QString localId = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("hello");

        // Simulate the API reporting a created remote conversation while this
        // conversation is the one being answered.
        MistralAPI api;
        manager.bindApi(&api);
        api.sendMessage("key", "model", QString(), "hello", QString(), false,
                        QString());
        api.processStreamData("event: conversation.created\n"
                              "data: {\"conversation_id\":\"remote-42\"}\n");
        QCOMPARE(manager.remoteConversationId(), QString("remote-42"));

        // Survives a switch away and back
        manager.createNewConversation();
        QCOMPARE(manager.remoteConversationId(), QString());
        manager.loadConversation(localId);
        QCOMPARE(manager.remoteConversationId(), QString("remote-42"));
    }

    // The pending turn handed to the API is the last user message of the
    // current conversation, image included.
    void pendingUserTurnAccessors()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QCOMPARE(manager.pendingUserContent(), QString());
        QCOMPARE(manager.pendingUserImagePath(), QString());

        manager.currentConversation()->addUserMessage("question", "/tmp/img.jpg");
        manager.currentConversation()->addAssistantMessage("");
        QCOMPARE(manager.pendingUserContent(), QString("question"));
        QCOMPARE(manager.pendingUserImagePath(), QString("/tmp/img.jpg"));
    }

    void conversationOverridesRoundTrip()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString id = manager.currentConversationId();
        QVariantMap empty = manager.getConversationOverrides(id);
        QCOMPARE(empty["model"].toString(), QString());
        QCOMPARE(empty["systemPrompt"].toString(), QString());

        manager.setConversationOverrides(id, "mistral-large-latest", " talk like a pirate ");
        manager.currentConversation()->addUserMessage("ahoy");
        manager.saveCurrentConversation();

        // Survives a switch away and back
        manager.createNewConversation();
        manager.loadConversation(id);

        QVariantMap overrides = manager.getConversationOverrides(id);
        QCOMPARE(overrides["model"].toString(), QString("mistral-large-latest"));
        QCOMPARE(overrides["systemPrompt"].toString(), QString("talk like a pirate"));
    }

    void listedConversationsCarryNoReservedRole()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        manager.currentConversation()->addUserMessage("hello");
        manager.saveCurrentConversation();
        manager.setConversationOverrides(manager.currentConversationId(),
                                         "mistral-large-latest", QString());

        // These objects are appended to a QML ListModel. A role named "model"
        // shadows the delegate's own model object and turns every other lookup
        // into undefined, which is how 2.1.0 lost all its conversation titles.
        QJsonObject entry = manager.getConversationsList().at(0).toObject();
        QVERIFY(!entry.contains("model"));
        QVERIFY(entry.contains("title"));
        QVERIFY(entry.contains("id"));
    }

    void conversationDigestCoversBothSpeakers()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString id = manager.currentConversationId();
        QCOMPARE(manager.conversationDigest(id), QString());

        manager.currentConversation()->addUserMessage("how do I mount a btrfs volume?");
        manager.currentConversation()->addAssistantMessage("Use the mount command.");
        manager.saveCurrentConversation();

        QString digest = manager.conversationDigest(id);
        QVERIFY(digest.contains("User: how do I mount a btrfs volume?"));
        QVERIFY(digest.contains("Assistant: Use the mount command."));

        // Long conversations stay bounded
        for (int i = 0; i < 50; ++i) {
            manager.currentConversation()->addUserMessage(QString(600, 'x'));
        }
        manager.saveCurrentConversation();
        QVERIFY(manager.conversationDigest(id).length() <= 2000);

        QCOMPARE(manager.conversationDigest("no-such-id"), QString());
    }

    void renameByIdWorksForAnyConversation()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString first = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("first");
        manager.saveCurrentConversation();

        manager.createNewConversation();
        manager.currentConversation()->addUserMessage("second");
        manager.saveCurrentConversation();

        // Renaming a conversation that is not the current one must work
        manager.renameConversation(first, "  Renamed from elsewhere  ");
        QCOMPARE(manager.getConversationOverrides(first)["title"].toString(),
                 QString("Renamed from elsewhere"));

        // And survive a reload from disk
        manager.loadConversation(first);
        QCOMPARE(manager.getConversationDetails(first).toMap()["title"].toString(),
                 QString("Renamed from elsewhere"));
    }

    void streamingLandsWithoutAnyUi()
    {
        // The chat page is popped whenever the user swipes back to the
        // history, so the whole streaming lifecycle has to work with no QML
        // attached at all.
        MistralAPI api;
        ConversationManager manager;
        manager.purgeAllConversations();
        manager.bindApi(&api);

        const QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("question");

        QVERIFY(QMetaObject::invokeMethod(&api, "messageSent"));
        QCOMPARE(manager.currentConversation()->rowCount(), 2);
        QCOMPARE(manager.streamingConversationId(), id);

        QVERIFY(QMetaObject::invokeMethod(&api, "streamingResponse",
                                          Q_ARG(QString, "Hel")));
        QVERIFY(QMetaObject::invokeMethod(&api, "streamingResponse",
                                          Q_ARG(QString, "lo")));
        // Deltas are coalesced, so give the throttle timer a chance to fire
        QTest::qWait(250);
        QCOMPARE(manager.currentConversation()->getLastAssistantMessage(),
                 QString("Hello"));

        QVERIFY(QMetaObject::invokeMethod(&api, "responseCompleted"));
        QCOMPARE(manager.streamingConversationId(), QString());

        QJsonObject entry = manager.getConversationsList().at(0).toObject();
        QCOMPARE(entry["id"].toString(), id);
        QCOMPARE(entry["messageCount"].toInt(), 2);
        QVERIFY(entry["unread"].toBool());

        manager.markConversationRead(id);
        QVERIFY(!manager.getConversationsList().at(0).toObject()["unread"].toBool());

        // Unknown ids are ignored rather than crashing
        manager.markConversationRead("no-such-id");
    }

    void answerNeverLeaksIntoAnotherConversation()
    {
        // ConversationModel is a single reused object. Switching conversation
        // mid-response used to make the rest of the answer land in - and be
        // saved into - whatever was opened next.
        MistralAPI api;
        ConversationManager manager;
        manager.purgeAllConversations();
        manager.bindApi(&api);

        const QString asking = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("question in A");
        manager.saveCurrentConversation();

        QVERIFY(QMetaObject::invokeMethod(&api, "messageSent"));
        QVERIFY(QMetaObject::invokeMethod(&api, "streamingResponse",
                                          Q_ARG(QString, "first half ")));
        QTest::qWait(250);

        // The user walks off to an unrelated conversation
        manager.createNewConversation();
        const QString other = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("unrelated question");
        manager.saveCurrentConversation();

        QVERIFY(QMetaObject::invokeMethod(&api, "streamingResponse",
                                          Q_ARG(QString, "second half")));
        QTest::qWait(250);

        // Nothing from the answer may show up here
        QCOMPARE(manager.currentConversation()->rowCount(), 1);
        QCOMPARE(manager.currentConversation()->getLastAssistantMessage(), QString());

        QVERIFY(QMetaObject::invokeMethod(&api, "responseCompleted"));

        // The other conversation is untouched
        QCOMPARE(manager.getConversationStatistics(other)["messageCount"].toInt(), 1);
        QCOMPARE(manager.currentConversation()->rowCount(), 1);

        // And the full answer landed where it was asked
        manager.loadConversation(asking);
        QCOMPARE(manager.currentConversation()->rowCount(), 2);
        QCOMPARE(manager.currentConversation()->getLastAssistantMessage(),
                 QString("first half second half"));
    }

    void returningMidStreamShowsTheLatestText()
    {
        MistralAPI api;
        ConversationManager manager;
        manager.purgeAllConversations();
        manager.bindApi(&api);

        const QString asking = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("question");
        manager.saveCurrentConversation();

        QVERIFY(QMetaObject::invokeMethod(&api, "messageSent"));
        QVERIFY(QMetaObject::invokeMethod(&api, "streamingResponse",
                                          Q_ARG(QString, "partial")));
        QTest::qWait(250);

        manager.createNewConversation();
        QVERIFY(QMetaObject::invokeMethod(&api, "streamingResponse",
                                          Q_ARG(QString, " and more")));
        QTest::qWait(250);

        // Coming back must show everything buffered so far, not the snapshot
        // taken when leaving
        manager.loadConversation(asking);
        QCOMPARE(manager.currentConversation()->getLastAssistantMessage(),
                 QString("partial and more"));

        QVERIFY(QMetaObject::invokeMethod(&api, "responseCompleted"));
    }

    void cancelledRequestLeavesNoEmptyBubble()
    {
        MistralAPI api;
        ConversationManager manager;
        manager.purgeAllConversations();
        manager.bindApi(&api);

        manager.currentConversation()->addUserMessage("question");
        QVERIFY(QMetaObject::invokeMethod(&api, "messageSent"));
        QCOMPARE(manager.currentConversation()->rowCount(), 2);

        // No content ever arrives: the placeholder must go, and nothing is
        // reported as unread
        QVERIFY(QMetaObject::invokeMethod(&api, "responseCompleted"));
        QCOMPARE(manager.currentConversation()->rowCount(), 1);

        QJsonObject entry = manager.getConversationsList().at(0).toObject();
        QVERIFY(entry["unread"].toBool());   // the question itself is still there
        QCOMPARE(entry["messageCount"].toInt(), 1);
    }

    void unreadFlagSurvivesReload()
    {
        MistralAPI api;
        {
            ConversationManager manager;
            manager.purgeAllConversations();
            manager.bindApi(&api);
            manager.currentConversation()->addUserMessage("question");
            QVERIFY(QMetaObject::invokeMethod(&api, "messageSent"));
            QVERIFY(QMetaObject::invokeMethod(&api, "streamingResponse",
                                              Q_ARG(QString, "answer")));
            QTest::qWait(250);
            QVERIFY(QMetaObject::invokeMethod(&api, "responseCompleted"));
        }

        ConversationManager reopened;
        QVERIFY(reopened.getConversationsList().at(0).toObject()["unread"].toBool());
    }

    void deletedConversationLeavesNoFile()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        manager.currentConversation()->addUserMessage("throwaway");
        manager.saveCurrentConversation();
        QVERIFY(manager.getStorageSize() > 0);

        manager.createNewConversation();
        int before = manager.conversationCount();
        manager.deleteConversation(manager.getConversationsList()
                                   .at(before - 1).toObject()["id"].toString());
        QCOMPARE(manager.conversationCount(), before - 1);
    }
};

class TestCategories : public QObject
{
    Q_OBJECT

private slots:
    void identifiersAreValidated()
    {
        QVERIFY(Categories::isValid("code"));
        QVERIFY(Categories::isValid("other"));
        QVERIFY(!Categories::isValid("nonsense"));
        QVERIFY(!Categories::isValid(""));

        // "other" is offered but must be the last resort
        QVERIFY(Categories::all().count() > 20);
        QCOMPARE(Categories::all().last(), QString("other"));
    }

    void classifiesObviousTopics()
    {
        QCOMPARE(Categories::classify(
                     "write a python function to loop over an array"), QString("code"));
        QCOMPARE(Categories::classify(
                     "quelle recette de sauce pour des pates au four"), QString("cooking"));
        QCOMPARE(Categories::classify(
                     "translate this sentence into English, traduction"), QString("translation"));
    }

    void classifierIgnoresAccentsAndCase()
    {
        // Same sentence with and without accents must land on the same label
        QCOMPARE(Categories::classify(QString::fromUtf8("Ma dépense, mon budget et mes impôts")),
                 Categories::classify("Ma depense, mon budget et mes impots"));
    }

    void refusesToGuessOnThinInput()
    {
        QCOMPARE(Categories::classify(""), QString());
        QCOMPARE(Categories::classify("   "), QString());
        QCOMPARE(Categories::classify("ok merci"), QString());
    }
};

class TestSecureStore : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        const QString secret = QString::fromUtf8("sk-ünïcode-key-1234567890");
        const QString encoded = SecureStore::obfuscate(secret);

        QVERIFY(!encoded.isEmpty());
        QVERIFY(!encoded.contains(secret));
        QCOMPARE(SecureStore::deobfuscate(encoded), secret);
    }

    void emptyInputStaysEmpty()
    {
        QCOMPARE(SecureStore::obfuscate(QString()), QString());
        QCOMPARE(SecureStore::deobfuscate(QString()), QString());
    }

    void plainTextPassesThrough()
    {
        // How a key written before this scheme existed is migrated
        QCOMPARE(SecureStore::deobfuscate("plain-api-key"), QString("plain-api-key"));
    }

    void corruptPayloadIsRejected()
    {
        QString encoded = SecureStore::obfuscate("secret");
        // Flip a payload character, past the 4-byte marker: the checksum must
        // catch it rather than hand back a mangled key
        const int pos = 5;
        QVERIFY(encoded.length() > pos);
        QChar original = encoded.at(pos);
        encoded.replace(pos, 1, original == QChar('A') ? QChar('B') : QChar('A'));
        QCOMPARE(SecureStore::deobfuscate(encoded), QString());
    }
};

class TestSettingsManager : public QObject
{
    Q_OBJECT

private slots:
    void apiKeyIsNotStoredInClearText()
    {
        {
            SettingsManager settings;
            settings.setApiKey("sk-test-secret-value");
            QCOMPARE(settings.apiKey(), QString("sk-test-secret-value"));
        }

        // Reread from disk: the key comes back, the file does not contain it
        SettingsManager reopened;
        QCOMPARE(reopened.apiKey(), QString("sk-test-secret-value"));

        QSettings raw("harbour-skrynja", "Skrynja");
        QVERIFY(!raw.contains("apiKey"));
        QVERIFY(!raw.value("apiKeyEnc").toString()
                .contains("sk-test-secret-value"));

        reopened.clearApiKey();
        QVERIFY(!reopened.hasApiKey());
    }

    void legacyProviderSettingsAreMigrated()
    {
        {
            SettingsManager settings;
            settings.clearApiKey();
        }
        // Simulate a SailCat install: key and model under providers/mistral/
        {
            QSettings raw("harbour-skrynja", "Skrynja");
            raw.remove("apiKeyEnc");
            raw.remove("modelName");
            raw.remove("models");
            raw.setValue("providers/mistral/apiKeyEnc",
                     SecureStore::obfuscate("sk-legacy-key"));
            raw.setValue("providers/mistral/modelName", "mistral-large-latest");
            raw.sync();
        }

        SettingsManager migrated;
        QCOMPARE(migrated.apiKey(), QString("sk-legacy-key"));
        QCOMPARE(migrated.modelName(), QString("mistral-large-latest"));

        // The multi-provider entries are gone, the single-provider ones remain
        QSettings raw("harbour-skrynja", "Skrynja");
        QVERIFY(!raw.contains("providers"));
        QVERIFY(!raw.contains("provider"));
        QVERIFY(!raw.value("apiKeyEnc").toString().isEmpty());
    }

    void resolveModelFallsBackToStoredDefault()
    {
        SettingsManager settings;

        QVariantList catalogue;
        QVariantMap entry;
        entry["id"] = "mistral-small-latest";
        entry["vision"] = false;
        catalogue.append(entry);
        settings.updateModelCache(catalogue);

        // The fetched catalogue replaces a model it does not serve
        QCOMPARE(settings.modelName(), QString("mistral-small-latest"));
        // A model the catalogue does not serve is rejected
        QCOMPARE(settings.resolveModel("some-other-model"),
                 QString("mistral-small-latest"));
        QCOMPARE(settings.resolveModel("mistral-small-latest"),
                 QString("mistral-small-latest"));
    }

    void chatStyleRejectsUnknownValues()
    {
        SettingsManager settings;
        settings.setChatStyle("bubbles");
        QCOMPARE(settings.chatStyle(), QString("bubbles"));

        settings.setChatStyle("neon-hologram");
        QCOMPARE(settings.chatStyle(), QString("bubbles"));

        settings.setChatStyle("flat");
    }

    void savedPromptsRoundTrip()
    {
        SettingsManager settings;
        while (!settings.savedPrompts().isEmpty()) {
            settings.removeSavedPrompt(0);
        }

        settings.addSavedPrompt("", "  Summarise this in three bullets  ");
        QCOMPARE(settings.savedPrompts().count(), 1);

        QVariantMap entry = settings.savedPrompts().at(0).toMap();
        QCOMPARE(entry["text"].toString(), QString("Summarise this in three bullets"));
        // A missing name falls back to the start of the prompt
        QVERIFY(!entry["title"].toString().isEmpty());

        settings.addSavedPrompt("Blank", "   ");   // ignored
        QCOMPARE(settings.savedPrompts().count(), 1);

        settings.updateSavedPrompt(0, "Summary", "Do it shorter");
        QCOMPARE(settings.savedPrompts().at(0).toMap()["title"].toString(), QString("Summary"));

        settings.removeSavedPrompt(5);   // out of range: no crash
        settings.removeSavedPrompt(0);
        QCOMPARE(settings.savedPrompts().count(), 0);
    }

    void costEstimation()
    {
        SettingsManager settings;

        // 1M in + 1M out on the large model, at 0.50 / 1.50 per million
        double cost = settings.estimatedCost("mistral-large-latest", 1000000, 1000000);
        QVERIFY(qAbs(cost - 2.0) < 0.0001);

        // The more specific entry wins over the "pixtral" prefix
        QCOMPARE(settings.modelPricing("pixtral-large-latest")["output"].toDouble(), 6.00);
        QCOMPARE(settings.modelPricing("pixtral-12b-latest")["output"].toDouble(), 0.15);
        QCOMPARE(settings.modelPricing("mistral-large-4")["input"].toDouble(), 1.36);
        QCOMPARE(settings.modelPricing("ministral-3-8b-2512")["input"].toDouble(), 0.15);

        // Unknown model: no invented number
        QVERIFY(!settings.modelPricing("some-future-model")["known"].toBool());
        QCOMPARE(settings.estimatedCost("some-future-model", 1000, 1000), -1.0);
    }

};

class TestProviders : public QObject
{
    Q_OBJECT

private slots:
    void constantsDescribeMistral()
    {
        QCOMPARE(Providers::ID, QString("mistral"));
        QCOMPARE(Providers::NAME, QString("Mistral AI"));
        QCOMPARE(Providers::BASE_URL, QString("https://api.mistral.ai/v1"));
        QVERIFY(Providers::KEY_URL.contains("mistral.ai"));
        QVERIFY(!Providers::REGION.isEmpty());
    }

    void fallbackCatalogueIsUsableBeforeTheFirstFetch()
    {
        const QStringList fallback = Providers::fallbackModels();
        QVERIFY(!fallback.isEmpty());
        // The default must always be offered
        QVERIFY(fallback.contains(Providers::defaultModel()));
        // No duplicates
        QStringList sorted = fallback;
        sorted.sort();
        QVERIFY(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
    }

    void defaultModelIsAChatModel()
    {
        QVERIFY(Providers::isChatModelId(Providers::defaultModel()));
    }

    void nonChatModelsAreFilteredOut()
    {
        // Real ids from the OVHcloud catalogue, which tags none of them
        QVERIFY(Providers::isChatModelId("Mistral-Small-3.2-24B-Instruct-2506"));
        QVERIFY(Providers::isChatModelId("gpt-oss-120b"));
        QVERIFY(!Providers::isChatModelId("Qwen3-Embedding-8B"));
        QVERIFY(!Providers::isChatModelId("bge-m3"));
        QVERIFY(!Providers::isChatModelId("whisper-large-v3"));
        QVERIFY(!Providers::isChatModelId("nvr-tts-en-us"));
        QVERIFY(!Providers::isChatModelId("stable-diffusion-xl-base-v10"));
        QVERIFY(!Providers::isChatModelId(""));
    }

    void visionIsGuessedConservatively()
    {
        QVERIFY(Providers::looksLikeVisionModel("pixtral-12b-latest"));
        QVERIFY(Providers::looksLikeVisionModel("Qwen2.5-VL-72B-Instruct"));
        QVERIFY(Providers::looksLikeVisionModel("llava-next-mistral-7b"));
        // On doubt the answer is no: the attachment button stays hidden
        QVERIFY(!Providers::looksLikeVisionModel("llama-3.3-70b-versatile"));
        QVERIFY(!Providers::looksLikeVisionModel("gpt-oss-120b"));
    }
};

int main(int argc, char *argv[])
{
    // Redirect QSettings and the conversation files away from the real
    // user configuration
    QTemporaryDir configDir;
    QTemporaryDir dataDir;
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());
    qputenv("XDG_DATA_HOME", dataDir.path().toUtf8());

    QCoreApplication app(argc, argv);
    app.setOrganizationName("harbour-skrynja");
    app.setApplicationName("Skrynja");

    int status = 0;
    {
        TestConversationModel t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestMistralAPI t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestConversationManager t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestCategories t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestSecureStore t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestSettingsManager t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestProviders t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "tst_main.moc"
