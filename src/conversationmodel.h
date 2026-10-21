#ifndef CONVERSATIONMODEL_H
#define CONVERSATIONMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>

struct Message {
    QString role;      // "user" or "assistant"
    QString content;
    QString imagePath; // local file attached to the message (vision models)
    // Remote file id from Mistral's /v1/files upload. Empty when the image
    // has not been uploaded yet (or when inline base64 was used instead).
    QString imageFileId;
    // Who answered. Recorded on assistant messages so a conversation that
    // changed provider halfway stays readable. Empty on user messages, and on
    // anything written before 2.3.
    QString model;
    QString provider;
    // URLs cited by web_search, attached to the assistant message that
    // used them. Empty for answers that did not search.
    QStringList sources;
    // Reasoning trace from a reasoning model (type "thinking" chunks).
    // Empty for non-reasoning models and user messages.
    QString thinking;
    // True when the answer was interrupted (cancelled, timeout, network
    // drop) before it completed. Shown as a subtle marker in the UI.
    bool interrupted = false;
    qint64 timestamp;
    bool pinned = false;
};

class ConversationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum MessageRoles {
        RoleRole = Qt::UserRole + 1,
        ContentRole,
        TimestampRole,
        PinnedRole,
        ImagePathRole,
        ModelRole,
        ProviderRole,
        SourcesRole,
        ThinkingRole,
        ImageFileIdRole,
        InterruptedRole
    };

    explicit ConversationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addUserMessage(const QString &content, const QString &imagePath = QString(),
                                    const QString &imageFileId = QString());
    Q_INVOKABLE void addAssistantMessage(const QString &content,
                                        const QString &model = QString(),
                                        const QString &provider = QString(),
                                        const QStringList &sources = QStringList(),
                                        const QString &thinking = QString());
    Q_INVOKABLE void updateLastAssistantMessage(const QString &content);
    // Attaches web_search source URLs to the last assistant message.
    void setLastAssistantSources(const QStringList &sources);
    // Attaches a reasoning trace to the last assistant message.
    void setLastAssistantThinking(const QString &thinking);
    // Attaches a remote file id to the last user message (vision upload).
    void setLastUserImageFileId(const QString &fileId);
    Q_INVOKABLE void removeLastMessageIfEmpty();
    Q_INVOKABLE void removeLastAssistantMessage();
    Q_INVOKABLE void truncateFrom(int index);
    Q_INVOKABLE void togglePinned(int index);
    Q_INVOKABLE void clearConversation();
    Q_INVOKABLE QString getFirstUserMessage() const;
    Q_INVOKABLE QString getLastAssistantMessage() const;
    Q_INVOKABLE QString getLastUserMessage() const;
    Q_INVOKABLE QString getLastUserImagePath() const;
    // The remote file id of the last user message's image, empty when the
    // image has not been uploaded (or was inlined as base64).
    QString getLastUserImageFileId() const;

    void addMessage(const QString &role, const QString &content, qint64 timestamp,
                    bool pinned = false, const QString &imagePath = QString(),
                    const QString &model = QString(), const QString &provider = QString(),
                    const QStringList &sources = QStringList(),
                    const QString &thinking = QString(),
                    const QString &imageFileId = QString(),
                    bool interrupted = false);
    // Marks the last assistant message as interrupted (cancelled/aborted).
    void markLastAssistantInterrupted();
    QJsonArray toJsonArray() const;

    // Read-only view used by ConversationManager to build the API payload.
    const QList<Message> &messages() const { return m_messages; }

signals:
    void countChanged();

private:
    QList<Message> m_messages;
};

#endif // CONVERSATIONMODEL_H
