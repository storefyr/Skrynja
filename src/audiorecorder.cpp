#include "audiorecorder.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QTimer>

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
    , m_recorder(new QAudioRecorder(this))
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
    m_recorder->record();
}

void AudioRecorder::stop()
{
    // Always start the safety timer, even if state is not RecordingState.
    // If the recorder is stuck, the timer will force-emit 'stopped'.
    m_stopTimer->start();
    if (m_recorder->state() == QAudioRecorder::RecordingState
            || m_recorder->state() == QAudioRecorder::PausedState) {
        m_recorder->stop();
    } else {
        // Already stopped or in an unknown state — emit immediately.
        m_stopTimer->stop();
        emit stopped();
    }
}

void AudioRecorder::onStopTimeout()
{
    // The recorder never reached StoppedState. Force-emit stopped so the
    // UI can proceed (the file may be incomplete, but better than hanging).
    emit stopped();
}

void AudioRecorder::pause()
{
    m_recorder->pause();
}

qint64 AudioRecorder::duration() const
{
    return m_recorder->duration();
}

void AudioRecorder::onStateChanged(QAudioRecorder::State state)
{
    emit recordingChanged();
    if (state == QAudioRecorder::StoppedState) {
        m_stopTimer->stop();
        emit stopped();
    }
}

void AudioRecorder::onError(QAudioRecorder::Error error)
{
    emit errorChanged();
    emit errorOccurred(error);
}
