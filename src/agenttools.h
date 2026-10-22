#ifndef AGENTTOOLS_H
#define AGENTTOOLS_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>

// Local function tools for agent mode (F19): the model calls read_file,
// list_files, search_code, edit_file or run_command through Mistral's
// Conversations API function calling, and this class executes them on the
// device, scoped to a user-picked project root. Pure Qt, no Silica, so the
// unit tests cover it on plain Ubuntu.
class AgentTools : public QObject
{
    Q_OBJECT
public:
    explicit AgentTools(QObject *parent = nullptr);

    // Canonical project root. Empty disables the tools: execute() returns an
    // error for every call.
    void setProjectRoot(const QString &path);
    QString projectRoot() const { return m_projectRoot; }

    // Whether run_command is allowed. When false the tool is not declared in
    // the schema the model sees, and execute() refuses it even if called.
    void setRunCommandAllowed(bool allowed);
    bool runCommandAllowed() const { return m_runCommandAllowed; }

    // Dispatches one function call. name is the function name, argumentsJson
    // the raw JSON arguments string sent by the model. Returns a JSON string
    // ({"ok":true,...} or {"error":"..."}), never throws.
    Q_INVOKABLE QString execute(const QString &name, const QString &argumentsJson);

    // The "tools" array entries for the conversation create body. Includes
    // run_command only when allowed.
    static QJsonArray toolSchemas(bool runCommandAllowed);

    // Resolves a relative path against the project root and returns the
    // canonical absolute path, or an empty string when the path escapes the
    // root (absolute paths, ".." traversal, symlink escape). Pure: does not
    // touch the filesystem beyond canonicalization.
    QString resolvePath(const QString &relativePath) const;

    // Test hook: inject the canonical root without touching the filesystem.
    void setProjectRootForTest(const QString &canonicalPath);

private:
    QString m_projectRoot;
    bool m_runCommandAllowed;

    QString readFile(const QJsonObject &args, QJsonObject &result) const;
    QString listFiles(const QJsonObject &args, QJsonObject &result) const;
    QString searchCode(const QJsonObject &args, QJsonObject &result) const;
    QString editFile(const QJsonObject &args, QJsonObject &result) const;
    QString runCommand(const QJsonObject &args, QJsonObject &result) const;

    static QString errorResult(const QString &message);
    static QString okResult(const QJsonObject &data);
};

#endif // AGENTTOOLS_H
