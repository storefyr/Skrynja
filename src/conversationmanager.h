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

class MistralAPI;

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

    // Model the in-flight request is using, recorded for the token statistics.
    Q_INVOKABLE void setActiveModel(const QString &model);

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

private slots:
    void onMessageSent();
    void onStreamingResponse(const QString &content);
    void onSourcesReceived(const QStringList &sources);
    void onThinkingReceived(const QString &thinking);
    void onResponseCompleted();
    void onUsageReceived(int promptTokens, int completionTokens);
    void onSideRequestUsage(int promptTokens, int completionTokens);
    void onConversationCreated(const QString &remoteId);
    void flushStream();
    void finishBackgroundResponse();

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
