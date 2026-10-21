#ifndef MISTRALAPI_H
#define MISTRALAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QVariant>

// Talks to Mistral's Conversations API (/v1/conversations), the only Mistral
// API that offers the web_search tool. The API is stateful: the first message
// of a conversation creates it (POST /v1/conversations#stream) and carries
// the model, the system prompt (instructions) and the tools; later messages
// only append (POST /v1/conversations/{id}#stream). The class keeps its name
// because the app started Mistral-only.
class MistralAPI : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit MistralAPI(QObject *parent = nullptr);

    bool isBusy() const;
    QString error() const;

    // Sends one user turn. An empty remoteConversationId creates the remote
    // conversation; a set one appends to it. When imageFileId is set the image
    // was uploaded to Mistral's Files API and the turn references it with a
    // file chunk; otherwise imagePath is inlined as a base64 image chunk.
    // temperature < 0 and maxTokens <= 0 are omitted so the API applies its
    // own defaults.
    Q_INVOKABLE void sendMessage(const QString &apiKey,
                                 const QString &modelName,
                                 const QString &systemPrompt,
                                 const QString &userContent,
                                 const QString &imagePath,
                                 bool webSearch,
                                 const QString &remoteConversationId,
                                 double temperature = -1.0,
                                 int maxTokens = 0,
                                 const QString &imageFileId = QString());
    // targetId is echoed back with the result so a caller can tell which
    // conversation the title belongs to. Runs on the Conversations API with
    // store=false: nothing is persisted server-side.
    Q_INVOKABLE void generateTitle(const QString &apiKey,
                                     const QString &modelName,
                                     const QString &conversationText,
                                     const QString &targetId = QString());
    Q_INVOKABLE void fetchModels(const QString &apiKey);
    // Uploads an image to Mistral's Files API (POST /v1/files, multipart)
    // so the turn can reference it by file id instead of carrying base64.
    // Emits imageUploaded(fileId) or imageUploadFailed().
    Q_INVOKABLE void uploadImage(const QString &apiKey,
                                 const QString &imagePath);
    Q_INVOKABLE void cancelRequest();
    Q_INVOKABLE void clearError();

signals:
    void isBusyChanged();
    void errorChanged();
    void streamingResponse(const QString &content);
    // Reasoning models send a "thinking" chunk alongside the text. Emitted
    // separately so the UI can show it collapsed, not as part of the answer.
    void thinkingResponse(const QString &thinking);
    void usageReceived(int promptTokens, int completionTokens);
    // Tokens spent by an auxiliary call (title generation): billed, but not
    // part of the conversation the token banner reports on.
    void sideRequestUsage(int promptTokens, int completionTokens);
    void responseCompleted();
    // Emitted when the remote answer ended without a done event (cancel,
    // error): a partial answer may already be on screen.
    void responseIncomplete();
    void messageSent();
    // Carries the conversation_id returned when a remote conversation is
    // created, so the caller can persist it and append later.
    void conversationCreated(const QString &conversationId);
    // A server-side tool (web_search) started: the UI can show activity.
    void toolActivity(const QString &toolName);
    // Sources cited by web_search, collected from tool.execution events.
    // Emitted once per response, before responseCompleted.
    void sourcesReceived(const QStringList &sources);
    void titleGenerated(const QString &title, const QString &category,
                        const QString &targetId);
    void titleGenerationFailed(const QString &targetId);
    void modelsFetched(const QVariantList &models);
    void modelsFetchFailed();
    // A file upload finished: the id to reference in the next turn.
    void imageUploaded(const QString &fileId);
    void imageUploadFailed();
    // The provider turned the request away (429/503) and it will be sent
    // again: the answer is still coming, so this is not an error yet.
    void rateLimited(int seconds, int attempt, int maxAttempts);

private slots:
    void onReadyRead();
    void onFinished();
    void onError(QNetworkReply::NetworkError error);
    void onTitleGenerationFinished();
    void onModelsFetchFinished();
    void onReconcileFinished();
    void onImageUploadFinished();
    void onTimeout();
    void sendPendingChatRequest();

