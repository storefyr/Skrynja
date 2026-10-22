#include "agenttools.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QSet>
#include <QTextStream>

// Bounds so a runaway read does not blow the model's context window.
static const int MAX_READ_BYTES = 100 * 1024;
static const int MAX_READ_LINES = 2000;
static const int MAX_LIST_ENTRIES = 500;
static const int MAX_SEARCH_MATCHES = 100;
static const int MAX_COMMAND_OUTPUT = 4000;
static const int COMMAND_TIMEOUT_MS = 30000;

// Text extensions search_code descends into by default. Binary and generated
// files are skipped; the model can still read them explicitly with read_file.
static bool isSearchableFile(const QString &suffix)
{
    static const QSet<QString> extensions = {
        "cpp", "h", "c", "hpp", "cc", "qml", "js", "py", "pro", "yaml", "yml",
        "json", "md", "ts", "spec", "desktop", "sh", "txt", "cmake", "conf",
        "xml", "html", "css", "qrc", "pri", "prf"
    };
    return extensions.contains(suffix.toLower());
}

// A single JSON-schema property of type string.
static QJsonObject stringProperty(const QString &description)
{
    QJsonObject prop;
    prop["type"] = "string";
    prop["description"] = description;
    return prop;
}

// Wraps a function schema as a Conversations API tool entry.
static QJsonObject functionTool(const QString &name,
                                const QString &description,
                                const QJsonObject &properties,
                                const QJsonArray &required)
{
    QJsonObject parameters;
    parameters["type"] = "object";
    parameters["properties"] = properties;
    parameters["required"] = required;

    QJsonObject function;
    function["name"] = name;
    function["description"] = description;
    function["parameters"] = parameters;

    QJsonObject tool;
    tool["type"] = "function";
    tool["function"] = function;
    return tool;
}

AgentTools::AgentTools(QObject *parent)
    : QObject(parent)
    , m_runCommandAllowed(false)
{
}

void AgentTools::setProjectRoot(const QString &path)
{
    // Canonicalize so the prefix check in resolvePath is reliable even when
    // the picker hands over a path with symlinks or a trailing slash.
    m_projectRoot = QDir(path).canonicalPath();
}

void AgentTools::setProjectRootForTest(const QString &canonicalPath)
{
    m_projectRoot = canonicalPath;
}

void AgentTools::setRunCommandAllowed(bool allowed)
{
    m_runCommandAllowed = allowed;
}

QString AgentTools::resolvePath(const QString &relativePath) const
{
    if (m_projectRoot.isEmpty()) {
        return QString();
    }
    // Absolute paths are rejected outright: the model is told to use relative
    // paths, and accepting an absolute one would make the root check moot.
    if (QDir::isAbsolutePath(relativePath)) {
        return QString();
    }
    const QString joined = m_projectRoot + QLatin1Char('/') + relativePath;
    const QString canonical = QFileInfo(joined).canonicalFilePath();
    // canonicalFilePath is empty when the file does not exist. For reads and
    // edits that is a legitimate error, but the path itself is still safe to
    // attempt: resolve it lexically and check the prefix instead.
    QString effective = canonical;
    if (effective.isEmpty()) {
        effective = QDir(joined).canonicalPath();
    }
    if (effective.isEmpty()) {
        // The target does not exist. Lexical cleanup is enough for the prefix
        // check: ".." components are resolved by QDir::cleanPath.
        effective = QDir::cleanPath(joined);
    }
    if (effective != m_projectRoot
            && !effective.startsWith(m_projectRoot + QLatin1Char('/'))) {
        return QString();
    }
    return effective;
}

