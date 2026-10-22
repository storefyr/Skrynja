#ifndef VOICEANONYMIZER_H
#define VOICEANONYMIZER_H

#include <QObject>
#include <QString>

// Voice anonymization for F18 voice input (ON BY DEFAULT since v0.9.7 —
// the only anonymization level; there is no strength selector anymore).
//
// Goal: make the speaker harder to identify from their voice before the
// audio leaves the device for transcription. One fixed pipeline of local,
// offline effects (the level that on-device testing converged on):
//
// 1. Pitch shift (resampling with linear interpolation), ~1.24. Changes
//    the fundamental frequency of the voice (higher).
// 2. Spectral tilt (a one-pole low-pass, tilt -0.55). Darkens the timbre
//    (the "radio voice" sound) while keeping the harmonic structure
//    intact — speech-to-text models handle it far better than ring
//    modulation, which destroyed the error rate (see v0.9.2-v0.9.6 notes).
// 3. Telephone-band limiting (300-3400 Hz, four RBJ biquads). Strips the
//    out-of-speech-band spectral detail that voiceprint systems rely on.
// 4. Light background noise (1.2% of full scale). Masks fine spectral
//    details without hurting transcription accuracy.
//
// Anti-voiceprint hardening (v0.9.5, kept):
// - The RNG is seeded per process. Without that, std::rand() produced the
//   IDENTICAL noise pattern on every app start, and an attacker could
//   average several anonymized recordings to recover and subtract it.
// - anonymizeFile() jitters pitch factor, tilt depth and noise level
//   slightly per recording, so no two files share the same
//   transformation — averaging or inverting across recordings fails.
//
// Honest limits: anonymization is obfuscation, not encryption. Keeping
// the audio on the device (local STT) would be the only true protection
// against voiceprinting — but on-device testing (Notes Plus, 2026-10-22)
// showed whisper.cpp is far too slow on a phone CPU and far too poor at
// Danish, so local STT is NOT a viable option right now. Realistic
// alternatives: this anonymization pipeline, or typing instead of speaking.
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

    // Anonymize inputFile into outputFile with raw parameters. C++ API,
    // kept for unit tests.
    // pitchFactor: 1.0 = unchanged, > 1.0 = higher voice, < 1.0 = lower.
    // noiseLevel: 0.0 = off, up to ~0.02 is subtle (2% of full scale).
    // Returns true on success; on failure returns false and sets *errorMessage.
    bool anonymize(const QString &inputFile,
                   const QString &outputFile,
                   double pitchFactor = 1.15,
                   double noiseLevel = 0.005,
                   QString *errorMessage = 0);

    // QML-friendly wrapper: anonymizes with the single fixed pipeline
    // (pitch ~1.24 + tilt -0.55 + telephone band + noise 0.012, jittered
    // per recording). Returns an empty string on success, otherwise a
    // human-readable error message. QML cannot call methods that take
    // QString* parameters ("Unknown method parameter type: QString*"), so
    // the QML integration must use this variant.
    Q_INVOKABLE QString anonymizeFile(const QString &inputFile,
                                      const QString &outputFile);

    // Pure DSP helpers, exposed for unit tests (no file I/O).
    static QByteArray shiftPitch(const QByteArray &pcm16, double factor);
    static QByteArray addNoise(const QByteArray &pcm16, double level);
    // Telephone-band limiting (300-3400 Hz, four RBJ biquads): strips the
    // spectral detail outside the speech band that voiceprint systems
    // rely on. Speech-to-text models handle band-limited audio well.
    static QByteArray bandLimit(const QByteArray &pcm16, quint32 sampleRate);
    // Spectral tilt: a one-pole low-pass (tilt < 0) or high-pass (tilt > 0)
    // applied sample-by-sample, y[n] = (1-a)*x[n] + a*y[n-1] with
    // a = exp(-2*pi*fc/sr) and fc derived from the tilt. A negative tilt
    // (default) cuts high frequencies, giving the voice a darker timbre.
    // Keeps the harmonic structure intact — STT-friendly.
    static QByteArray spectralTilt(const QByteArray &pcm16,
                                   quint32 sampleRate,
                                   double tilt);
};

#endif
