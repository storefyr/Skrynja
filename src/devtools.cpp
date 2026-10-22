#include "devtools.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QDebug>

// Limits — keep answers and tool outputs small enough for the model to
// digest without blowing the context window.
static const int MAX_READ_BYTES = 200 * 1024;      // 200 KB per read
static const int MAX_WRITE_BYTES = 1024 * 1024;     // 1 MB per write
static const int MAX_DIR_ENTRIES = 500;
static const int MAX_SEARCH_MATCHES = 100;
static const int COMMAND_TIMEOUT_MS = 60 * 1000;    // 60 s

DevTools::DevTools(QObject *parent)
    : QObject(parent)
    , m_workspaceReady(false)
{
}

QString DevTools::workspaceRoot() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/workspace";
}

bool DevTools::workspaceReady() const
{
    return m_workspaceReady;
}

bool DevTools::ensureWorkspace()
{
    m_lastError.clear();
    if (m_workspaceReady) {
        return true;
    }
    const QString root = workspaceRoot();
    QDir dir(root);
    if (dir.exists() && QFileInfo::exists(root + "/harbour-skrynja.pro")) {
        m_workspaceReady = true;
        return true;
    }

    // First use: clone the repo into the workspace.
    if (!dir.mkpath(".")) {
        m_lastError = tr("Could not create the workspace directory");
        return false;
    }
    QProcess git;
    git.setWorkingDirectory(root);
    git.start("git", QStringList()
              << "clone" << "--depth" << "1"
              << "https://github.com/storefyr/Skrynja.git" << ".");
    if (!git.waitForFinished(COMMAND_TIMEOUT_MS * 2)) {
        m_lastError = tr("Cloning the repository timed out or git is not installed");
        return false;
    }
    if (git.exitCode() != 0) {
        m_lastError = tr("Cloning the repository failed: %1")
                .arg(QString::fromUtf8(git.readAllStandardError()).trimmed());
        return false;
    }
    m_workspaceReady = true;
    return true;
}

QString DevTools::lastError() const
{
    return m_lastError;
}

// ---------------------------------------------------------------------------
// Tool-call parsing
// ---------------------------------------------------------------------------

QVariantList DevTools::parseToolCalls(const QString &text)
{
    QVariantList calls;
    // <tool name="read_file"><path>src/main.cpp</path></tool>
    // Parameter tags are simple: <param>value</param> (no nesting).
    static const QRegularExpression blockRe(
                "<tool\\s+name=\"([a-z_]+)\"\\s*>(.*?)</tool>",
                QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression paramRe(
                "<([a-z_]+)>(.*?)</\\1>",
                QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatchIterator it = blockRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QVariantMap call;
        call["name"] = m.captured(1);
        QVariantMap args;
        QRegularExpressionMatchIterator pit = paramRe.globalMatch(m.captured(2));
        while (pit.hasNext()) {
            const QRegularExpressionMatch pm = pit.next();
            args[pm.captured(1)] = pm.captured(2);
        }
        call["args"] = args;
        calls.append(call);
    }
    return calls;
}

QString DevTools::stripToolCalls(const QString &text)
{
    static const QRegularExpression blockRe(
                "<tool\\s+name=\"[a-z_]+\"\\s*>.*?</tool>",
                QRegularExpression::DotMatchesEverythingOption);
    QString out = text;
    out.remove(blockRe);
    return out.trimmed();
}

// ---------------------------------------------------------------------------
// Path safety
// ---------------------------------------------------------------------------

QString DevTools::resolvePath(const QString &relPath) const
{
    if (relPath.isEmpty()) {
        return QString();
    }
    // Reject absolute paths and anything that escapes the workspace.
    if (relPath.startsWith("/") || relPath.startsWith("~")) {
        return QString();
    }
    const QString root = QDir(workspaceRoot()).canonicalPath();
    if (root.isEmpty()) {
        // Workspace does not exist (yet): nothing to resolve against.
        return QString();
    }
    // Canonical check: the resolved path must stay inside the workspace.
    // QDir::cleanPath removes "." and ".." segments lexically; the startsWith
    // test then rejects anything that climbed out of the root.
    const QString abs = QDir::cleanPath(QDir(root).absoluteFilePath(relPath));
    if (!abs.startsWith(root + "/") && abs != root) {
        return QString();
    }
    return abs;
}

// ---------------------------------------------------------------------------
// Tool implementations
// ---------------------------------------------------------------------------

QVariantMap DevTools::readFile(const QString &relPath)
{
    QVariantMap result;
    const QString abs = resolvePath(relPath);
    if (abs.isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("Invalid path (must stay inside the workspace)");
        return result;
    }
    QFile f(abs);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result["ok"] = false;
        result["error"] = tr("Could not read the file");
        return result;
    }
    const QByteArray data = f.read(MAX_READ_BYTES + 1);
    f.close();
    QString text = QString::fromUtf8(data);
    bool truncated = data.size() > MAX_READ_BYTES;
    if (truncated) {
        text = QString::fromUtf8(data.left(MAX_READ_BYTES));
    }
    result["ok"] = true;
    result["output"] = text + (truncated ? "\n... [truncated]" : "");
    return result;
}