QJsonArray AgentTools::toolSchemas(bool runCommandAllowed)
{
    QJsonArray tools;

    {
        QJsonObject props;
        props["path"] = stringProperty(
            "Relative path from the project root, e.g. src/mistralapi.cpp");
        tools.append(functionTool(
            "read_file",
            "Read a file from the project. Path is relative to the project root. "
            "Returns the file content as text, truncated if very large.",
            props, QJsonArray{"path"}));
    }

    {
        QJsonObject props;
        props["path"] = stringProperty(
            "Optional subdirectory relative to the project root. Empty or omitted "
            "means the project root.");
        props["pattern"] = stringProperty(
            "Optional glob filter, e.g. *.qml. Omit to list all files.");
        tools.append(functionTool(
            "list_files",
            "List files in a directory of the project. Paths are relative to the "
            "project root. Results are capped at 500 entries.",
            props, QJsonArray()));
    }

    {
        QJsonObject props;
        props["query"] = stringProperty("Text to search for (plain substring match).");
        props["path"] = stringProperty(
            "Optional subdirectory relative to the project root. Omit to search "
            "the whole project.");
        tools.append(functionTool(
            "search_code",
            "Search for text in the project's source files. Returns matching lines "
            "as path:line: content, capped at 100 matches.",
            props, QJsonArray{"query"}));
    }

    {
        QJsonObject props;
        props["path"] = stringProperty(
            "Relative path from the project root, e.g. src/mistralapi.cpp");
        props["old_text"] = stringProperty(
            "Exact text to replace. Must match the file exactly, including "
            "whitespace and indentation. Must be unique in the file.");
        props["new_text"] = stringProperty("Replacement text.");
        tools.append(functionTool(
            "edit_file",
            "Replace an exact, unique snippet of text in a file. old_text must "
            "appear exactly once; use more context if it does not.",
            props, QJsonArray{"path", "old_text", "new_text"}));
    }

    if (runCommandAllowed) {
        QJsonObject props;
        props["command"] = stringProperty(
            "Shell command to run with the project root as working directory, "
            "e.g. \"qmake && make\" or \"git status\". No pipes to files, no "
            "background jobs. Output is truncated.");
        tools.append(functionTool(
            "run_command",
            "Run a shell command in the project directory and return its output "
            "and exit code. Use for builds, tests and git.",
            props, QJsonArray{"command"}));
    }

    return tools;
}

QString AgentTools::errorResult(const QString &message)
{
    QJsonObject obj;
    obj["error"] = message;
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString AgentTools::okResult(const QJsonObject &data)
{
    QJsonObject obj = data;
    obj["ok"] = true;
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString AgentTools::execute(const QString &name, const QString &argumentsJson)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(argumentsJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return errorResult(QString("Invalid arguments JSON: %1")
                           .arg(parseError.errorString()));
    }
    const QJsonObject args = doc.object();

    if (m_projectRoot.isEmpty()) {
        return errorResult("Agent mode is not configured: no project root set.");
    }

    QJsonObject result;
    if (name == "read_file") {
        return readFile(args, result);
    }
    if (name == "list_files") {
        return listFiles(args, result);
    }
    if (name == "search_code") {
        return searchCode(args, result);
    }
    if (name == "edit_file") {
        return editFile(args, result);
    }
    if (name == "run_command") {
        if (!m_runCommandAllowed) {
            return errorResult("run_command is disabled in settings.");
        }
        return runCommand(args, result);
    }
    return errorResult(QString("Unknown tool: %1").arg(name));
}

QString AgentTools::readFile(const QJsonObject &args, QJsonObject &result) const
{
    const QString path = resolvePath(args["path"].toString());
    if (path.isEmpty()) {
        return errorResult("Path escapes the project root or is invalid.");
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return errorResult(QString("Cannot open file: %1").arg(file.errorString()));
    }
    QTextStream in(&file);
    in.setCodec("UTF-8");
    QString content;
    int lines = 0;
    bool truncated = false;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (content.size() + line.size() + 1 > MAX_READ_BYTES || lines >= MAX_READ_LINES) {
            truncated = true;
            break;
        }
        if (!content.isEmpty()) {
            content += QLatin1Char('\n');
        }
        content += line;
        ++lines;
    }
    result["path"] = args["path"].toString();
    result["content"] = content;
    result["lines"] = lines;
    result["truncated"] = truncated;
    return okResult(result);
}

QString AgentTools::listFiles(const QJsonObject &args, QJsonObject &result) const
{
    const QString sub = args["path"].toString();
    const QString root = sub.isEmpty() ? m_projectRoot : resolvePath(sub);
    if (root.isEmpty()) {
        return errorResult("Path escapes the project root or is invalid.");
    }
    const QString pattern = args["pattern"].toString();

    QStringList nameFilters;
    if (!pattern.isEmpty()) {
        nameFilters << pattern;
    }

    QStringList entries;
    QDirIterator it(root,
                    nameFilters,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext() && entries.size() < MAX_LIST_ENTRIES) {
        const QString absolute = it.next();
        // Skip the .git directory: it is huge and never useful to the model.
        if (absolute.contains("/.git/") || absolute.endsWith("/.git")) {
            continue;
        }
        entries.append(QDir(m_projectRoot).relativeFilePath(absolute));
    }
    result["path"] = sub.isEmpty() ? QString(".") : sub;
    result["count"] = entries.size();
    result["truncated"] = it.hasNext();
    result["entries"] = QJsonArray::fromStringList(entries);
    return okResult(result);
}

