#ifndef CONVERSATIONMANAGER_H
#define CONVERSATIONMANAGER_H

#include <QObject>
#include <QSet>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QVariant>
#include <QStringList>
#include "conversationmodel.h"

// QSettings path under SailJail: AppConfigLocation points to the sandboxed
// ~/.config/<OrganizationName>/<ApplicationName>/ directory. The two-argument
// QSettings constructor would use an inaccessible path outside the sandbox.
QString skrynjaSettingsPath(const QString &fileName);

class MistralAPI;
class AgentTools;

struct Conversation {
    QString id;
    QString title;
    QString category;
    QString provider;       // always "mistral" (kept for the stored format)
    QString model;          // model this conversation was created with
    QString systemPrompt;   // per-conversation system prompt, empty = global
    // Remote conversation id from Mistral's Conversations API. Empty until
    // the first turn has been answered server-side; migration (D8) means old
    // conversations start fresh remotely.
    QString remoteConversationId;
    // True when the remote conversation no longer exists on Mistral's
    // servers (deleted on another device, or expired). The local copy is
    // kept and shown as archived; the user can delete it permanently.
    bool archived = false;
    bool unread = false;    // an answer landed while the user was elsewhere
    qint64 createdAt;
    qint64 updatedAt;
    qint64 totalTokens = 0;
    QList<Message> messages;
};

class ConversationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConversationModel* currentConversation READ currentConversation NOTIFY currentConversationChanged)
    Q_PROPERTY(int conversationCount READ conversationCount NOTIFY conversationCountChanged)
    // Conversation currently receiving a streamed answer, empty when idle.
    // Lets the history show which one is still being written to.
    Q_PROPERTY(QString streamingConversationId READ streamingConversationId NOTIFY streamingConversationIdChanged)
