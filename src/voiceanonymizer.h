#ifndef VOICEANONYMIZER_H
#define VOICEANONYMIZER_H

#include <QObject>
#include <QString>

// Voice anonymization for F18 voice input (EXPERIMENTAL — test branch).
//
// Goal: make the speaker harder to identify from their voice before the
// audio leaves the device for transcription. Two local, offline effects:
//
// 1. Pitch shift (resampling with linear interpolation). A factor != 1.0
//    changes the fundamental frequency of the voice: 1.15 raises it
//    (towards a higher voice), 0.85 lowers it. This is the single most
//    effective cheap anonymization against casual voice recognition.
//
// 2. Light background noise (very low-level white noise mixed in). It
//    masks fine spectral details of the voice without hurting
//    transcription accuracy.
//
// Input/output: 16-bit PCM WAV files (the format AudioRecorder produces
// with codec audio/PCM + container wav). Mono and stereo are supported;
// other bit depths are rejected with a clear error.
//
// The processing is synchronous and O(n) — a few seconds of speech take
// well under a second on a phone CPU. It runs on the GUI thread in the
// current integration, which is acceptable for short voice messages.
class VoiceAnonymizer : public QObject
{
    Q_OBJECT

public:
    explicit VoiceAnonymizer(QObject *parent = 0);

    // Anonymize inputFile into outputFile.
    // pitchFactor: 1.0 = unchanged, > 1.0 = higher voice, < 1.0 = lower.
    // noiseLevel: 0.0 = off, up to ~0.02 is subtle (2% of full scale).
    // Returns true on success; on failure returns false and sets *errorMessage.
    Q_INVOKABLE bool anonymize(const QString &inputFile,
                               const QString &outputFile,
                               double pitchFactor = 1.15,
                               double noiseLevel = 0.005,
                               QString *errorMessage = 0);

    // Pure DSP helpers, exposed for unit tests (no file I/O).
    static QByteArray shiftPitch(const QByteArray &pcm16, double factor);
    static QByteArray addNoise(const QByteArray &pcm16, double level);
};

#endif
