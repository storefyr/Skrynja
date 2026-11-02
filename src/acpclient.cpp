#include "acpclient.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>

// Line cap: v3 §4.2 sets 66 060 288 byte (63 MiB), arithmetically derived from
// Qt's Value::MaxSize; for the first alpha a smaller cap is sufficient and
// changes nothing else (v3 §10 række 6 note). 8 MiB here.
static const qint64 kAlphaLineCap = 8LL * 1024 * 1024;

QString AcpClient::defaultAgentPath()
{
    // v3 §9.2 field 1, measured on the device 2026-10-29.
    return QStringLiteral("/home/defaultuser/.local/bin/vibe-acp");
}

qint64 AcpClient::defaultLineCap()
{
    return kAlphaLineCap;
}

AcpClient::AcpClient(QObject *parent)
    : QObject(parent), m_cap(kAlphaLineCap)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &AcpClient::onReadyRead);
    // Qt 5.6 (Sailfish) has no QOverload; use the old SIGNAL/SLOT syntax for the
    // overloaded finished() signal.
    connect(&m_process, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(onFinished(int,QProcess::ExitStatus)));
    connect(&m_process, &QProcess::errorOccurred, this, &AcpClient::onErrorOccurred);
}

AcpClient::~AcpClient()
{
    stop();
}

void AcpClient::start(const QString &agentPath, const QString &workingDir,
                      const QStringList &args)
{
    if (m_process.state() != QProcess::NotRunning)
        return;
    m_buffer.clear();
    m_counter = 0;
    m_l1Fired = false;
    m_discarding = false;
    m_process.setWorkingDirectory(workingDir);
    // v3 §9.2b: cwd is set in the spawn call, not via an agent flag.
    // v3 §9.2a: no --setup in production (field 2); args exist for tests only.
    m_process.start(agentPath, args);
    if (m_process.waitForStarted(5000))
        emit started();
}

void AcpClient::stop()
{
    if (m_process.state() == QProcess::NotRunning)
        return;
    // v3 §7.4: close stdin, wait for the agent to exit; outstanding requests
    // are not answered (C1 territory, handled by the caller).
    m_process.closeWriteChannel();
    if (!m_process.waitForFinished(5000))
        m_process.kill();
}

void AcpClient::onFinished(int exitCode, QProcess::ExitStatus status)
{
    handleTail();
    emit finished(exitCode, status);
}

void AcpClient::onErrorOccurred(QProcess::ProcessError error)
{
    // C2 territory: abnormal end is logged as 4002 (v3 §6.2); the finished
    // signal carries the exit code.
    emit logEvent(4002, QStringLiteral("process error %1").arg(int(error)));
}

void AcpClient::onReadyRead()
{
    feedBytes(m_process.readAllStandardOutput());
}

void AcpClient::feedBytes(const QByteArray &chunk)
{
    for (char c : chunk) {
        ++m_counter; // v3 §3.1: tæller counts received bytes, discarded included
        if (c == '\n') {
            // 0x0A is always consumed, also while discarding (v3 §3.1/§3.2,
            // decision 127), so buffer draining never fails.
            const QByteArray brutto = m_buffer;
            const qint64 received = m_counter;
            const bool overCap = m_discarding;
            m_buffer.clear();
            m_counter = 0;
            m_discarding = false;
            m_l1Fired = false;
            if (overCap) {
                // v3 §3.2: a line where L1 fired is never handed to §4/§5.
                emit lineOverCap(received - 1, m_cap);
                emit logEvent(LogL1OverCap,
                              QStringLiteral("counter=%1 cap=%2").arg(received - 1).arg(m_cap));
                continue;
            }
            handleLine(brutto, received - 1);
        } else if (!m_discarding) {
            if (m_counter > m_cap && !m_l1Fired) {
                // v3 §3.1, decision 127: past the cap, extra bytes are dropped
                // at arrival; L1 fires once per line.
                m_discarding = true;
                m_l1Fired = true;
            }
            if (!m_discarding)
                m_buffer.append(c);
        }
        // else: byte over the cap, dropped at arrival
    }
}