QString AgentTools::searchCode(const QJsonObject &args, QJsonObject &result) const
{
    const QString query = args["query"].toString();
    if (query.isEmpty()) {
        return errorResult("Empty search query.");
    }
    const QString sub = args["path"].toString();
    const QString root = sub.isEmpty() ? m_projectRoot : resolvePath(sub);
    if (root.isEmpty()) {
        return errorResult("Path escapes the project root or is invalid.");
    }

    QStringList matches;
    QDirIterator it(root,
                    QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext() && matches.size() < MAX_SEARCH_MATCHES) {
        const QString absolute = it.next();
        if (absolute.contains("/.git/") || absolute.endsWith("/.git")) {
            continue;
        }
        if (!isSearchableFile(QFileInfo(absolute).suffix())) {
            continue;
        }
        QFile file(absolute);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        QTextStream in(&file);
        in.setCodec("UTF-8");
        const QString relative = QDir(m_projectRoot).relativeFilePath(absolute);
        int lineNumber = 0;
        while (!in.atEnd() && matches.size() < MAX_SEARCH_MATCHES) {
            const QString line = in.readLine();
            ++lineNumber;
            if (line.contains(query, Qt::CaseSensitive)) {
                matches.append(QString("%1:%2: %3")
                               .arg(relative)
                               .arg(lineNumber)
                               .arg(line.left(200)));
            }
        }
    }
    result["query"] = query;
    result["count"] = matches.size();
    result["matches"] = QJsonArray::fromStringList(matches);
    return okResult(result);
}

QString AgentTools::editFile(const QJsonObject &args, QJsonObject &result) const
{
    const QString relative = args["path"].toString();
    const QString path = resolvePath(relative);
    if (path.isEmpty()) {
        return errorResult("Path escapes the project root or is invalid.");
    }
    const QString oldText = args["old_text"].toString();
    const QString newText = args["new_text"].toString();
    if (oldText.isEmpty()) {
        return errorResult("old_text must not be empty.");
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return errorResult(QString("Cannot open file: %1").arg(file.errorString()));
    }
    const QString original = QString::fromUtf8(file.readAll());
    file.close();

    // Qt 5.6: QString::split with a QString pattern, count occurrences.
    const int firstIndex = original.indexOf(oldText);
    if (firstIndex < 0) {
        return errorResult("old_text not found in file. Read the file first to get "
                           "the exact text.");
    }
    const int secondIndex = original.indexOf(oldText, firstIndex + oldText.length());
    if (secondIndex >= 0) {
        return errorResult("old_text matches multiple locations. Include more "
                           "surrounding context to make it unique.");
    }

    QString updated = original;
    updated.replace(firstIndex, oldText.length(), newText);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return errorResult(QString("Cannot write file: %1").arg(file.errorString()));
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << updated;
    file.close();

    result["path"] = relative;
    result["replacements"] = 1;
    return okResult(result);
}

QString AgentTools::runCommand(const QJsonObject &args, QJsonObject &result) const
{
    const QString command = args["command"].toString().trimmed();
    if (command.isEmpty()) {
        return errorResult("Empty command.");
    }

    QProcess process;
    process.setWorkingDirectory(m_projectRoot);
    // QProcess::start with a single string splits on whitespace in Qt 5.6.
    // No shell is involved, so pipes, redirects and $() do not work — the
    // model is told this in the tool description.
    process.start(command);
    if (!process.waitForFinished(COMMAND_TIMEOUT_MS)) {
        process.kill();
        process.waitForFinished(2000);
        return errorResult(QString("Command timed out after %1 ms.")
                           .arg(COMMAND_TIMEOUT_MS));
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput())
            + QString::fromUtf8(process.readAllStandardError());
    if (output.size() > MAX_COMMAND_OUTPUT) {
        output = output.left(MAX_COMMAND_OUTPUT) + "\n... (truncated)";
    }
    result["exitCode"] = process.exitCode();
    result["output"] = output;
    return okResult(result);
}
