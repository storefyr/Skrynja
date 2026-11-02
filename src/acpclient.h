#ifndef ACPCLIENT_H
#define ACPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QByteArray>
#include <QJsonObject>

// ACP client over stdio, per dokumenter/vibe-acp/spec-protokol-v3.md (v3).
// Spawns vibe-acp as a child process and speaks newline-delimited JSON-RPC 2.0
// on its stdin/stdout. Only the protocol layer is implemented here: session
// handling and content blocks are out of scope (v3 §0.2).
//
// Wire rules implemented (v3 §2-§6):
//  - framing by 0x0A only; a raw 0x0D is left in the line and eaten by Qt's
//    parser as insignificant whitespace (v3 §2.2, decision 126);
//  - one QJsonDocument::fromJson per line, no tolerance (v3 §5.1, decision 96);
//  - scalar probe: IllegalValue at top level -> wrap in [ ... ] and reparse
//    (v3 §5.5); a scalar answers -32600, id: null (P2);
//  - a matrix is rejected wholesale as P2: -32600, id: null (v3 §8, decision 116);
//  - line counter (v3 §3.1, decision 127): counts received bytes of the
//    current line, discarded bytes included; past the cap the extra bytes are
//    dropped at arrival, 0x0A is still consumed, L1 fires once per line, and
//    the line is never handed to the parser (v3 §3.2);
//  - at EOF a non-empty tail is parsed and delivered but never answered,
//    logged with internal code 1003 (v3 §3.3, decisions 128-130);
//  - internal L-codes are log-only, never sent on the wire (v3 §6.5).
class AcpClient : public QObject
{
    Q_OBJECT

public:
    // Internal log-only event codes (v3 Bilag C). Not wire codes.
    enum LogCode {
        LogReserved1000 = 1000, // reserved, unused (v3 §6.5)
        LogL1OverCap    = 1001, // received bytes of current line over the cap
        LogHale         = 1003  // tail at EOF without 0x0A
    };
    Q_ENUM(LogCode)

    explicit AcpClient(QObject *parent = nullptr);
    ~AcpClient() override;

    // Agent path as configured for the device (v3 §9.2 field 1).
    static QString defaultAgentPath();
    // Line cap in received bytes (v3 §4.2; alpha may use a smaller value).
    static qint64 defaultLineCap();

    void start(const QString &agentPath, const QString &workingDir,
               const QStringList &args = QStringList());
    void stop();

    // Alpha/test hook: lower the line cap (v3 §4.2 allows any value <= the
    // spec cap; the cap itself is a configuration value, v3 §9.2 field 9).
    void setLineCap(qint64 cap) { m_cap = cap; }
    qint64 lineCap() const { return m_cap; }

    // Feeds received bytes through the line splitter (v3 §3). Public so unit
    // tests can drive the parser without spawning a process; in production
    // only onReadyRead() calls it.
    void feedBytes(const QByteArray &data);
    // Simulates EOF: parses and logs the tail (v3 §3.3). Public for tests.
    void feedEof() { handleTail(); }

    // Sends one JSON-RPC request (id assigned here) or notification (no id).
    // Returns the id, or -1 for a notification / when not running.
    qint64 sendRequest(const QString &method, const QJsonObject &params);
    void sendNotification(const QString &method, const QJsonObject &params);

signals:
    // A complete, parsed message from the agent (response or notification).
    void messageReceived(const QJsonObject &message);
    // A line was dropped because the counter passed the cap (L1). No reply
    // is sent for it (v3 §6.2 L1).
    void lineOverCap(qint64 receivedBytes, qint64 cap);
    // EOF with a non-empty tail: parsed and delivered, never answered.
    void haleReceived(const QJsonObject &message);
    // Internal log event; code is a LogCode, never a wire code.
    void logEvent(int code, const QString &detail);
    void started();
    void finished(int exitCode, QProcess::ExitStatus status);

private slots:
    void onReadyRead();
    void onErrorOccurred(QProcess::ProcessError error);

private:
    void handleLine(const QByteArray &brutto, qint64 receivedBytes);
    void handleTail();
    void dispatchParsed(const QJsonObject &obj, bool fromHale);
    void sendError(qint64 id, int code, const QString &message);
    void writeFrame(const QJsonObject &obj);

    QProcess m_process;
    QByteArray m_buffer;   // bytes of the current line, capped (v3 §3.1)
    qint64 m_counter = 0;  // received bytes of the current line (v3 §1 "tæller")
    qint64 m_cap;
    bool m_l1Fired = false; // L1 already fired for the current line
    bool m_discarding = false; // past the cap: drop at arrival
    qint64 m_nextId = 1;
};

#endif // ACPCLIENT_H
