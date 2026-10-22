#ifndef VOICEANONYMIZER_H
#define VOICEANONYMIZER_H

#include <QObject>
#include <QString>

// Voice anonymization for F18 voice input (EXPERIMENTAL — test branch).
//
// Goal: make the speaker harder to identify from their voice before the
// audio leaves the device for transcription. Four local, offline effects:
//
// 1. Pitch shift (resampling with linear interpolation). A factor != 1.0
//    changes the fundamental frequency of the voice: 1.15 raises it
//    (towards a higher voice), 0.85 lowers it. This is the single most
//    effective cheap anonymization against casual voice recognition.
//
// 2. Spectral tilt (a one-pole low-pass in the frequency domain). A
//    negative tilt attenuates the high frequencies relative to the low
//    ones, changing the timbre of the voice — the "radio voice" sound.
//    Unlike ring modulation this keeps the harmonic structure intact,
//    which speech-to-text models handle far better (see v0.9.6 note).
//
// 3. Ring modulation. The signal is multiplied by a slowly oscillating
//    carrier (sample * (1 - depth + depth * cos(2*pi*f*t))). At depth 1.0
//    this is classic "robot voice" ring modulation, which destroys the
//    natural harmonic structure of the voice and is much harder to
//    recognize than a plain pitch shift. At depth 0 it is a no-op.
//    NOTE: kept as a DSP primitive but NOT used in any preset since
//    v0.9.6 — on-device testing showed it too often breaks transcription.
//
// 4. Light background noise (very low-level white noise mixed in). It
//    masks fine spectral details of the voice without hurting
//    transcription accuracy.
//
// The Strength presets combine these effects:
//   subtle: pitch 1.10 + noise 0.005 (mild, still natural)
//   medium: pitch 1.18 + spectral tilt + noise 0.008 (recommended)
//   strong: pitch 1.24 + stronger tilt + telephone band (300-3400 Hz)
//           + noise 0.012 (max, may hurt STT)
//
// NOTE on ring modulation: the carrier frequency must stay ABOVE the
// human voice fundamental range (85-255 Hz). At 180-220 Hz the sidebands
// overlap the speech spectrum and the speech-to-text model hears
// nonsense — verified on device (see v0.9.2 test round). Even above the
// voice range it kept raising the error rate (v0.9.6), so the presets no
// longer use it.
//
// Anti-voiceprint hardening (v0.9.5):
// - The RNG is seeded per process. Without that, std::rand() produced the
//   IDENTICAL noise pattern on every app start, and an attacker could
//   average several anonymized recordings to recover and subtract it.
// - anonymizeWithStrength() jitters pitch factor, tilt depth and noise
//   level slightly per recording, so no two files share the same
//   transformation — averaging or inverting across recordings fails.
// - "strong" additionally band-limits the audio to the telephone band,
//   stripping the out-of-speech-band spectral detail that voiceprint
//   systems rely on. This is obfuscation, not a guarantee: the only true
//   protection is keeping the audio on the device (local STT) or typing.
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
    // Kept as a primitive (unit-tested) but unused by the presets since
    // v0.9.6: it raised the on-device transcription error rate.
    static QByteArray ringModulate(const QByteArray &pcm16,
                                   quint32 sampleRate,
                                   double frequency,
                                   double depth);
    // Telephone-band limiting (300-3400 Hz, four RBJ biquads): strips the
    // spectral detail outside the speech band that voiceprint systems
    // rely on. Speech-to-text models handle band-limited audio well.
    static QByteArray bandLimit(const QByteArray &pcm16, quint32 sampleRate);
    // Spectral tilt: a one-pole low-pass (tilt < 0) or high-pass (tilt > 0)
    // applied sample-by-sample, y[n] = (1-a)*x[n] + a*y[n-1] with
    // a = exp(2*pi*fc/sr) and fc derived from the tilt. A negative tilt
    // (default) cuts high frequencies, giving the voice a darker timbre.
    // Keeps the harmonic structure intact — STT-friendly.
    static QByteArray spectralTilt(const QByteArray &pcm16,
                                   quint32 sampleRate,
                                   double tilt);
};

#endif