void AcpClient::handleLine(const QByteArray &brutto, qint64 receivedBytes)
{
    const int n = brutto.size();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(brutto, &err);
    if (err.error == QJsonParseError::NoError) {
        if (doc.isObject()) {
            dispatchParsed(doc.object(), false);
        } else if (doc.isArray()) {
            // v3 §8 (decision 116): a matrix is rejected wholesale as P2.
            sendError(0, -32600, QStringLiteral("Invalid Request: batch not supported"));
            emit logEvent(3001, QStringLiteral("batch rejected, n=%1").arg(n));
        }
        return;
    }
    if (err.error == QJsonParseError::IllegalValue) {
        // v3 §5.5 scalar probe: wrap in [ ... ] and reparse.
        QByteArray wrapped;
        wrapped.reserve(n + 2);
        wrapped.append('[');
        wrapped.append(brutto);
        wrapped.append(']');
        QJsonParseError err2;
        QJsonDocument doc2 = QJsonDocument::fromJson(wrapped, &err2);
        if (err2.error == QJsonParseError::NoError && doc2.isArray()
            && doc2.array().size() == 1 && !doc2.array().at(0).isObject()
            && !doc2.array().at(0).isArray()) {
            // A valid non-message JSON scalar: P2, -32600, id: null (v3 §5.5 trin 6).
            sendError(0, -32600, QStringLiteral("Invalid Request: not a message"));
            emit logEvent(3001, QStringLiteral("scalar, n=%1").arg(n));
            return;
        }
    }
    // P1: not valid JSON (all other ParseError, incl. GarbageAtEnd and
    // DocumentTooLarge as L2's cousin here — both are parse errors with
    // id: null per v3 §6.3).
    sendError(0, -32700, QStringLiteral("Parse error"));
    emit logEvent(2001, QStringLiteral("error=%1 offset=%2 n=%3")
                  .arg(int(err.error)).arg(err.offset).arg(n));
    Q_UNUSED(receivedBytes)
}

void AcpClient::handleTail()
{
    // v3 §3.3: at EOF a non-empty tail is parsed and delivered but never
    // answered; logged with 1003. A zero-byte tail is not a hale (decision 128).
    if (m_buffer.isEmpty())
        return;
    if (m_discarding) {
        // L1 already fired for this "line"; the tail must not be parsed (v3 §3.3 trin 1).
        m_buffer.clear();
        return;
    }
    const int h = m_buffer.size(); // v3 §6.5: hale positions are measured from its first byte, clamped to [0, h]
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(m_buffer, &err);
    m_buffer.clear();
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        emit haleReceived(doc.object());
        emit logEvent(LogHale, QStringLiteral("h=%1").arg(h));
    } else {
        emit logEvent(LogHale, QStringLiteral("unparseable tail, error=%1").arg(int(err.error)));
    }
}

void AcpClient::dispatchParsed(const QJsonObject &obj, bool fromHale)
{
    Q_UNUSED(fromHale)
    emit messageReceived(obj);
}

void AcpClient::sendError(qint64 id, int code, const QString &message)
{
    QJsonObject errObj;
    errObj.insert(QStringLiteral("code"), code);
    errObj.insert(QStringLiteral("message"), message);
    QJsonObject resp;
    resp.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    // v3 §6.3 P1: id MUST be null when it cannot be detected.
    resp.insert(QStringLiteral("id"), id == 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(id));
    resp.insert(QStringLiteral("error"), errObj);
    writeFrame(resp);
}

qint64 AcpClient::sendRequest(const QString &method, const QJsonObject &params)
{
    if (m_process.state() != QProcess::Running)
        return -1;
    return sendRequestRaw(method, params);
}

qint64 AcpClient::sendRequestRaw(const QString &method, const QJsonObject &params)
{
    const qint64 id = m_nextId++;
    QJsonObject req;
    req.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    req.insert(QStringLiteral("id"), id);
    req.insert(QStringLiteral("method"), method);
    if (!params.isEmpty())
        req.insert(QStringLiteral("params"), params);
    writeFrame(req);
    return id;
}

void AcpClient::setSessionId(const QString &id)
{
    if (m_sessionId == id)
        return;
    m_sessionId = id;
    emit sessionIdChanged();
}

qint64 AcpClient::sendSessionNew(const QString &cwd)
{
    QJsonObject params;
    params.insert(QStringLiteral("cwd"), cwd);
    params.insert(QStringLiteral("mcpServers"), QJsonArray());
    return sendRequestRaw(QStringLiteral("session/new"), params);
}

qint64 AcpClient::sendPrompt(const QString &text)
{
    if (m_sessionId.isEmpty())
        return -1;
    QJsonObject content;
    content.insert(QStringLiteral("type"), QStringLiteral("text"));
    content.insert(QStringLiteral("text"), text);
    QJsonObject params;
    params.insert(QStringLiteral("sessionId"), m_sessionId);
    params.insert(QStringLiteral("prompt"), QJsonArray{content});
    const qint64 id = sendRequestRaw(QStringLiteral("session/prompt"), params);
    emit promptSent(id, text);
    return id;
}

void AcpClient::sendResult(qint64 id, const QJsonObject &result)
{
    QJsonObject resp;
    resp.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    resp.insert(QStringLiteral("id"), id);
    resp.insert(QStringLiteral("result"), result);
    writeFrame(resp);
}

void AcpClient::sendNotification(const QString &method, const QJsonObject &params)
{
    if (m_process.state() != QProcess::Running)
        return;
    QJsonObject note;
    note.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    note.insert(QStringLiteral("method"), method);
    if (!params.isEmpty())
        note.insert(QStringLiteral("params"), params);
    writeFrame(note);
}

void AcpClient::writeFrame(const QJsonObject &obj)
{
    // v3 §2.1: newline-delimited; Skrynja writes only 0x0A on outgoing traffic
    // (decision 126). Compact separators, no embedded newlines.
    QByteArray frame = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    frame.append('\n');
    m_process.write(frame);
    emit frameWritten(frame);
}