QVariantMap DevTools::listDir(const QString &relPath)
{
    QVariantMap result;
    const QString abs = resolvePath(relPath.isEmpty() ? "." : relPath);
    if (abs.isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("Invalid path (must stay inside the workspace)");
        return result;
    }
    QDir dir(abs);
    if (!dir.exists()) {
        result["ok"] = false;
        result["error"] = tr("Directory does not exist");
        return result;
    }
    const QFileInfoList entries = dir.entryInfoList(
                QDir::Filters(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::DirsFirst),
                QDir::Name);
    QStringList lines;
    int count = 0;
    for (const QFileInfo &fi : entries) {
        if (count >= MAX_DIR_ENTRIES) {
            lines << "... [truncated]";
            break;
        }
        lines << (fi.isDir() ? fi.fileName() + "/" : fi.fileName());
        ++count;
    }
    result["ok"] = true;
    result["output"] = lines.join("\n");
    return result;
}

void DevTools::searchRecursive(const QString &dir, const QString &pattern,
                               QStringList &results, int &budget) const
{
    if (budget <= 0) return;
    QDir d(dir);
    const QFileInfoList entries = d.entryInfoList(
                QDir::Filters(QDir::AllEntries | QDir::NoDotAndDotDot), QDir::DirsFirst);
    for (const QFileInfo &fi : entries) {
        if (budget <= 0) return;
        // Skip .git and build artifacts.
        if (fi.isDir()) {
            if (fi.fileName() == ".git" || fi.fileName().startsWith("build")) {
                continue;
            }
            searchRecursive(fi.absoluteFilePath(), pattern, results, budget);
        } else if (fi.suffix() == "cpp" || fi.suffix() == "h"
                   || fi.suffix() == "qml" || fi.suffix() == "js"
                   || fi.suffix() == "pro" || fi.suffix() == "spec"
                   || fi.suffix() == "md" || fi.suffix() == "ts"
                   || fi.suffix() == "sh" || fi.suffix() == "yaml"
                   || fi.suffix() == "yml" || fi.suffix() == "json") {
            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            int lineNo = 0;
            while (!in.atEnd() && budget > 0) {
                const QString line = in.readLine();
                ++lineNo;
                if (line.contains(pattern, Qt::CaseInsensitive)) {
                    const QString rel = QDir(workspaceRoot()).relativeFilePath(
                                fi.absoluteFilePath());
                    results << QString("%1:%2: %3").arg(rel).arg(lineNo).arg(line);
                    --budget;
                }
            }
            f.close();
        }
    }
}

QVariantMap DevTools::search(const QString &pattern, const QString &relDir)
{
    QVariantMap result;
    if (pattern.isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("Empty search pattern");
        return result;
    }
    const QString abs = resolvePath(relDir.isEmpty() ? "." : relDir);
    if (abs.isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("Invalid path (must stay inside the workspace)");
        return result;
    }
    QStringList matches;
    int budget = MAX_SEARCH_MATCHES;
    searchRecursive(abs, pattern, matches, budget);
    result["ok"] = true;
    result["output"] = matches.isEmpty()
            ? tr("No matches") : matches.join("\n");
    return result;
}

