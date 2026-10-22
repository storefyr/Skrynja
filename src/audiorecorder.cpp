#include "audiorecorder.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QTimer>
#include <QDebug>
#include <QMediaRecorder>

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
    , m_recorder(new QAudioRecorder(this))
    , m_stoppedEmitted(false)
{
    // Pointer-based connects: string-based SIGNAL/SLOT connects with
    // QAudioRecorder::State / QAudioRecorder::Error FAIL on Sailfish's
    // Qt 5.6 ("No such signal") because moc registers the inherited
    // QMediaRecorder signatures. Pointer connects are checked at compile
    // time and work on both Qt 5.6 and 5.15.
    connect(m_recorder, &QAudioRecorder::stateChanged,
            this, &AudioRecorder::onStateChanged);
    connect(m_recorder, &QAudioRecorder::durationChanged,
            this, &AudioRecorder::durationChanged);
    // 'error' is overloaded (signal error(Error) + getter error() const),
    // so a plain &QAudioRecorder::error is ambiguous — disambiguate.
    connect(m_recorder,
            static_cast<void (QMediaRecorder::*)(QMediaRecorder::Error)>(
                &QMediaRecorder::error),
            this, &AudioRecorder::onError);

    m_stopTimer = new QTimer(this);
    m_stopTimer->setSingleShot(true);
    m_stopTimer->setInterval(5000);
    connect(m_stopTimer, &QTimer::timeout, this, &AudioRecorder::onStopTimeout);

    // Sailfish/GStreamer encoding configuration. IMPORTANT: the codec must
    // be a real audio codec ("audio/PCM"), NOT the container mimetype
    // ("audio/x-wav") — setting the container as codec fails with
    // "Not compatible codecs and container format" and record() never
    // starts. Pattern taken from mentaljam/sailfish-recorder, which works
    // on Sailfish OS.
    qWarning() << "AudioRecorder: supportedAudioCodecs:"
               << m_recorder->supportedAudioCodecs()
               << "supportedContainers:" << m_recorder->supportedContainers()
               << "audioInputs:" << m_recorder->audioInputs();
    m_recorder->setAudioInput(QStringLiteral("pulseaudio:"));

    QAudioEncoderSettings settings;
    settings.setCodec(QStringLiteral("audio/PCM"));
    settings.setQuality(QMultimedia::HighQuality);
    m_recorder->setEncodingSettings(settings);
    m_recorder->setContainerFormat(QStringLiteral("wav"));
}

AudioRecorder::~AudioRecorder()
{
    if (m_recorder->state() != QAudioRecorder::StoppedState) {
        m_recorder->stop();
    }
}

bool AudioRecorder::recording() const
{
    return m_recorder->state() == QAudioRecorder::RecordingState;
}

bool AudioRecorder::available() const
{
    return m_recorder->status() != QAudioRecorder::UnavailableStatus;
}

QString AudioRecorder::errorString() const
{
    return m_recorder->errorString();
}

QString AudioRecorder::outputLocation() const
{
    return m_outputLocation;
}

void AudioRecorder::setOutputLocation(const QString &location)
{
    if (m_outputLocation != location) {
        m_outputLocation = location;
        m_recorder->setOutputLocation(QUrl::fromLocalFile(location));
        emit outputLocationChanged();
    }
}

void AudioRecorder::record()
{
    if (m_outputLocation.isEmpty()) {
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cacheDir);
        m_outputLocation = cacheDir + "/skrynja-recording.wav";
        m_recorder->setOutputLocation(QUrl::fromLocalFile(m_outputLocation));
        emit outputLocationChanged();
    }
    qWarning() << "AudioRecorder::record() location:" << m_outputLocation
               << "state before:" << m_recorder->state()
               << "status before:" << m_recorder->status()
               << "error before:" << m_recorder->errorString()
               << "codec:" << m_recorder->audioSettings().codec()
               << "container:" << m_recorder->containerFormat();
    m_stoppedEmitted = false;
    m_recorder->record();
    qWarning() << "AudioRecorder::record() state after:" << m_recorder->state()
               << "status after:" << m_recorder->status()
               << "error after:" << m_recorder->errorString();
}

void AudioRecorder::stop()
{
    qWarning() << "AudioRecorder::stop() state:" << m_recorder->state()
               << "status:" << m_recorder->status()
               << "error:" << m_recorder->errorString();
    // Always start the safety timer, even if state is not RecordingState.
    // If the recorder is stuck, the timer will force-emit 'stopped'.
    m_stopTimer->start();
    if (m_recorder->state() == QAudioRecorder::RecordingState
            || m_recorder->state() == QAudioRecorder::PausedState) {
        // NOTE: on some backends (Sailfish/GStreamer) stateChanged(StoppedState)
        // is emitted SYNCHRONOUSLY from inside this call, which makes our
        // 'stopped' signal fire before this function returns. Callers in QML
        // must therefore set their 'transcribing' flag BEFORE calling stop().
        m_recorder->stop();
    } else {
        // Already stopped or in an unknown state — emit immediately.
        m_stopTimer->stop();
        emitStoppedOnce();
    }
}

void AudioRecorder::onStopTimeout()
{
    // The recorder never reached StoppedState. Force-emit stopped so the
    // UI can proceed (the file may be incomplete, but better than hanging).
    qWarning() << "AudioRecorder::onStopTimeout() — recorder never reached"
               << "StoppedState, forcing 'stopped'. state:"
               << m_recorder->state() << "status:" << m_recorder->status()
               << "error:" << m_recorder->errorString();
    emitStoppedOnce();
}

void AudioRecorder::pause()
{
    m_recorder->pause();
}

qint64 AudioRecorder::duration() const
{
    return m_recorder->duration();
}

void AudioRecorder::emitStoppedOnce()
{
    // 'stopped' must fire exactly once per recording session. Both
    // onStateChanged(StoppedState) and the stop-timeout can trigger it,
    // and on some backends stateChanged is emitted synchronously from
    // inside stop(), so without this guard the transcription could be
    // started twice.
    if (m_stoppedEmitted) {
        qWarning() << "AudioRecorder::emitStoppedOnce() — already emitted,"
                   << "ignoring duplicate";
        return;
    }
    m_stoppedEmitted = true;
    emit stopped();
}

void AudioRecorder::onStateChanged(QAudioRecorder::State state)
{
    qWarning() << "AudioRecorder::onStateChanged() state:" << state
               << "status:" << m_recorder->status()
               << "error:" << m_recorder->errorString();
    emit recordingChanged();
    if (state == QAudioRecorder::StoppedState) {
        m_stopTimer->stop();
        qWarning() << "AudioRecorder: file at" << m_outputLocation
                   << "size:" << QFileInfo(m_outputLocation).size();
        emitStoppedOnce();
    }
}

void AudioRecorder::onError(QAudioRecorder::Error error)
{
    qWarning() << "AudioRecorder::onError() error:" << error
               << "errorString:" << m_recorder->errorString()
               << "state:" << m_recorder->state()
               << "status:" << m_recorder->status();
    emit errorChanged();
    emit errorOccurred(error);
}
