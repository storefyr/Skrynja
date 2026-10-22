#include <QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QScopedPointer>
#include <QtMath>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QUrl>
#include <QImage>
#include <QImageWriter>
#include <QSet>

#include "categories.h"
#include "conversationmodel.h"
#include "conversationmanager.h"
#include "mistralapi.h"
#include "providers.h"
#include "securestore.h"
#include "settingsmanager.h"
#include "voiceanonymizer.h"
#include "agenttools.h"

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

    // F14: the remote file id rides along with the image path and survives
    // the JSON round trip.
    void imageFileIdRoundTrip()
    {
        ConversationModel model;
        model.addUserMessage("look", "/tmp/photo.jpg", "file-id-abc");
        QCOMPARE(model.getLastUserImageFileId(), QString("file-id-abc"));
        QCOMPARE(model.toJsonArray().at(0).toObject()["imageFileId"].toString(),
                 QString("file-id-abc"));

        // setLastUserImageFileId updates the last user message
        model.addUserMessage("second");
        model.setLastUserImageFileId("file-id-xyz");
        QCOMPARE(model.getLastUserImageFileId(), QString("file-id-xyz"));
        // An assistant message is not a user message: the setter is a no-op
        model.addAssistantMessage("answer");
        model.setLastUserImageFileId("should-not-stick");
        QCOMPARE(model.getLastUserImageFileId(), QString("file-id-xyz"));
    }

    // Without a file id the field stays empty: old conversations load as
    // they were.
    void imageFileIdDefaultsToEmpty()
    {
        ConversationModel model;
        model.addUserMessage("plain text");
        QCOMPARE(model.getLastUserImageFileId(), QString());
        QVERIFY(!model.toJsonArray().at(0).toObject().contains("imageFileId"));
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
    // streamed, thinking chunks are dropped from the answer.
    void reasoningChunksKeepOnlyText()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));
        QSignalSpy thinkSpy(&api, SIGNAL(thinkingResponse(QString)));

        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":[{\"type\":\"thinking\",\"thinking\":\"hmm\"},"
                              "{\"type\":\"text\",\"text\":\"answer\"}]}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("answer"));
        // Thinking is not emitted during streaming, only at the end
        QCOMPARE(thinkSpy.count(), 0);
    }

    // Thinking chunks are accumulated across deltas and emitted once when
    // the response completes, before sources and usage.
    void thinkingIsBufferedAndEmittedAtEnd()
    {
        MistralAPI api;
        QSignalSpy thinkSpy(&api, SIGNAL(thinkingResponse(QString)));
        QSignalSpy textSpy(&api, SIGNAL(streamingResponse(QString)));

        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":[{\"type\":\"thinking\",\"thinking\":\"step 1 \"},"
                              "{\"type\":\"text\",\"text\":\"Hello\"}]}\n");
        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":[{\"type\":\"thinking\",\"thinking\":\"step 2\"},"
                              "{\"type\":\"text\",\"text\":\" world\"}]}\n");
        QCOMPARE(thinkSpy.count(), 0);
        QCOMPARE(textSpy.count(), 2);

        api.processStreamData("event: conversation.response.done\n"
                              "data: {\"usage\":{}}\n");
        QCOMPARE(thinkSpy.count(), 1);
        QCOMPARE(thinkSpy.at(0).at(0).toString(), QString("step 1 step 2"));
    }

    // No thinking chunks means no thinkingResponse signal.
    void noThinkingNoSignal()
    {
        MistralAPI api;
        QSignalSpy thinkSpy(&api, SIGNAL(thinkingResponse(QString)));

        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":\"plain answer\"}\n");
        api.processStreamData("event: conversation.response.done\n"
                              "data: {\"usage\":{}}\n");
        QCOMPARE(thinkSpy.count(), 0);
    }

    // Thinking survives a save/load cycle on the conversation file.
    void thinkingSurvivesPersistence()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        const QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("why is the sky blue?");
        manager.currentConversation()->addAssistantMessage(
                    "Because of Rayleigh scattering.",
                    "magistral-medium-latest", "mistral",
                    QStringList(), "Light scatters more at shorter wavelengths.");
        manager.saveCurrentConversation();

        manager.createNewConversation("mistral-small-latest");
        manager.loadConversation(id);

        const QList<Message> &msgs = manager.currentConversation()->messages();
        QCOMPARE(msgs.count(), 2);
        QCOMPARE(msgs.at(1).role, QString("assistant"));
        QCOMPARE(msgs.at(1).content, QString("Because of Rayleigh scattering."));
        QCOMPARE(msgs.at(1).thinking,
                 QString("Light scatters more at shorter wavelengths."));
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

        api.processStreamData("event: conversation.response.started\n"
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

    // web_search results arrive on tool.execution.done and are collected
    // as source URLs, deduplicated, in order of first appearance.
    void toolExecutionDoneCollectsSources()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(sourcesReceived(QStringList)));

        api.processStreamData(
            "event: tool.execution.done\n"
            "data: {\"data\":{\"output\":["
            "{\"url\":\"https://example.com/a\"},"
            "{\"url\":\"https://example.com/b\"}"
            "]}}\n");
        // Emitted only when the response completes, not on the tool event.
        QCOMPARE(spy.count(), 0);

        api.processStreamData("event: conversation.response.done\n"
                              "data: {\"usage\":{\"prompt_tokens\":1,\"completion_tokens\":1}}\n");
        QCOMPARE(spy.count(), 1);
        const QStringList sources = spy.at(0).at(0).toStringList();
        QCOMPARE(sources.count(), 2);
        QCOMPARE(sources.at(0), QString("https://example.com/a"));
        QCOMPARE(sources.at(1), QString("https://example.com/b"));
    }

    // Duplicate URLs from multiple searches are kept once.
    void sourcesAreDeduplicated()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(sourcesReceived(QStringList)));

        api.processStreamData(
            "event: tool.execution.done\n"
            "data: {\"data\":{\"output\":[{\"url\":\"https://a.com\"}]}}\n");
        api.processStreamData(
            "event: tool.execution.done\n"
            "data: {\"data\":{\"output\":["
            "{\"url\":\"https://a.com\"},"
            "{\"url\":\"https://b.com\"}"
            "]}}\n");
        api.processStreamData("event: conversation.response.done\n"
                              "data: {\"usage\":{}}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toStringList().count(), 2);
    }

    // No sources means no signal: the UI does not need to clear anything.
    void noSourcesNoSignal()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(sourcesReceived(QStringList)));

        api.processStreamData("event: conversation.response.done\n"
                              "data: {\"usage\":{}}\n");
        QCOMPARE(spy.count(), 0);
    }

    // Sources survive a save/load cycle on the conversation file.
    void sourcesSurvivePersistence()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        const QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage("what is skynja?");
        manager.currentConversation()->addAssistantMessage(
                    "A chat app.",
                    "mistral-small-latest", "mistral",
                    QStringList() << "https://example.com/skrynja");
        manager.saveCurrentConversation();

        manager.createNewConversation("mistral-small-latest");
        manager.loadConversation(id);

        const QList<Message> &msgs = manager.currentConversation()->messages();
        QCOMPARE(msgs.count(), 2);
        QCOMPARE(msgs.at(1).role, QString("assistant"));
        QCOMPARE(msgs.at(1).sources.count(), 1);
        QCOMPARE(msgs.at(1).sources.at(0), QString("https://example.com/skrynja"));
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
        // A data line without an event name and without a "type" field
        // is not a delta we can identify.
        api.processStreamData("data: {\"content\":\"x\"}\n");
        QCOMPARE(spy.count(), 0);
    }

    // SSE spec: the space after the colon is optional. "data:{...}" and
    // "event:foo" must parse the same as "data: {...}" and "event: foo".
    void sseNoSpaceAfterColon()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(streamingResponse(QString)));
        api.processStreamData("event:message.output.delta\n"
                              "data:{\"content\":\"nospace\"}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("nospace"));
    }

    // Some servers send data-only SSE (no "event:" line). The payload
    // self-describes via its "type" field and must be dispatched.
    void dataOnlySseSelfDescribesViaType()
    {
        MistralAPI api;
        QSignalSpy streamSpy(&api, SIGNAL(streamingResponse(QString)));
        QSignalSpy convSpy(&api, SIGNAL(conversationCreated(QString)));
        QSignalSpy doneSpy(&api, SIGNAL(usageReceived(int,int)));

        api.processStreamData("data: {\"type\":\"conversation.response.started\","
                              "\"conversation_id\":\"data-only-1\"}\n");
        api.processStreamData("data: {\"type\":\"message.output.delta\","
                              "\"content\":\"hello\"}\n");
        api.processStreamData("data: {\"type\":\"conversation.response.done\","
                              "\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":3}}\n");

        QCOMPARE(convSpy.count(), 1);
        QCOMPARE(convSpy.at(0).at(0).toString(), QString("data-only-1"));
        QCOMPARE(streamSpy.count(), 1);
        QCOMPARE(streamSpy.at(0).at(0).toString(), QString("hello"));
        QCOMPARE(doneSpy.count(), 1);
        QVERIFY(api.m_streamDone);
    }

    // ThinkChunk.thinking is a list of text chunks, not a plain string.
    // The reasoning text must be extracted from the nested structure.
    void thinkingChunkListIsFlattened()
    {
        MistralAPI api;
        QSignalSpy thinkingSpy(&api, SIGNAL(thinkingResponse(QString)));
        QSignalSpy streamSpy(&api, SIGNAL(streamingResponse(QString)));

        api.processStreamData("event: message.output.delta\n"
                              "data: {\"content\":["
                              "{\"type\":\"thinking\","
                              "\"thinking\":[{\"type\":\"text\",\"text\":\"let me think\"}]},"
                              "{\"type\":\"text\",\"text\":\"answer\"}"
                              "]}\n");
        api.processStreamData("event: conversation.response.done\n"
                              "data: {\"usage\":{}}\n");

        QCOMPARE(streamSpy.count(), 1);
        QCOMPARE(streamSpy.at(0).at(0).toString(), QString("answer"));
        QCOMPARE(thinkingSpy.count(), 1);
        QCOMPARE(thinkingSpy.at(0).at(0).toString(), QString("let me think"));
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

    // The Conversations API uses plain paths; streaming is a body parameter
    // ("stream": true), not a URL fragment. Mistral's docs write "#stream"
    // as endpoint notation, but the wire path is just /v1/conversations.
    void turnPathIsPlainConversations()
    {
        MistralAPI api;
        QCOMPARE(api.turnPath(QString()), QString("/conversations"));
        QCOMPARE(api.turnPath("abc-123"), QString("/conversations/abc-123"));

        const QUrl url = api.endpoint(api.turnPath("abc-123"));
        QVERIFY(url.isValid());
        QCOMPARE(url.path(), QString("/v1/conversations/abc-123"));
        QVERIFY(!url.hasFragment());
        QVERIFY(!url.toEncoded().contains("%23"));
    }

    // The request body must carry "stream": true so the server answers with
    // SSE instead of a single JSON response.
    void buildTurnBodyHasStreamTrue()
    {
        MistralAPI api;
        const QByteArray body = api.buildTurnBody("mistral-small-latest",
                                                  QString(), "hello",
                                                  QString(), false,
                                                  true, -1.0, 0);
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        QCOMPARE(obj["stream"].toBool(), true);
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

    // F14: an uploaded image is referenced by a file chunk, not a data URL.
    // The request body stays small no matter how large the image was.
    void buildTurnBodyWithFileIdUsesFileChunk()
    {
        MistralAPI api;

        const QJsonDocument doc = QJsonDocument::fromJson(api.buildTurnBody(
                    "pixtral-large-latest", QString(), "what is this?",
                    "/some/local/path.jpg", false, true, -1.0, 0,
                    "abc-123-file-id"));
        const QJsonObject input = doc.object()["inputs"].toArray().at(0).toObject();
        const QJsonArray chunks = input["content"].toArray();
        QCOMPARE(chunks.count(), 2);
        QCOMPARE(chunks.at(0).toObject()["type"].toString(), QString("text"));
        QCOMPARE(chunks.at(0).toObject()["text"].toString(), QString("what is this?"));
        QCOMPARE(chunks.at(1).toObject()["type"].toString(), QString("file"));
        QCOMPARE(chunks.at(1).toObject()["file_id"].toString(),
                 QString("abc-123-file-id"));
        // No base64 payload anywhere in the body
        QVERIFY(!doc.toJson().contains("base64"));
    }

    // A file id with no text: the chunk list holds only the file chunk.
    void buildTurnBodyWithFileIdOnlyImage()
    {
        MistralAPI api;

        const QJsonDocument doc = QJsonDocument::fromJson(api.buildTurnBody(
                    "m", QString(), QString(),
                    "/path.jpg", false, true, -1.0, 0, "fid-1"));
        const QJsonObject input = doc.object()["inputs"].toArray().at(0).toObject();
        const QJsonArray chunks = input["content"].toArray();
        QCOMPARE(chunks.count(), 1);
        QCOMPARE(chunks.at(0).toObject()["type"].toString(), QString("file"));
        QCOMPARE(chunks.at(0).toObject()["file_id"].toString(), QString("fid-1"));
    }

    // The file id wins over the inline path: when both are set the body
    // carries the reference, not the data URL.
    void buildTurnBodyFileIdTakesPrecedenceOverInline()
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
                    "m", QString(), "look", imagePath, false, true, -1.0, 0,
                    "uploaded-id"));
        const QJsonObject input = doc.object()["inputs"].toArray().at(0).toObject();
        const QJsonArray chunks = input["content"].toArray();
        QCOMPARE(chunks.count(), 2);
        QCOMPARE(chunks.at(1).toObject()["type"].toString(), QString("file"));
        QCOMPARE(chunks.at(1).toObject()["file_id"].toString(),
                 QString("uploaded-id"));
        QVERIFY(!doc.toJson().contains("base64"));
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

    // F18: the transcription model must be the Voxtral 3 batch model. The
    // previous alias voxtral-mini-latest resolves to Voxtral Mini Transcribe
    // 2, which supports only 13 languages — Danish is not among them, so
    // Danish speech failed or hung. Voxtral 3 covers 50+ languages.
    void transcriptionModelSupportsDanish()
    {
        QCOMPARE(MistralAPI::transcriptionModelId(),
                 QString("voxtral-mini-transcribe-3-0"));
    }

    // F18 (v0.8.12): transcribeAudio must reject an empty file fast.
    void transcriptionRejectsEmptyFile()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(transcriptionFailed(QString)));

        QString emptyPath = QDir::temp().filePath("skrynja-test-empty.wav");
        QFile f(emptyPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        api.transcribeAudio("fake-key", emptyPath);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(),
                 QString("The recording is empty"));
        QFile::remove(emptyPath);
    }

    // F18 (v0.8.12): transcribeAudio must reject a truncated/invalid WAV
    // (known Sailfish QAudioRecorder failure mode) before hitting the network.
    void transcriptionRejectsInvalidWav()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(transcriptionFailed(QString)));

        QString badPath = QDir::temp().filePath("skrynja-test-bad.wav");
        QFile f(badPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a wav file, just garbage bytes");
        f.close();

        api.transcribeAudio("fake-key", badPath);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("could not be saved"));
        QFile::remove(badPath);
    }

    // F18 (v0.8.12): transcribeAudio must reject a missing file.
    void transcriptionRejectsMissingFile()
    {
        MistralAPI api;
        QSignalSpy spy(&api, SIGNAL(transcriptionFailed(QString)));

        api.transcribeAudio("fake-key",
                            "/nonexistent/skrynja-test-missing.wav");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(),
                 QString("Could not read the recording file"));
    }

    // F17: parseRemoteConversations handles the bare-array top-level shape
    // that the live API returns (not wrapped in {"data": [...]}).
    void parseRemoteConversationsHandlesBareArray()
    {
        const QJsonDocument doc = QJsonDocument::fromJson(
                    "[{\"id\":\"conv-aaa\",\"name\":\"First\","
                    "\"model\":\"mistral-small-latest\","
                    "\"created_at\":\"2026-01-01T10:00:00Z\","
                    "\"updated_at\":\"2026-01-01T11:00:00Z\"},"
                    "{\"id\":\"conv-bbb\",\"name\":\"Second\","
                    "\"model\":\"mistral-large-latest\","
                    "\"created_at\":\"2026-01-02T10:00:00Z\","
                    "\"updated_at\":\"2026-01-02T11:00:00Z\"}]");
        QVERIFY(doc.isArray());

        QJsonObject wrapper;
        wrapper["data"] = doc.array();
        const QVariantList result = MistralAPI::parseRemoteConversations(wrapper);
        QCOMPARE(result.count(), 2);
        QCOMPARE(result.at(0).toMap()["id"].toString(), QString("conv-aaa"));
        QCOMPARE(result.at(0).toMap()["name"].toString(), QString("First"));
        QCOMPARE(result.at(1).toMap()["id"].toString(), QString("conv-bbb"));
        QCOMPARE(result.at(1).toMap()["name"].toString(), QString("Second"));
    }

    // F17: parseRemoteConversations extracts id, name, model, timestamps
    // from a GET /v1/conversations response (SDK shape: bare array wrapped
    // in {"data": [...]}; live API may return a bare array directly).
    void parseRemoteConversationsExtractsFields()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"data\":[{\"id\":\"conv-1\",\"name\":\"My chat\","
                    "\"model\":\"mistral-small-latest\","
                    "\"created_at\":\"2026-01-01T10:00:00Z\","
                    "\"updated_at\":\"2026-01-02T11:00:00Z\"},"
                    "{\"id\":\"conv-2\",\"name\":\"\","
                    "\"model\":\"mistral-large-latest\","
                    "\"created_at\":\"2026-01-03T12:00:00Z\","
                    "\"updated_at\":\"2026-01-03T12:00:00Z\"}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteConversations(doc);
        QCOMPARE(result.count(), 2);

        const QVariantMap first = result.at(0).toMap();
        QCOMPARE(first["id"].toString(), QString("conv-1"));
        QCOMPARE(first["name"].toString(), QString("My chat"));
        QCOMPARE(first["model"].toString(), QString("mistral-small-latest"));
        QCOMPARE(first["created_at"].toString(), QString("2026-01-01T10:00:00Z"));

        const QVariantMap second = result.at(1).toMap();
        QCOMPARE(second["id"].toString(), QString("conv-2"));
        QCOMPARE(second["name"].toString(), QString());
    }

    // F17: parseRemoteConversations handles agent conversations (agent_id
    // instead of model).
    void parseRemoteConversationsHandlesAgentShape()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"data\":[{\"id\":\"conv-agent\","
                    "\"agent_id\":\"ag_123\","
                    "\"name\":\"Agent chat\","
                    "\"created_at\":\"2026-01-01T10:00:00Z\","
                    "\"updated_at\":\"2026-01-01T11:00:00Z\"}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteConversations(doc);
        QCOMPARE(result.count(), 1);
        QCOMPARE(result.at(0).toMap()["id"].toString(), QString("conv-agent"));
        QCOMPARE(result.at(0).toMap()["name"].toString(), QString("Agent chat"));
    }

    // F17: parseRemoteConversations skips entries without an id.
    void parseRemoteConversationsSkipsEmptyIds()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"data\":[{\"name\":\"no id\"},"
                    "{\"id\":\"valid\",\"name\":\"ok\"}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteConversations(doc);
        QCOMPARE(result.count(), 1);
        QCOMPARE(result.at(0).toMap()["id"].toString(), QString("valid"));
    }

    // F17: parseRemoteConversations deduplicates entries with the same id
    // (the API can return duplicates when pagination overlaps).
    void parseRemoteConversationsDeduplicates()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"data\":["
                    "{\"id\":\"conv-a\",\"name\":\"First\","
                    "\"model\":\"m1\",\"created_at\":\"2026-01-01T10:00:00Z\"},"
                    "{\"id\":\"conv-b\",\"name\":\"Second\","
                    "\"model\":\"m2\",\"created_at\":\"2026-01-02T10:00:00Z\"},"
                    "{\"id\":\"conv-a\",\"name\":\"First dup\","
                    "\"model\":\"m1\",\"created_at\":\"2026-01-01T10:00:00Z\"},"
                    "{\"id\":\"conv-a\",\"name\":\"First dup2\","
                    "\"model\":\"m1\",\"created_at\":\"2026-01-01T10:00:00Z\"}"
                    "]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteConversations(doc);
        QCOMPARE(result.count(), 2);
        QCOMPARE(result.at(0).toMap()["id"].toString(), QString("conv-a"));
        QCOMPARE(result.at(0).toMap()["name"].toString(), QString("First"));
        QCOMPARE(result.at(1).toMap()["id"].toString(), QString("conv-b"));
        QCOMPARE(result.at(1).toMap()["name"].toString(), QString("Second"));
    }

    // F17: parseRemoteMessages extracts role, content and timestamp from
    // a GET /v1/conversations/{id}/messages response.
    void parseRemoteMessagesExtractsFields()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"object\":\"list\",\"data\":["
                    "{\"role\":\"user\",\"content\":\"Hello\","
                    "\"created_at\":\"2026-01-01T10:00:00Z\"},"
                    "{\"role\":\"assistant\",\"content\":\"Hi there\","
                    "\"created_at\":\"2026-01-01T10:00:05Z\"}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteMessages(doc);
        QCOMPARE(result.count(), 2);

        const QVariantMap user = result.at(0).toMap();
        QCOMPARE(user["role"].toString(), QString("user"));
        QCOMPARE(user["content"].toString(), QString("Hello"));
        QVERIFY(user["timestamp"].toLongLong() > 0);

        const QVariantMap assistant = result.at(1).toMap();
        QCOMPARE(assistant["role"].toString(), QString("assistant"));
        QCOMPARE(assistant["content"].toString(), QString("Hi there"));
    }

    // F17: parseRemoteMessages handles content as a chunk list (image turns).
    void parseRemoteMessagesHandlesChunkList()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"data\":["
                    "{\"role\":\"user\","
                    "\"content\":[{\"type\":\"text\",\"text\":\"describe this\"},"
                    "{\"type\":\"image_url\",\"image_url\":\"...\"}],"
                    "\"created_at\":\"2026-01-01T10:00:00Z\"}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteMessages(doc);
        QCOMPARE(result.count(), 1);
        QCOMPARE(result.at(0).toMap()["content"].toString(),
                 QString("describe this"));
    }

    // F17: parseRemoteMessages skips non-user/assistant roles (tool, system).
    void parseRemoteMessagesSkipsNonChatRoles()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"data\":["
                    "{\"role\":\"tool\",\"content\":\"result\"},"
                    "{\"role\":\"system\",\"content\":\"prompt\"},"
                    "{\"role\":\"user\",\"content\":\"hello\"}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteMessages(doc);
        QCOMPARE(result.count(), 1);
        QCOMPARE(result.at(0).toMap()["role"].toString(), QString("user"));
    }

    // F17: parseRemoteMessages reads the typed entry shape (message.input /
    // message.output) that the live API returns.
    void parseRemoteMessagesHandlesTypedEntries()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"conversation_id\":\"conv-1\","
                    "\"messages\":["
                    "{\"object\":\"entry\",\"type\":\"message.input\","
                    "\"role\":\"user\",\"content\":\"What is 2+2?\","
                    "\"created_at\":\"2026-01-01T10:00:00Z\"},"
                    "{\"object\":\"entry\",\"type\":\"message.output\","
                    "\"role\":\"assistant\",\"content\":\"4\","
                    "\"model\":\"mistral-small-latest\","
                    "\"created_at\":\"2026-01-01T10:00:05Z\"}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteMessages(doc);
        QCOMPARE(result.count(), 2);

        QCOMPARE(result.at(0).toMap()["role"].toString(), QString("user"));
        QCOMPARE(result.at(0).toMap()["content"].toString(), QString("What is 2+2?"));

        QCOMPARE(result.at(1).toMap()["role"].toString(), QString("assistant"));
        QCOMPARE(result.at(1).toMap()["content"].toString(), QString("4"));
        QCOMPARE(result.at(1).toMap()["model"].toString(),
                 QString("mistral-small-latest"));
    }

    // F17: parseRemoteMessages handles assistant content as a chunk list
    // (thinking + text) in the typed entry shape.
    void parseRemoteMessagesHandlesChunkContent()
    {
        const QJsonObject doc = QJsonDocument::fromJson(
                    "{\"messages\":["
                    "{\"type\":\"message.output\",\"role\":\"assistant\","
                    "\"content\":["
                    "{\"type\":\"thinking\",\"thinking\":[{\"type\":\"text\",\"text\":\"hmm\"}]},"
                    "{\"type\":\"text\",\"text\":\"The answer\"}"
                    "]}]}"
                    ).object();

        const QVariantList result = MistralAPI::parseRemoteMessages(doc);
        QCOMPARE(result.count(), 1);
        QCOMPARE(result.at(0).toMap()["content"].toString(),
                 QString("The answer"));
    }

    // F17: messageContentToText handles both string and chunk-list content.
    void messageContentToTextHandlesBothShapes()
    {
        QCOMPARE(MistralAPI::messageContentToText(QJsonValue("plain string")),
                 QString("plain string"));

        const QJsonArray chunks = QJsonDocument::fromJson(
                    "[{\"type\":\"text\",\"text\":\"part one\"},"
                    "{\"type\":\"image_url\",\"image_url\":\"...\"},"
                    "{\"type\":\"text\",\"text\":\" part two\"}]"
                    ).array();
        QCOMPARE(MistralAPI::messageContentToText(QJsonValue(chunks)),
                 QString("part one part two"));

        QCOMPARE(MistralAPI::messageContentToText(QJsonValue()),
                 QString());
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
        api.processStreamData("event: conversation.response.started\n"
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
        QCOMPARE(manager.pendingUserImageFileId(), QString());

        manager.currentConversation()->addUserMessage("question", "/tmp/img.jpg");
        QCOMPARE(manager.pendingUserContent(), QString("question"));
        QCOMPARE(manager.pendingUserImagePath(), QString("/tmp/img.jpg"));
        QCOMPARE(manager.pendingUserImageFileId(), QString());

        // After an upload the file id is visible to the caller
        manager.setLastUserImageFileId("remote-file-1");
        QCOMPARE(manager.pendingUserImageFileId(), QString("remote-file-1"));

        // The assistant turn that follows does not disturb the accessors:
        // they look at the last user message, not the last message.
        manager.currentConversation()->addAssistantMessage("");
        QCOMPARE(manager.pendingUserContent(), QString("question"));
        QCOMPARE(manager.pendingUserImageFileId(), QString("remote-file-1"));
    }

    // F14: the file id survives the save + reload cycle, so a retry or a
    // later turn references the same server-side file.
    void imageFileIdSurvivesReload()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        const QString id = manager.currentConversationId();
        manager.currentConversation()->addUserMessage(
                    "describe this", "/tmp/photo.jpg", "file-id-42");
        manager.saveCurrentConversation();

        manager.createNewConversation();
        manager.loadConversation(id);

        QCOMPARE(manager.pendingUserImageFileId(), QString("file-id-42"));
        QCOMPARE(manager.currentConversation()->getLastUserImagePath(),
                 QString("/tmp/photo.jpg"));
    }

    void conversationOverridesRoundTrip()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QString id = manager.currentConversationId();
        QVariantMap empty = manager.getConversationOverrides(id);
        QCOMPARE(empty["systemPrompt"].toString(), QString());

        manager.setConversationOverrides(id, " talk like a pirate ");
        manager.currentConversation()->addUserMessage("ahoy");
        manager.saveCurrentConversation();

        // Survives a switch away and back
        manager.createNewConversation();
        manager.loadConversation(id);

        QVariantMap overrides = manager.getConversationOverrides(id);
        QCOMPARE(overrides["systemPrompt"].toString(), QString("talk like a pirate"));
    }

    void modelIsFixedAtCreation()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        // Created with an explicit model
        manager.createNewConversation("mistral-large-latest");
        const QString id = manager.currentConversationId();
        QCOMPARE(manager.conversationModel(id), QString("mistral-large-latest"));

        // setConversationOverrides must not change it
        manager.setConversationOverrides(id, "you are a pirate");
        QCOMPARE(manager.conversationModel(id), QString("mistral-large-latest"));

        // A second conversation gets its own model.
        manager.createNewConversation("mistral-small-latest");
        const QString id2 = manager.currentConversationId();
        QCOMPARE(manager.conversationModel(id2), QString("mistral-small-latest"));

        // Survives a save + reload cycle
        manager.saveCurrentConversation();
        manager.loadConversation(id);
        QCOMPARE(manager.conversationModel(id), QString("mistral-large-latest"));

        // A conversation created with no model argument reports empty: the
        // caller is expected to resolve the global default before the first
        // turn and store it on the conversation.
        manager.createNewConversation();
        QCOMPARE(manager.conversationModel(manager.currentConversationId()),
                 QString());
    }

    void listedConversationsCarryNoReservedRole()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        manager.currentConversation()->addUserMessage("hello");
        manager.saveCurrentConversation();
        manager.setConversationOverrides(manager.currentConversationId(),
                                         QString());

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

    // F17: attaching a remote conversation creates a local one that keeps
    // the remote id, so the next turn appends to it.
    void attachRemoteConversationKeepsRemoteId()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QVERIFY(!manager.hasRemoteConversation("remote-abc"));

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "Hvad er hovedstaden i Danmark?";
        m1["timestamp"] = 1000LL;
        messages.append(m1);
        QVariantMap m2;
        m2["role"] = "assistant";
        m2["content"] = "København.";
        m2["timestamp"] = 2000LL;
        messages.append(m2);

        const QString localId = manager.attachRemoteConversation(
                    "remote-abc", "Hovedstad", "mistral-small-latest", messages);
        QVERIFY(!localId.isEmpty());
        QVERIFY(manager.hasRemoteConversation("remote-abc"));
        QCOMPARE(manager.remoteConversationId(), QString("remote-abc"));
        // The manager starts with one empty conversation; attaching adds one.
        QCOMPARE(manager.conversationCount(), 2);

        // The messages landed in the model.
        QCOMPARE(manager.currentConversation()->rowCount(), 2);

        // The title comes from the argument.
        QCOMPARE(manager.getConversationsList().at(0).toObject()["title"].toString(),
                 QString("Hovedstad"));

        // The model is recorded on the assistant message.
        QCOMPARE(manager.currentConversation()->data(
                    manager.currentConversation()->index(1, 0),
                    ConversationModel::ModelRole).toString(),
                 QString("mistral-small-latest"));
    }

    // F17: attaching the same remote id twice is rejected.
    void attachRemoteConversationRejectsDuplicate()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "hello";
        m1["timestamp"] = 1000LL;
        messages.append(m1);

        QVERIFY(!manager.attachRemoteConversation(
                    "remote-dup", "First", "model", messages).isEmpty());
        const int afterFirst = manager.conversationCount();
        QCOMPARE(manager.attachRemoteConversation(
                    "remote-dup", "Second", "model", messages), QString());
        QCOMPARE(manager.conversationCount(), afterFirst);
    }

    // F17: attaching with an empty remote id or no messages fails.
    void attachRemoteConversationRejectsEmpty()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "hello";
        m1["timestamp"] = 1000LL;
        messages.append(m1);

        QCOMPARE(manager.attachRemoteConversation(
                    "", "No id", "model", messages), QString());
        QCOMPARE(manager.attachRemoteConversation(
                    "remote-empty", "No messages", "model", QVariantList()),
                 QString());
    }

    // F17: after attaching, the conversation survives a reload and the
    // remote id is still there for the next append.
    void attachRemoteConversationSurvivesReload()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "first question";
        m1["timestamp"] = 1000LL;
        messages.append(m1);
        QVariantMap m2;
        m2["role"] = "assistant";
        m2["content"] = "first answer";
        m2["timestamp"] = 2000LL;
        messages.append(m2);

        const QString localId = manager.attachRemoteConversation(
                    "remote-persist", "Persisted", "mistral-small-latest", messages);
        QVERIFY(!localId.isEmpty());

        // Simulate app restart: a fresh manager loads from disk.
        ConversationManager reopened;
        QVERIFY(reopened.hasRemoteConversation("remote-persist"));
        reopened.loadConversation(localId);
        QCOMPARE(reopened.remoteConversationId(), QString("remote-persist"));
        QCOMPARE(reopened.currentConversation()->rowCount(), 2);
    }

    // F17: sources from tool executions in the remote history are preserved.
    void attachRemoteConversationKeepsSources()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "search for news";
        m1["timestamp"] = 1000LL;
        messages.append(m1);
        QVariantMap m2;
        m2["role"] = "assistant";
        m2["content"] = "Here are the news.";
        m2["timestamp"] = 2000LL;
        m2["sources"] = QStringList() << "https://example.com/a"
                                      << "https://example.com/b";
        messages.append(m2);

        const QString localId = manager.attachRemoteConversation(
                    "remote-sources", "With sources", "model", messages);
        QVERIFY(!localId.isEmpty());

        const QStringList sources = manager.currentConversation()->data(
                    manager.currentConversation()->index(1, 0),
                    ConversationModel::SourcesRole).toStringList();
        QCOMPARE(sources.count(), 2);
        QCOMPARE(sources.at(0), QString("https://example.com/a"));
    }

    // F17: when the remote conversation has no name, the title is derived
    // from the first user message.
    void attachRemoteConversationDerivesTitleFromMessage()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "Hvordan laver jeg surdej?";
        m1["timestamp"] = 1000LL;
        messages.append(m1);
        QVariantMap m2;
        m2["role"] = "assistant";
        m2["content"] = "Her er opskriften...";
        m2["timestamp"] = 2000LL;
        messages.append(m2);

        const QString localId = manager.attachRemoteConversation(
                    "remote-notitle", "", "mistral-small-latest", messages);
        QVERIFY(!localId.isEmpty());

        const QString title = manager.getConversationsList().at(0).toObject()["title"].toString();
        QCOMPARE(title, QString("Hvordan laver jeg surdej?"));
    }

    // F17: when the title argument equals the model name (a UI fallback),
    // it is replaced by the first user message.
    void attachRemoteConversationIgnoresModelAsTitle()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "Fortæl mig om bier";
        m1["timestamp"] = 1000LL;
        messages.append(m1);

        const QString localId = manager.attachRemoteConversation(
                    "remote-modeltitle", "mistral-small-latest",
                    "mistral-small-latest", messages);
        QVERIFY(!localId.isEmpty());

        const QString title = manager.getConversationsList().at(0).toObject()["title"].toString();
        QCOMPARE(title, QString("Fortæl mig om bier"));
    }

    // F17: an attached conversation gets updatedAt = now, so it appears at
    // the top of the history regardless of the original message timestamps.
    void attachRemoteConversationSetsUpdatedAtToNow()
    {
        ConversationManager manager;
        manager.purgeAllConversations();

        const qint64 before = QDateTime::currentMSecsSinceEpoch();

        QVariantList messages;
        QVariantMap m1;
        m1["role"] = "user";
        m1["content"] = "old question from 2020";
        // 2020-01-01 in epoch ms — far in the past.
        m1["timestamp"] = 1577836800000LL;
        messages.append(m1);

        const QString localId = manager.attachRemoteConversation(
                    "remote-old", "Old conversation", "model", messages);
        QVERIFY(!localId.isEmpty());

        const qint64 updatedAt = manager.getConversationsList().at(0)
                .toObject()["updatedAt"].toVariant().toLongLong();
        QVERIFY2(updatedAt >= before,
                 qPrintable(QString("updatedAt %1 should be >= %2")
                            .arg(updatedAt).arg(before)));
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

        QSettings raw(skrynjaSettingsPath("Skrynja"), QSettings::NativeFormat);
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
            QSettings raw(skrynjaSettingsPath("Skrynja"), QSettings::NativeFormat);
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
        QSettings raw(skrynjaSettingsPath("Skrynja"), QSettings::NativeFormat);
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

    // F18: Danish must be a selectable interface language, and it must map
    // to a translation file that exists.
    void danishIsAnAvailableLanguage()
    {
        SettingsManager settings;
        const QStringList languages = settings.availableLanguages();
        QVERIFY(languages.contains(QString("da")));

        settings.setLanguage("da");
        QCOMPARE(settings.language(), QString("da"));

        // The spoken-language hint derived from the interface language is
        // a plain ISO 639-1 code the transcription API accepts.
        QCOMPARE(settings.language().left(2), QString("da"));

        settings.setLanguage("en");
    }

    void webSearchTogglePersists()
    {
        {
            SettingsManager settings;
            QCOMPARE(settings.webSearch(), false); // default
            settings.setWebSearch(true);
            QCOMPARE(settings.webSearch(), true);
        }
        // Reread from disk: the toggle survives a reload
        {
            SettingsManager reopened;
            QCOMPARE(reopened.webSearch(), true);
            reopened.setWebSearch(false);
        }
        {
            SettingsManager reopened;
            QCOMPARE(reopened.webSearch(), false);
        }
    }

    // F18 anonymization: ON BY DEFAULT since v0.9.7. The toggle persists
    // across reloads, and a stored value (including an explicit "off") is
    // always respected.
    void voiceAnonymizationSettingsPersist()
    {
        {
            SettingsManager settings;
            QCOMPARE(settings.voiceAnonymization(), true);   // default ON
            settings.setVoiceAnonymization(false);          // user opts out
            QCOMPARE(settings.voiceAnonymization(), false);
        }
        // Reread from disk: the explicit "off" survives a reload
        {
            SettingsManager reopened;
            QCOMPARE(reopened.voiceAnonymization(), false);
            reopened.setVoiceAnonymization(true);
        }
        {
            SettingsManager reopened;
            QCOMPARE(reopened.voiceAnonymization(), true);
        }
    }

    // F18 anonymization: legacy keys from the experimental strength presets
    // (v0.9.1 "voice/pitchFactor", v0.9.2-v0.9.6 "voice/anonymizationStrength")
    // are removed on load, and anonymization still defaults to on.
    void legacyVoiceKeysAreRemoved()
    {
        {
            // Simulate a 0.9.x install: write the old keys directly.
            QSettings raw(skrynjaSettingsPath("Skrynja"), QSettings::NativeFormat);
            raw.setValue("voice/pitchFactor", 1.28);
            raw.setValue("voice/anonymizationStrength", "strong");
            raw.sync();
        }
        {
            SettingsManager settings;
            // Fresh install semantics: no "voice/anonymization" stored -> ON
            QCOMPARE(settings.voiceAnonymization(), true);
        }
        {
            QSettings raw(skrynjaSettingsPath("Skrynja"), QSettings::NativeFormat);
            // The old keys must be gone after the load.
            QVERIFY(!raw.contains("voice/pitchFactor"));
            QVERIFY(!raw.contains("voice/anonymizationStrength"));
            raw.remove("voice/anonymization");
            raw.sync();
        }
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

// F18 voice anonymization (experimental test branch).
class TestVoiceAnonymizer : public QObject
{
    Q_OBJECT

private:
    // Build a tiny valid 16-bit mono PCM WAV in memory and write it to disk.
    static QString writeTestWav(const QString &path, int sampleCount)
    {
        QByteArray pcm;
        pcm.reserve(sampleCount * 2);
        for (int i = 0; i < sampleCount; ++i) {
            // A simple ramp so interpolation has something to bite on.
            const qint16 v = static_cast<qint16>((i % 100) * 100 - 5000);
            pcm.append(static_cast<char>(v & 0xFF));
            pcm.append(static_cast<char>((v >> 8) & 0xFF));
        }
        QByteArray wav;
        wav.append("RIFF");
        auto u32 = [&wav](quint32 v) {
            wav.append(static_cast<char>(v & 0xFF));
            wav.append(static_cast<char>((v >> 8) & 0xFF));
            wav.append(static_cast<char>((v >> 16) & 0xFF));
            wav.append(static_cast<char>((v >> 24) & 0xFF));
        };
        auto u16 = [&wav](quint16 v) {
            wav.append(static_cast<char>(v & 0xFF));
            wav.append(static_cast<char>((v >> 8) & 0xFF));
        };
        u32(36 + pcm.size());
        wav.append("WAVE");
        wav.append("fmt ");
        u32(16);
        u16(1);      // PCM
        u16(1);      // mono
        u32(16000);  // sample rate
        u32(32000);  // byte rate
        u16(2);      // block align
        u16(16);     // bits per sample
        wav.append("data");
        u32(pcm.size());
        wav.append(pcm);

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return QString();
        }
        f.write(wav);
        f.close();
        return path;
    }

private slots:
    // Pitch factor 1.0 is a no-op: the PCM payload comes back unchanged.
    void pitchFactorOneIsNoOp()
    {
        QByteArray pcm;
        for (int i = 0; i < 1000; ++i) {
            const qint16 v = static_cast<qint16>(i * 7 - 3000);
            pcm.append(static_cast<char>(v & 0xFF));
            pcm.append(static_cast<char>((v >> 8) & 0xFF));
        }
        QCOMPARE(VoiceAnonymizer::shiftPitch(pcm, 1.0), pcm);
    }

    // Pitching up (factor > 1) shortens the sample count.
    void pitchUpShortensAudio()
    {
        QByteArray pcm;
        for (int i = 0; i < 16000; ++i) {
            const qint16 v = static_cast<qint16>(qSin(i / 50.0) * 10000);
            pcm.append(static_cast<char>(v & 0xFF));
            pcm.append(static_cast<char>((v >> 8) & 0xFF));
        }
        QCOMPARE(pcm.size(), 32000);  // 16000 samples × 2 bytes
        const QByteArray up = VoiceAnonymizer::shiftPitch(pcm, 1.25);
        // 16000 samples / 1.25 = 12800 samples = 25600 bytes
        QCOMPARE(up.size(), 25600);
    }

    // Pitching down (factor < 1) lengthens the sample count.
    void pitchDownLengthensAudio()
    {
        QByteArray pcm;
        for (int i = 0; i < 16000; ++i) {
            const qint16 v = static_cast<qint16>(qSin(i / 50.0) * 10000);
            pcm.append(static_cast<char>(v & 0xFF));
            pcm.append(static_cast<char>((v >> 8) & 0xFF));
        }
        QCOMPARE(pcm.size(), 32000);
        const QByteArray down = VoiceAnonymizer::shiftPitch(pcm, 0.8);
        // 16000 / 0.8 = 20000 samples = 40000 bytes
        QCOMPARE(down.size(), 40000);
    }

    // Noise level 0 is a no-op.
    void noiseZeroIsNoOp()
    {
        QByteArray pcm;
        for (int i = 0; i < 500; ++i) {
            pcm.append(static_cast<char>(i & 0xFF));
            pcm.append(static_cast<char>((i >> 8) & 0xFF));
        }
        QCOMPARE(VoiceAnonymizer::addNoise(pcm, 0.0), pcm);
    }

    // Noise changes the samples but keeps the length.
    void noiseChangesSamplesKeepsLength()
    {
        QByteArray pcm;
        for (int i = 0; i < 2000; ++i) {
            const qint16 v = static_cast<qint16>(i);
            pcm.append(static_cast<char>(v & 0xFF));
            pcm.append(static_cast<char>((v >> 8) & 0xFF));
        }
        QCOMPARE(pcm.size(), 4000);  // 2000 samples × 2 bytes
        const QByteArray noisy = VoiceAnonymizer::addNoise(pcm, 0.01);
        QCOMPARE(noisy.size(), pcm.size());
        QVERIFY(noisy != pcm);
    }

    // Full round-trip: WAV in → anonymized WAV out → valid WAV, shorter.
    void anonymizeRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString in = dir.filePath("in.wav");
        const QString out = dir.filePath("out.wav");
        QVERIFY(!writeTestWav(in, 16000).isEmpty());

        VoiceAnonymizer anonymizer;
        QString err;
        const bool ok = anonymizer.anonymize(in, out, 1.25, 0.005, &err);
        if (!ok) {
            QFAIL(qPrintable(QString("anonymize failed: ") + err));
        }
        QVERIFY(QFile::exists(out));

        // Output must be a valid WAV with a data chunk.
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray wav = f.readAll();
        f.close();
        QVERIFY(wav.size() > 44);
        QVERIFY(wav.startsWith("RIFF"));
        QVERIFY(wav.mid(8, 4) == "WAVE");
        const quint32 dataSize = (static_cast<quint8>(wav[40]))
                               | (static_cast<quint32>(static_cast<quint8>(wav[41])) << 8)
                               | (static_cast<quint32>(static_cast<quint8>(wav[42])) << 16)
                               | (static_cast<quint32>(static_cast<quint8>(wav[43])) << 24);
        // 16000 samples pitched up by 1.25 → 12800 samples → 25600 bytes
        // (noise keeps the length unchanged)
        QCOMPARE(dataSize, 25600u);
        QCOMPARE(wav.size(), 44 + 25600);
    }

    // Garbage input is rejected with a clear error, not a crash.
    void anonymizeRejectsGarbage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString bad = dir.filePath("bad.wav");
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a wav file at all");
        f.close();

        VoiceAnonymizer anonymizer;
        QString err;
        QVERIFY(!anonymizer.anonymize(bad, dir.filePath("out.wav"),
                                      1.15, 0.005, &err));
        QVERIFY(!err.isEmpty());
    }

    // A missing input file is rejected.
    void anonymizeRejectsMissingFile()
    {
        VoiceAnonymizer anonymizer;
        QString err;
        QVERIFY(!anonymizer.anonymize("/nonexistent/in.wav",
                                      "/nonexistent/out.wav",
                                      1.15, 0.005, &err));
        QVERIFY(!err.isEmpty());
    }

    // The fixed anonymization pipeline (the only level since v0.9.7):
    // output is a valid WAV, shorter than the input (pitch ~1.24 resamples
    // 16000 samples down to ~12903), and clearly different from the input.
    // Pitch is jittered ±3% per recording (anti-voiceprint), so exact
    // lengths vary — assert a range instead of an exact size.
    void anonymizeFileRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString in = dir.filePath("in.wav");
        QVERIFY(!writeTestWav(in, 16000).isEmpty());

        VoiceAnonymizer anonymizer;
        const QString out = dir.filePath("out.wav");
        QCOMPARE(anonymizer.anonymizeFile(in, out), QString());
        QVERIFY(QFile::exists(out));

        auto dataSize = [](const QString &path) -> quint32 {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) return 0;
            const QByteArray wav = f.readAll();
            f.close();
            if (wav.size() < 44 || !wav.startsWith("RIFF")) return 0;
            return (static_cast<quint8>(wav[40]))
                 | (static_cast<quint32>(static_cast<quint8>(wav[41])) << 8)
                 | (static_cast<quint32>(static_cast<quint8>(wav[42])) << 16)
                 | (static_cast<quint32>(static_cast<quint8>(wav[43])) << 24);
        };
        // Pitch in [1.203, 1.277] → 12518..13304 samples → 25036..26608 bytes
        const quint32 size = dataSize(out);
        QVERIFY(size >= 25036u && size <= 26608u);
        // The pipeline must actually change the audio.
        QFile fIn(in), fOut(out);
        QVERIFY(fIn.open(QIODevice::ReadOnly));
        QVERIFY(fOut.open(QIODevice::ReadOnly));
        QVERIFY(fIn.readAll() != fOut.readAll());
    }

    // Anti-voiceprint: every anonymized recording must use a slightly
    // different transformation (jittered pitch), so no two files share
    // the same parameters and averaging/inversion across recordings fails.
    void anonymizeFileJitterDiffersPerRecording()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString in = dir.filePath("in.wav");
        QVERIFY(!writeTestWav(in, 16000).isEmpty());

        auto dataSize = [](const QString &path) -> quint32 {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) return 0;
            const QByteArray wav = f.readAll();
            f.close();
            if (wav.size() < 44) return 0;
            return (static_cast<quint8>(wav[40]))
                 | (static_cast<quint32>(static_cast<quint8>(wav[41])) << 8)
                 | (static_cast<quint32>(static_cast<quint8>(wav[42])) << 16)
                 | (static_cast<quint32>(static_cast<quint8>(wav[43])) << 24);
        };

        VoiceAnonymizer anonymizer;
        QSet<quint32> sizes;
        for (int i = 0; i < 3; ++i) {
            const QString out = dir.filePath(QString("out%1.wav").arg(i));
            QCOMPARE(anonymizer.anonymizeFile(in, out), QString());
            sizes.insert(dataSize(out));
        }
        // Pitch jitter is ±3% → lengths span hundreds of samples; three
        // recordings landing on the exact same length is practically
        // impossible if jitter works.
        QVERIFY(sizes.count() >= 2);
    }

    // Spectral tilt with 0 is a no-op.
    void spectralTiltZeroIsNoOp()
    {
        QByteArray pcm;
        for (int i = 0; i < 500; ++i) {
            pcm.append(static_cast<char>(i & 0xFF));
            pcm.append(static_cast<char>((i >> 8) & 0xFF));
        }
        QCOMPARE(VoiceAnonymizer::spectralTilt(pcm, 16000, 0.0), pcm);
    }

    // A negative spectral tilt low-passes: a high-frequency tone is
    // attenuated while a low-frequency tone passes mostly unchanged.
    // Length is always preserved.
    void spectralTiltAttenuatesHighFrequencies()
    {
        auto rms = [](const QByteArray &pcm) -> double {
            const int count = pcm.size() / 2;
            double acc = 0.0;
            for (int i = 0; i < count; ++i) {
                const qint16 s = static_cast<qint16>(
                            static_cast<quint8>(pcm[i * 2])
                            | (static_cast<quint16>(static_cast<quint8>(pcm[i * 2 + 1])) << 8));
                acc += s * s;
            }
            return qSqrt(acc / qMax(1, count));
        };
        auto sine = [](double freq, quint32 sampleRate, int count) {
            QByteArray pcm;
            pcm.reserve(count * 2);
            for (int i = 0; i < count; ++i) {
                const qint16 v = static_cast<qint16>(
                            qSin(2.0 * M_PI * freq * i / sampleRate) * 10000);
                pcm.append(static_cast<char>(v & 0xFF));
                pcm.append(static_cast<char>((v >> 8) & 0xFF));
            }
            return pcm;
        };

        // Low tone (200 Hz) — inside the voice fundamental range, passes.
        const QByteArray low = sine(200.0, 16000, 16000);
        const QByteArray lowOut = VoiceAnonymizer::spectralTilt(low, 16000, -0.35);
        QCOMPARE(lowOut.size(), low.size());
        QVERIFY(rms(lowOut) > rms(low) * 0.50);

        // High tone (6 kHz) — attenuated by the low-pass tilt.
        const QByteArray high = sine(6000.0, 16000, 16000);
        const QByteArray highOut = VoiceAnonymizer::spectralTilt(high, 16000, -0.35);
        QCOMPARE(highOut.size(), high.size());
        QVERIFY(rms(highOut) < rms(high) * 0.50);

        // Silence stays silent.
        QByteArray silence(4000, '\0');
        QCOMPARE(VoiceAnonymizer::spectralTilt(silence, 16000, -0.35), silence);
    }

    // Telephone band limiting: silence stays silent, length is preserved,
    // and an out-of-band tone (6 kHz at 16 kHz sample rate) is attenuated
    // while an in-band tone (1 kHz) passes mostly unchanged.
    void bandLimitAttenuatesOutOfBandTone()
    {
        auto rms = [](const QByteArray &pcm) -> double {
            const int count = pcm.size() / 2;
            double acc = 0.0;
            for (int i = 0; i < count; ++i) {
                const qint16 s = static_cast<qint16>(
                            static_cast<quint8>(pcm[i * 2])
                            | (static_cast<quint16>(static_cast<quint8>(pcm[i * 2 + 1])) << 8));
                acc += s * s;
            }
            return qSqrt(acc / qMax(1, count));
        };
        auto sine = [](double freq, quint32 sampleRate, int count) {
            QByteArray pcm;
            pcm.reserve(count * 2);
            for (int i = 0; i < count; ++i) {
                const qint16 v = static_cast<qint16>(
                            qSin(2.0 * M_PI * freq * i / sampleRate) * 10000);
                pcm.append(static_cast<char>(v & 0xFF));
                pcm.append(static_cast<char>((v >> 8) & 0xFF));
            }
            return pcm;
        };

        // Out-of-band tone (6 kHz > 3400 Hz) must be strongly attenuated.
        const QByteArray high = sine(6000.0, 16000, 16000);
        const QByteArray highOut = VoiceAnonymizer::bandLimit(high, 16000);
        QCOMPARE(highOut.size(), high.size());
        QVERIFY(rms(highOut) < rms(high) * 0.10);   // > 20 dB down

        // In-band tone (1 kHz) passes with only mild attenuation.
        const QByteArray mid = sine(1000.0, 16000, 16000);
        const QByteArray midOut = VoiceAnonymizer::bandLimit(mid, 16000);
        QCOMPARE(midOut.size(), mid.size());
        QVERIFY(rms(midOut) > rms(mid) * 0.70);    // less than 3 dB down

        // Silence stays silent; degenerate input is a no-op.
        QByteArray silence(4000, '\0');
        QCOMPARE(VoiceAnonymizer::bandLimit(silence, 16000), silence);
        QCOMPARE(VoiceAnonymizer::bandLimit(QByteArray("ab"), 16000),
                 QByteArray("ab"));
        QCOMPARE(VoiceAnonymizer::bandLimit(high, 0), high);
    }

    // F18b: the QML-facing wrapper returns "" on success and a non-empty
    // error string on failure. This is the method ChatPage.qml actually
    // calls — anonymize() itself takes a QString* which QML cannot pass.
    void anonymizeFileWrapperReturnsEmptyOnSuccess()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString in = dir.filePath("in.wav");
        const QString out = dir.filePath("out.wav");
        QVERIFY(!writeTestWav(in, 16000).isEmpty());

        VoiceAnonymizer anonymizer;
        const QString err = anonymizer.anonymizeFile(in, out);
        QCOMPARE(err, QString());
        QVERIFY(QFile::exists(out));
    }

    void anonymizeFileWrapperReturnsErrorOnFailure()
    {
        VoiceAnonymizer anonymizer;
        const QString err = anonymizer.anonymizeFile("/nonexistent/in.wav",
                                                     "/nonexistent/out.wav");
        QVERIFY(!err.isEmpty());
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

class TestAgentTools : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // A throwaway project tree under a temp dir.
        m_root.reset(new QTemporaryDir());
        QVERIFY(m_root->isValid());
        const QString base = m_root->path();
        QDir dir(base);
        QVERIFY(dir.mkpath("src"));
        QVERIFY(dir.mkpath("qml/pages"));

        writeFile(base + "/src/main.cpp", "int main() { return 0; }\n");
        writeFile(base + "/src/util.cpp", "int helper() { return 42; }\n");
        writeFile(base + "/qml/pages/ChatPage.qml",
                  "import Sailfish.Silica 1.0\nPage { id: page }\n");
        writeFile(base + "/README.md", "# Test project\n");
    }

    void toolSchemasHaveExpectedNames()
    {
        const QJsonArray tools = AgentTools::toolSchemas(false);
        QStringList names;
        for (const QJsonValue &v : tools) {
            names.append(v.toObject()["function"].toObject()["name"].toString());
        }
        QVERIFY(names.contains("read_file"));
        QVERIFY(names.contains("list_files"));
        QVERIFY(names.contains("search_code"));
        QVERIFY(names.contains("edit_file"));
        // run_command is opt-in
        QVERIFY(!names.contains("run_command"));

        const QJsonArray withRun = AgentTools::toolSchemas(true);
        QStringList namesWithRun;
        for (const QJsonValue &v : withRun) {
            namesWithRun.append(v.toObject()["function"].toObject()["name"].toString());
        }
        QVERIFY(namesWithRun.contains("run_command"));
    }

    void toolSchemasAreWellFormed()
    {
        const QJsonArray tools = AgentTools::toolSchemas(true);
        for (const QJsonValue &v : tools) {
            const QJsonObject tool = v.toObject();
            QCOMPARE(tool["type"].toString(), QString("function"));
            const QJsonObject function = tool["function"].toObject();
            QVERIFY(!function["name"].toString().isEmpty());
            QVERIFY(!function["description"].toString().isEmpty());
            const QJsonObject params = function["parameters"].toObject();
            QCOMPARE(params["type"].toString(), QString("object"));
            QVERIFY(params.contains("properties"));
            QVERIFY(params.contains("required"));
        }
    }

    void pathEscapeIsRejected()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        // Absolute path
        QVERIFY(tools.resolvePath("/etc/passwd").isEmpty());
        // .. traversal
        QVERIFY(tools.resolvePath("../outside.txt").isEmpty());
        QVERIFY(tools.resolvePath("src/../../outside.txt").isEmpty());
        // A valid relative path resolves under the root
        const QString resolved = tools.resolvePath("src/main.cpp");
        QVERIFY(!resolved.isEmpty());
        QVERIFY(resolved.startsWith(m_root->path()));
    }

    void noProjectRootDisablesTools()
    {
        AgentTools tools;
        // No root set: every call errors out
        const QString result = tools.execute("read_file",
                                             "{\"path\":\"src/main.cpp\"}");
        QVERIFY(result.contains("error"));
    }

    void readFileReturnsContent()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute("read_file",
                                             "{\"path\":\"src/main.cpp\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QCOMPARE(obj["ok"].toBool(), true);
        QVERIFY(obj["content"].toString().contains("int main()"));
        QCOMPARE(obj["lines"].toInt(), 1);
    }

    void readFileRejectsEscape()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute("read_file",
                                             "{\"path\":\"../../etc/passwd\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QVERIFY(obj.contains("error"));
        QVERIFY(!obj["ok"].toBool(true));
    }

    void listFilesFindsProjectFiles()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute("list_files", "{}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QCOMPARE(obj["ok"].toBool(), true);
        const QJsonArray entries = obj["entries"].toArray();
        QStringList paths;
        for (const QJsonValue &v : entries) {
            paths.append(v.toString());
        }
        QVERIFY(paths.contains("src/main.cpp"));
        QVERIFY(paths.contains("qml/pages/ChatPage.qml"));
        QVERIFY(paths.contains("README.md"));
    }

    void listFilesWithPattern()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute(
                    "list_files", "{\"pattern\":\"*.qml\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        const QJsonArray entries = obj["entries"].toArray();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.at(0).toString(), QString("qml/pages/ChatPage.qml"));
    }

    void searchCodeFindsMatches()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute("search_code",
                                             "{\"query\":\"helper\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QCOMPARE(obj["ok"].toBool(), true);
        const QJsonArray matches = obj["matches"].toArray();
        QVERIFY(matches.size() >= 1);
        QVERIFY(matches.at(0).toString().contains("src/util.cpp"));
        QVERIFY(matches.at(0).toString().contains("int helper()"));
    }

    void searchCodeInSubdirectory()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute(
                    "search_code", "{\"query\":\"main\",\"path\":\"src\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        const QJsonArray matches = obj["matches"].toArray();
        QVERIFY(matches.size() >= 1);
        for (const QJsonValue &v : matches) {
            QVERIFY(v.toString().startsWith("src/"));
        }
    }

    void editFileAppliesUniqueReplacement()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute(
                    "edit_file",
                    "{\"path\":\"src/util.cpp\","
                    "\"old_text\":\"return 42;\","
                    "\"new_text\":\"return 43;\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QCOMPARE(obj["ok"].toBool(), true);
        QCOMPARE(obj["replacements"].toInt(), 1);

        // The file on disk changed
        QFile file(m_root->path() + "/src/util.cpp");
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(file.readAll());
        file.close();
        QVERIFY(content.contains("return 43;"));
        QVERIFY(!content.contains("return 42;"));
    }

    void editFileRejectsMissingOldText()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute(
                    "edit_file",
                    "{\"path\":\"src/util.cpp\","
                    "\"old_text\":\"this does not exist\","
                    "\"new_text\":\"x\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QVERIFY(obj.contains("error"));
    }

    void editFileRejectsAmbiguousOldText()
    {
        // Write a file where the target text appears twice
        writeFile(m_root->path() + "/src/dup.cpp", "foo\nfoo\n");
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute(
                    "edit_file",
                    "{\"path\":\"src/dup.cpp\","
                    "\"old_text\":\"foo\","
                    "\"new_text\":\"bar\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QVERIFY(obj.contains("error"));
        QVERIFY(obj["error"].toString().contains("multiple"));
    }

    void runCommandIsRefusedWhenDisabled()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        tools.setRunCommandAllowed(false);
        const QString result = tools.execute("run_command",
                                             "{\"command\":\"ls\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QVERIFY(obj.contains("error"));
        QVERIFY(obj["error"].toString().contains("disabled"));
    }

    void runCommandExecutesWhenAllowed()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        tools.setRunCommandAllowed(true);
        // "echo" is available everywhere; QProcess splits on whitespace.
        const QString result = tools.execute("run_command",
                                             "{\"command\":\"/bin/echo hello\"}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QCOMPARE(obj["ok"].toBool(), true);
        QCOMPARE(obj["exitCode"].toInt(), 0);
        QVERIFY(obj["output"].toString().contains("hello"));
    }

    void unknownToolReturnsError()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute("delete_everything", "{}");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QVERIFY(obj.contains("error"));
        QVERIFY(obj["error"].toString().contains("Unknown tool"));
    }

    void invalidArgumentsJsonReturnsError()
    {
        AgentTools tools;
        tools.setProjectRootForTest(m_root->path());
        const QString result = tools.execute("read_file", "not json");
        const QJsonObject obj = QJsonDocument::fromJson(result.toUtf8()).object();
        QVERIFY(obj.contains("error"));
    }