QVariantMap DevTools::editFile(const QString &relPath, const QString &oldText,
                               const QString &newText)
{
    QVariantMap result;
    if (oldText.isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("The search text must not be empty");
        return result;
    }
    const QString abs = resolvePath(relPath);
    if (abs.isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("Invalid path (must stay inside the workspace)");
        return result;
    }
    QFile f(abs);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result["ok"] = false;
        result["error"] = tr("Could not read the file");
        return result;
    }
    const QString content = QString::fromUtf8(f.readAll());
    f.close();
    const int pos = content.indexOf(oldText);
    if (pos < 0) {
        result["ok"] = false;
        result["error"] = tr("Search text not found in the file");
        return result;
    }
    if (content.indexOf(oldText, pos + 1) >= 0) {
        result["ok"] = false;
        result["error"] = tr("Search text found more than once — make it more specific");
        return result;
    }
    QString updated = content;
    updated.replace(pos, oldText.length(), newText);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        result["ok"] = false;
        result["error"] = tr("Could not write the file");
        return result;
    }
    f.write(updated.toUtf8());
    f.close();
    result["ok"] = true;
    result["output"] = tr("Replaced %1 character(s) at position %2")
            .arg(oldText.length()).arg(pos);
    return result;
}

QVariantMap DevTools::writeFile(const QString &relPath, const QString &content)
{
    QVariantMap result;
    const QString abs = resolvePath(relPath);
    if (abs.isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("Invalid path (must stay inside the workspace)");
        return result;
    }
    if (content.toUtf8().size() > MAX_WRITE_BYTES) {
        result["ok"] = false;
        result["error"] = tr("Content too large (max 1 MB)");
        return result;
    }
    // Make sure the parent directory exists.
    QDir().mkpath(QFileInfo(abs).absolutePath());
    QFile f(abs);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        result["ok"] = false;
        result["error"] = tr("Could not write the file");
        return result;
    }
    f.write(content.toUtf8());
    f.close();
    result["ok"] = true;
    result["output"] = tr("Wrote %1 bytes to %2").arg(content.toUtf8().size()).arg(relPath);
    return result;
}

QVariantMap DevTools::runCommand(const QString &command)
{
    QVariantMap result;
    if (command.trimmed().isEmpty()) {
        result["ok"] = false;
        result["error"] = tr("Empty command");
        return result;
    }
    QProcess proc;
    proc.setWorkingDirectory(workspaceRoot());
    // Merge stderr into stdout so the model sees everything.
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start("/bin/sh", QStringList() << "-c" << command);
    const bool finished = proc.waitForFinished(COMMAND_TIMEOUT_MS);
    const QString output = QString::fromUtf8(proc.readAll());
    if (!finished) {
        result["ok"] = false;
        result["error"] = tr("Command timed out after 60 s");
        return result;
    }
    result["ok"] = proc.exitCode() == 0;
    const QString exitNote = QString("[exit code %1]").arg(proc.exitCode());
    result["output"] = output.isEmpty()
            ? exitNote
            : output + (output.endsWith("\n") ? "" : "\n") + exitNote;
    if (proc.exitCode() != 0) {
        result["error"] = tr("Command failed with exit code %1").arg(proc.exitCode());
    }
    return result;
}

QVariantMap DevTools::runTool(const QString &name, const QVariantMap &args)
{
    emit toolStarted(name);
    QVariantMap result;
    if (name == "read_file") {
        result = readFile(args.value("path").toString());
    } else if (name == "list_dir") {
        result = listDir(args.value("path").toString());
    } else if (name == "search") {
        result = search(args.value("pattern").toString(),
                        args.value("dir").toString());
    } else if (name == "edit_file") {
        result = editFile(args.value("path").toString(),
                          args.value("old_text").toString(),
                          args.value("new_text").toString());
    } else if (name == "write_file") {
        result = writeFile(args.value("path").toString(),
                           args.value("content").toString());
    } else if (name == "run_command") {
        result = runCommand(args.value("command").toString());
    } else {
        result["ok"] = false;
        result["error"] = tr("Unknown tool: %1").arg(name);
    }
    emit toolFinished(name, result.value("ok").toBool());
    return result;
}
