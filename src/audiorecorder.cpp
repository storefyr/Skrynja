#include "audiorecorder.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QTimer>
#include <QDebug>

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
    , m_recorder(new QAudioRecorder(this))
    , m_stoppedEmitted(false)
{
    connect(m_recorder, SIGNAL(stateChanged(QAudioRecorder::State)),
            this, SLOT(onStateChanged(QAudioRecorder::State)));
    connect(m_recorder, SIGNAL(durationChanged(qint64)),
            this, SIGNAL(durationChanged(qint64)));
    connect(m_recorder, SIGNAL(error(QAudioRecorder::Error)),
            this, SLOT(onError(QAudioRecorder::Error)));

    m_stopTimer = new QTimer(this);
    m_stopTimer->setSingleShot(true);
    m_stopTimer->setInterval(5000);
    connect(m_stopTimer, SIGNAL(timeout()), this, SLOT(onStopTimeout()));

    QAudioEncoderSettings settings;
    settings.setCodec("audio/x-wav");
    settings.setQuality(QMultimedia::HighQuality);
    m_recorder->setEncodingSettings(settings);
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
               << "error before:" << m_recorder->errorString();
    m_stoppedEmitted = false;
    m_recorder->record();
    qWarning() << "AudioRecorder::record() state after:" << m_recorder->state()
               << "status after:" << m_recorder->status();
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