private:
    void writeFile(const QString &path, const QString &content)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(content.toUtf8());
        file.close();
    }

    QScopedPointer<QTemporaryDir> m_root;
};

class TestAgentFunctionCalling : public QObject
{
    Q_OBJECT

private slots:
    void functionResultsBodyHasCorrectShape()
    {
        QVariantList results;
        QVariantMap r1;
        r1["tool_call_id"] = "call_1";
        r1["result"] = "{\"ok\":true,\"content\":\"hello\"}";
        results.append(r1);
        QVariantMap r2;
        r2["tool_call_id"] = "call_2";
        r2["result"] = "{\"error\":\"not found\"}";
        results.append(r2);

        const QByteArray body = MistralAPI::buildFunctionResultsBody(results);
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QVERIFY(doc.isObject());
        const QJsonObject obj = doc.object();
        QCOMPARE(obj["stream"].toBool(), true);
        const QJsonArray inputs = obj["inputs"].toArray();
        QCOMPARE(inputs.size(), 2);

        const QJsonObject first = inputs.at(0).toObject();
        QCOMPARE(first["object"].toString(), QString("entry"));
        QCOMPARE(first["type"].toString(), QString("function.result"));
        QCOMPARE(first["tool_call_id"].toString(), QString("call_1"));
        QCOMPARE(first["result"].toString(),
                 QString("{\"ok\":true,\"content\":\"hello\"}"));

        const QJsonObject second = inputs.at(1).toObject();
        QCOMPARE(second["tool_call_id"].toString(), QString("call_2"));
    }

