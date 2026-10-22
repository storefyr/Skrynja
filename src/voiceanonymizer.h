#ifndef VOICEANONYMIZER_H
#define VOICEANONYMIZER_H

#include <QObject>
#include <QString>

// Voice anonymization for F18 voice input (EXPERIMENTAL — test branch).
//
// Goal: make the speaker harder to identify from their voice before the
// audio leaves the device for transcription. Three local, offline effects:
//
// 1. Pitch shift (resampling with linear interpolation). A factor != 1.0
//    changes the fundamental frequency of the voice: 1.15 raises it
//    (towards a higher voice), 0.85 lowers it. This is the single most
//    effective cheap anonymization against casual voice recognition.
//
// 2. Ring modulation. The signal is multiplied by a slowly oscillating
//    carrier (sample * (1 - depth + depth * cos(2*pi*f*t))). At depth 1.0
//    this is classic "robot voice" ring modulation, which destroys the
//    natural harmonic structure of the voice and is much harder to
//    recognize than a plain pitch shift. At depth 0 it is a no-op.
//
// 3. Light background noise (very low-level white noise mixed in). It
//    masks fine spectral details of the voice without hurting
//    transcription accuracy.
//
// The Strength presets combine these effects:
//   subtle: pitch 1.10 + noise 0.005 (mild, still natural)
//   medium: pitch 1.18 + ring 220 Hz @ 35% + noise 0.008 (clearly changed)
//   strong: pitch 1.28 + ring 180 Hz @ 60% + noise 0.012 (robotic)
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

    // Strength presets for preset-based anonymization.
    enum Strength {
        Subtle = 0,   // mild: pitch 1.10 + subtle noise
        Medium = 1,   // pitch 1.18 + ring modulation + noise
        Strong = 2    // pitch 1.28 + strong ring modulation + more noise
    };

    // Map a strength name ("subtle", "medium", "strong") to the enum.
    // Unknown or empty names fall back to Medium.
    static Strength strengthFromString(const QString &name);

    // Anonymize inputFile into outputFile using a strength preset.
    // C++ API. Returns true on success; on failure returns false and
    // sets *errorMessage.
    bool anonymizeWithStrength(const QString &inputFile,
                               const QString &outputFile,
                               Strength strength,
                               QString *errorMessage = 0);

    // Anonymize inputFile into outputFile with raw parameters. C++ API.
    // pitchFactor: 1.0 = unchanged, > 1.0 = higher voice, < 1.0 = lower.
    // noiseLevel: 0.0 = off, up to ~0.02 is subtle (2% of full scale).
    // Returns true on success; on failure returns false and sets *errorMessage.
    bool anonymize(const QString &inputFile,
                   const QString &outputFile,
                   double pitchFactor = 1.15,
                   double noiseLevel = 0.005,
                   QString *errorMessage = 0);

    // QML-friendly wrapper: returns an empty string on success, otherwise
    // a human-readable error message. QML cannot call methods that take
    // QString* parameters ("Unknown method parameter type: QString*"), so
    // the QML integration must use this variant.
    Q_INVOKABLE QString anonymizeFile(const QString &inputFile,
                                      const QString &outputFile,
                                      const QString &strength);

    // Pure DSP helpers, exposed for unit tests (no file I/O).
    static QByteArray shiftPitch(const QByteArray &pcm16, double factor);
    static QByteArray addNoise(const QByteArray &pcm16, double level);
    // Ring modulation: y[n] = x[n] * (1 - depth + depth * cos(2*pi*f*n/sr)).
    // depth 0.0 = no-op, 1.0 = full ring modulation ("robot voice").
    static QByteArray ringModulate(const QByteArray &pcm16,
                                   quint32 sampleRate,
                                   double frequency,
                                   double depth);
};

#endif