public:
    explicit ConversationManager(QObject *parent = nullptr);

    ConversationModel* currentConversation() const { return m_currentConversation; }
    int conversationCount() const { return m_conversations.count(); }
    QString streamingConversationId() const { return m_streamingConversationId; }

    // The model is chosen here and fixed for the life of the conversation:
    // the Conversations API locks it at creation time.
    Q_INVOKABLE void createNewConversation(const QString &model = QString());
    Q_INVOKABLE void loadConversation(const QString &conversationId);
    Q_INVOKABLE void deleteConversation(const QString &conversationId);
    // F7: Deletes a conversation both locally and on Mistral's servers (when
    // the conversation has a remote id and an API key is available). The
    // remote deletion is fire-and-forget: the local copy is removed
    // immediately regardless of the server outcome.
    Q_INVOKABLE void deleteConversationWithRemote(const QString &conversationId,
                                                  const QString &apiKey);
    Q_INVOKABLE void renameConversation(const QString &conversationId, const QString &newTitle);
    Q_INVOKABLE void updateCurrentConversationTitle(const QString &newTitle);
    Q_INVOKABLE void updateCurrentConversationCategory(const QString &category);
    Q_INVOKABLE void setConversationCategory(const QString &conversationId, const QString &category);
    // Title and category without asking a model: the first question makes the
    // title, the local keyword classifier makes the category. Costs nothing,
    // which is the whole point when the provider counts requests.
    Q_INVOKABLE void autoLabelConversation(const QString &conversationId);
    Q_INVOKABLE void saveCurrentConversation();
    Q_INVOKABLE QJsonArray getConversationsList() const;
    Q_INVOKABLE QVariant getConversationDetails(const QString &conversationId) const;
    Q_INVOKABLE qint64 getStorageSize() const;
    Q_INVOKABLE QString getStorageSizeFormatted() const;
    Q_INVOKABLE void purgeAllConversations();
    // F7: like purgeAllConversations but also deletes every remote
    // conversation on Mistral's servers first.
    Q_INVOKABLE void purgeAllConversationsWithRemote(const QString &apiKey);
    // Deletes only archived conversations (those whose remote counterpart
    // no longer exists on the server). Local-only conversations are kept.
    Q_INVOKABLE int deleteAllArchived(const QString &apiKey);
    // True when the conversation is archived (remote copy gone).
    Q_INVOKABLE bool isArchived(const QString &conversationId) const;
    // Clears the archived flag and the remote id, so the next message creates
    // a fresh server-side conversation.
    Q_INVOKABLE void unarchiveConversation(const QString &conversationId);
    Q_INVOKABLE QVariantMap getStatistics() const;
    Q_INVOKABLE QVariantMap getConversationStatistics(const QString &conversationId) const;
    Q_INVOKABLE QVariantMap getFunStats() const;
    Q_INVOKABLE QVariantList searchConversations(const QString &query) const;
    Q_INVOKABLE void addTokenUsage(int promptTokens, int completionTokens,
                                   const QString &model = QString());
    Q_INVOKABLE QString currentConversationId() const;
    // Clears the "an answer arrived" badge. The chat page calls this for the
    // conversation it is showing.
    Q_INVOKABLE void markConversationRead(const QString &conversationId);
    Q_INVOKABLE QVariantList getPinnedMessages() const;
    Q_INVOKABLE QString conversationToMarkdown(const QString &conversationId) const;
    // Condensed transcript handed to the model when asking for a title.
    Q_INVOKABLE QString conversationDigest(const QString &conversationId) const;
    Q_INVOKABLE QString exportConversation(const QString &conversationId) const;

    // Per-conversation overrides. The model is not one: it is fixed at
    // creation (see createNewConversation). Only the system prompt can
    // still be overridden per conversation.
    Q_INVOKABLE QVariantMap getConversationOverrides(const QString &conversationId) const;
    Q_INVOKABLE void setConversationOverrides(const QString &conversationId,
                                              const QString &systemPrompt);
    // The model this conversation was created with, empty before creation.
    Q_INVOKABLE QString conversationModel(const QString &conversationId) const;

    // The remote conversation id of the current conversation, empty when the
    // remote conversation has not been created yet. Pass it to
    // MistralAPI::sendMessage: empty creates, set appends.
    Q_INVOKABLE QString remoteConversationId() const;
    // The text of the last user message in the current conversation, for the
    // caller to send as the new turn.
    Q_INVOKABLE QString pendingUserContent() const;
    // The local path of the image attached to the last user message, if any.
    Q_INVOKABLE QString pendingUserImagePath() const;
    // The remote file id of the last user message's uploaded image, empty
    // when the image has not been uploaded yet (or was inlined as base64).
    Q_INVOKABLE QString pendingUserImageFileId() const;
    // Records the file id returned by MistralAPI::uploadImage on the last
    // user message and persists it, so a retry or a later turn references the
    // same server-side file instead of uploading again.
    Q_INVOKABLE void setLastUserImageFileId(const QString &fileId);

    // Re-run the local classifier over every conversation still labelled
    // "other" or unlabelled. Returns the number of conversations relabelled.
    Q_INVOKABLE int recategorizeConversations();

    // Copy a picked image into the app data directory so the conversation keeps
    // working after the original is deleted from the gallery. Returns the new
    // path, or the input unchanged on failure.
    Q_INVOKABLE QString retainImage(const QString &filePath) const;

    // Takes ownership of the streaming lifecycle: the answer is accumulated
    // and written into the model here, so it keeps arriving even when the chat
    // page is destroyed (navigating back to the history pops it).
    void bindApi(MistralAPI *api);

    // F19: agent mode. When set, the manager executes function calls locally
    // via this AgentTools instance and appends the results, looping until
    // the model answers with text or the call cap is hit.
    void setAgentTools(AgentTools *tools);
    // True when agent mode produced tool schemas for the current turn.
    bool agentModeActive() const { return m_agentModeActive; }

    // Model the in-flight request is using, recorded for the token statistics.
    Q_INVOKABLE void setActiveModel(const QString &model);

    // F17: Creates a local conversation from a remote one (already on
    // Mistral's servers). The remote id is kept so the next turn appends to
    // it. Returns the new local conversation id, or empty on failure.
    // messages is a QVariantList of {role, content, timestamp, sources?}.
    Q_INVOKABLE QString attachRemoteConversation(const QString &remoteId,
                                                 const QString &title,
                                                 const QString &model,
                                                 const QVariantList &messages);

    // True when a local conversation already references this remote id.
    Q_INVOKABLE bool hasRemoteConversation(const QString &remoteId) const;

    // F17b: Automatically attach all remote conversations that are not yet
    // attached locally. Fetches the list, then fetches messages for each
    // unattached conversation sequentially. No-op when a sync is already
    // running or the API key is empty. Emits autoSyncFinished when done.
    Q_INVOKABLE void autoSyncRemoteConversations(const QString &apiKey);
    Q_PROPERTY(bool autoSyncing READ autoSyncing NOTIFY autoSyncingChanged)
    bool autoSyncing() const { return m_autoSyncing; }

    void loadAllConversations();