    void turnBodyIncludesAgentToolsAtCreate()
    {
        MistralAPI api;
        QJsonArray agentTools;
        QJsonObject readFileTool;
        readFileTool["type"] = "function";
        QJsonObject readFileFn;
        readFileFn["name"] = "read_file";
        readFileTool["function"] = readFileFn;
        agentTools.append(readFileTool);

        const QByteArray body = api.buildTurnBody(
                    "mistral-small-latest", "Be helpful", "hi", "",
                    true, true, -1.0, 0, QString(), agentTools);
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        const QJsonArray tools = obj["tools"].toArray();
        // web_search + read_file
        QCOMPARE(tools.size(), 2);
        QCOMPARE(tools.at(0).toObject()["type"].toString(), QString("web_search"));
        QCOMPARE(tools.at(1).toObject()["type"].toString(), QString("function"));
        QCOMPARE(tools.at(1).toObject()["function"].toObject()["name"].toString(),
                 QString("read_file"));
    }

    void turnBodyOmitsToolsOnAppend()
    {
        // Tools are create-time only: an append (isCreate=false) must not
        // carry them even when agent tools are passed.
        MistralAPI api;
        QJsonArray agentTools;
        QJsonObject tool;
        tool["type"] = "function";
        agentTools.append(tool);

        const QByteArray body = api.buildTurnBody(
                    "mistral-small-latest", "", "hi", "",
                    false, false, -1.0, 0, QString(), agentTools);
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        QVERIFY(!obj.contains("tools"));
        QVERIFY(!obj.contains("model"));
        QVERIFY(!obj.contains("instructions"));
    }