private:
    friend class TestMistralAPI;
    friend class TestConversationManager;

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    QTimer *m_timeoutTimer;
    // Holds a chat request back, either to space it out or to retry it.
    QTimer *m_requestTimer;
    // Since the last request actually left, whatever its kind.
    QElapsedTimer m_lastRequestAt;
    bool m_isBusy;
    bool m_timedOut;
    QString m_error;
    QByteArray m_streamBuffer;
    // Name of the SSE event currently being assembled ("event: <name>").
    QString m_currentEvent;
    QString m_baseUrl;
    bool m_keyRequired;
    int m_minRequestIntervalMs;
    int m_retryCount;
    // Content already reached the screen: resending would duplicate it.
    bool m_streamStarted;
    // Set when a conversation.response.done event arrived: the answer is
    // complete, as opposed to cancelled or cut off.
    bool m_streamDone;
    // The chat request as sent, kept in memory only, so a rate-limited call
    // can go out again without the UI having to know it ever failed.
    QString m_pendingApiKey;
    QByteArray m_pendingBody;
    // Empty for a create, set for an append: append is not idempotent, so a
    // retry after an ambiguous failure must first reconcile (GET messages).
    QString m_pendingRemoteId;
    // The user turn as sent, for the reconcile comparison.
    QString m_pendingUserContent;
    // The remote file id sent with the turn, empty when the image went out
    // inlined as base64 (or there was no image).
    QString m_pendingImageFileId;
    // Last conversation_id seen on the wire, used to build the reconcile URL.
    QString m_remoteConversationId;
    // A reconcile GET is in flight: the chat reply is parked meanwhile.
    bool m_reconciling;
    QNetworkReply *m_reconcileReply;
    // URLs cited by web_search during this response, deduplicated, in order
    // of first appearance. Emitted once when the response completes.
    QStringList m_pendingSources;
    // Accumulated "thinking" chunks from a reasoning model, emitted at the
    // end so the UI can show the reasoning collapsed under the answer.
    QString m_pendingThinking;

    void setIsBusy(bool busy);
    void setError(const QString &error);
    // Endpoint URL, or an invalid one when no base URL is configured. The
    // path may contain %23: "#stream" is a URL fragment, and QNetworkAccessManager
    // would strip a literal '#' before sending, so it must stay encoded.
    QUrl endpoint(const QString &path) const;
    // Content type, bearer token when there is one, TLS pinning.
    void prepareRequest(QNetworkRequest &request, const QString &apiKey) const;
    void processStreamData(const QByteArray &data);
    // One SSE line: tracks the event name, or parses a data payload.
    void handleSseLine(const QString &line);
    // One typed event: conversation.created, message.output.delta,
    // conversation.response.done, tool.execution.*, error.
    void parseStreamLine(const QString &event, const QString &jsonData);
    // True while the same request is still worth sending again.
    bool shouldRetry(int httpStatus) const;
    // Milliseconds to wait before a request may leave, 0 when the gate is open.
    int throttleDelayMs() const;
    void noteRequestSent();
    // Sends the stored chat request, after extraDelayMs and the throttle.
    void queueChatRequest(int extraDelayMs);
    void forgetPendingRequest();
    // Builds the create/append request body and the URL path for a turn.
    QByteArray buildTurnBody(const QString &modelName,
                             const QString &systemPrompt,
                             const QString &userContent,
                             const QString &imagePath,
                             bool webSearch,
                             bool isCreate,
                             double temperature,
                             int maxTokens,
                             const QString &imageFileId = QString()) const;
    QString turnPath(const QString &remoteConversationId) const;
    // Starts the reconcile GET before an append is retried.
    void startReconcile();
    // True when the remote history already contains the pending user turn.
    static bool remoteHasUserMessage(const QJsonObject &messagesResponse,
                                     const QString &userContent);
    // Extracts the assistant text from a non-streaming outputs array.
    static QString outputsToText(const QJsonArray &outputs);
};

#endif // MISTRALAPI_H
