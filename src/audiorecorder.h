#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QObject>
#include <QAudioRecorder>
#include <QAudioEncoderSettings>

class AudioRecorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QString outputLocation READ outputLocation WRITE setOutputLocation NOTIFY outputLocationChanged)

public:
    explicit AudioRecorder(QObject *parent = 0);
    ~AudioRecorder();

    bool recording() const;
    bool available() const;
    QString errorString() const;
    QString outputLocation() const;

    void setOutputLocation(const QString &location);

    Q_INVOKABLE void record();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();

signals:
    void recordingChanged();
    void availableChanged();
    void errorChanged();
    void outputLocationChanged();
    void stopped();
    void error(QAudioRecorder::Error error);

private slots:
    void onStateChanged(QAudioRecorder::State state);
    void onError(QAudioRecorder::Error error);

private:
    QAudioRecorder *m_recorder;
    QString m_outputLocation;
};

#endif