    void functionCallDeltaReassembly()
    {
        // The model sends arguments as fragments across several deltas.
        // parseStreamLine accumulates them by tool_call_id.
        MistralAPI api;
        QSignalSpy spy(&api, &MistralAPI::functionCallRequested);

        // Simulate: first delta opens the call, second carries a fragment,
        // conversation.response.done flushes.
        api.parseStreamLine("function.call.delta",
            "{\"type\":\"function.call.delta\",\"id\":\"fc_1\","
            "\"tool_call_id\":\"call_abc\",\"name\":\"read_file\","
            "\"arguments\":\"{\\\"pa\"}");
        api.parseStreamLine("function.call.delta",
            "{\"type\":\"function.call.delta\",\"id\":\"fc_1\","
            "\"tool_call_id\":\"call_abc\",\"name\":\"read_file\","
            "\"arguments\":\"th\\\":\\\"src/main.cpp\\\"}\"}");
        api.parseStreamLine("conversation.response.done",
            "{\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":5}}");

        QCOMPARE(spy.count(), 1);
        const QVariantList calls = spy.at(0).at(0).toList();
        QCOMPARE(calls.size(), 1);
        const QVariantMap call = calls.at(0).toMap();
        QCOMPARE(call["tool_call_id"].toString(), QString("call_abc"));
        QCOMPARE(call["name"].toString(), QString("read_file"));
        const QVariantMap args = call["arguments"].toMap();
        QCOMPARE(args["path"].toString(), QString("src/main.cpp"));
    }

    void functionCallDeltaMultipleCalls()
    {
        // Parallel tool calls: two tool_call_ids in one response.
        MistralAPI api;
        QSignalSpy spy(&api, &MistralAPI::functionCallRequested);

        api.parseStreamLine("function.call.delta",
            "{\"tool_call_id\":\"c1\",\"name\":\"read_file\","
            "\"arguments\":\"{\\\"path\\\":\\\"a.cpp\\\"}\"}");
        api.parseStreamLine("function.call.delta",
            "{\"tool_call_id\":\"c2\",\"name\":\"search_code\","
            "\"arguments\":\"{\\\"query\\\":\\\"foo\\\"}\"}");
        api.parseStreamLine("conversation.response.done", "{}");

        QCOMPARE(spy.count(), 1);
        const QVariantList calls = spy.at(0).at(0).toList();
        QCOMPARE(calls.size(), 2);
    }

    void dataOnlyFunctionCallDelta()
    {
        // Some servers send data-only SSE (no "event:" line); the payload
        // self-describes via its "type" field.
        MistralAPI api;
        QSignalSpy spy(&api, &MistralAPI::functionCallRequested);

        api.parseStreamLine("",
            "{\"type\":\"function.call.delta\",\"tool_call_id\":\"c1\","
            "\"name\":\"list_files\",\"arguments\":\"{}\"}");
        api.parseStreamLine("conversation.response.done", "{}");

        QCOMPARE(spy.count(), 1);
        const QVariantList calls = spy.at(0).at(0).toList();
        QCOMPARE(calls.size(), 1);
        QCOMPARE(calls.at(0).toMap()["name"].toString(), QString("list_files"));
    }

    void noFunctionCallsMeansNoSignal()
    {
        // A plain text answer: no function.call.delta, no signal.
        MistralAPI api;
        QSignalSpy spy(&api, &MistralAPI::functionCallRequested);
        api.parseStreamLine("message.output.delta",
                            "{\"content\":\"Hello\"}");
        api.parseStreamLine("conversation.response.done", "{}");
        QCOMPARE(spy.count(), 0);
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
    app.setOrganizationName("org.skrynja");
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
        TestVoiceAnonymizer t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestProviders t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestAgentTools t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestAgentFunctionCalling t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "tst_main.moc"