signals:
    void currentConversationChanged();
    void conversationCountChanged();
    // Emitted on every throttled write of the streaming answer, so a visible
    // chat page can follow it.
    void streamingUpdated();
    void responseFinished(const QString &conversationId);
    void streamingConversationIdChanged();
    void tokenUsageChanged(const QString &conversationId,
                           int promptTokens, int completionTokens);
    // F17b: emitted when autoSyncRemoteConversations finishes (all done or
    // aborted). attachedCount is how many were newly attached.
    void autoSyncFinished(int attachedCount);
    void autoSyncingChanged();

private slots:
    void onMessageSent();
    void onStreamingResponse(const QString &content);
    void onSourcesReceived(const QStringList &sources);
    void onThinkingReceived(const QString &thinking);
    void onResponseCompleted();
    void onUsageReceived(int promptTokens, int completionTokens);
    void onSideRequestUsage(int promptTokens, int completionTokens);
    void onConversationCreated(const QString &remoteId);
    void onResponseIncomplete();
    void flushStream();
    void finishBackgroundResponse();
    // F19: a stream ended with pending function calls.
    void onFunctionCallRequested(const QVariantList &calls);
    // F17b: auto-sync slots
    void onAutoSyncConversationsFetched(const QVariantList &conversations);
    void onAutoSyncConversationsFetchFailed();
    void onAutoSyncMessagesFetched(const QString &remoteConversationId,
                                   const QVariantList &messages);
    void onAutoSyncMessagesFetchFailed(const QString &remoteConversationId);

private:
    ConversationModel *m_currentConversation;
    QString m_currentConversationId;
    QList<Conversation> m_conversations;
    QSettings m_settings;
    QSet<QString> m_dirtyIds;
    QTimer *m_streamTimer;
    QString m_streamBuffer;
    QStringList m_pendingSources;
    QString m_pendingThinking;
    QString m_activeModel;
    QString m_streamingConversationId;
    bool m_streamDirty;
    qint64 m_totalPromptTokens;
    qint64 m_totalCompletionTokens;
    // F17b: auto-sync state
    MistralAPI *m_api;
    QString m_autoSyncApiKey;
    QVariantList m_autoSyncPending;   // remote conversations not yet attached
    QVariantMap m_autoSyncNameModel;   // remoteId → {name, model}
    bool m_autoSyncing;
    int m_autoSyncAttached;
    // True when the current stream ended without a done event (cancel,
    // timeout, network drop). Applied as interrupted=true on the message
    // when the response completes.
    bool m_streamIncomplete;
    // F19: agent mode state.
    AgentTools *m_agentTools;
    bool m_agentModeActive;
    int m_agentCallCount;

    QString generateConversationId() const;
    QString generateConversationTitle(const QList<Message> &messages) const;
    // The user side of a conversation, clipped, as fed to the classifier.
    static QString userText(const Conversation &conv);

    QString conversationsDir() const;
    QString conversationFilePath(const QString &id) const;
    static bool isSafeId(const QString &id);
    QJsonObject conversationToJson(const Conversation &conv) const;
    static Conversation conversationFromJson(const QJsonObject &obj);
    void migrateFromSettings();
    void writeConversation(const Conversation &conv) const;
    void saveDirtyConversations();
    void markDirty(const QString &id);
    Conversation* findConversation(const QString &id);
    const Conversation* findConversation(const QString &id) const;
};

#endif // CONVERSATIONMANAGER_H
