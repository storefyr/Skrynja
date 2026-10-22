#include "voiceanonymizer.h"

#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QtGlobal>
#include <QtMath>
#include <QDateTime>
#include <cstdlib>

namespace {
// Seed std::rand() once per process. Without a seed the C runtime uses
// the default seed 1, so every app start would produce the IDENTICAL
// noise pattern — an attacker could average several anonymized
// recordings, recover the noise and subtract it. Seeding from the
// current time + a random-ish address makes each run different.
// (Not cryptographic — but enough to break deterministic averaging.)
void seedRandOnce()
{
    static bool seeded = false;
    if (seeded) return;
    seeded = true;
    const quint64 now = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    const quint64 addr = reinterpret_cast<quintptr>(&seeded);
    std::srand(static_cast<unsigned int>((now ^ addr) & 0xFFFFFFFFu));
}

// Uniform random double in [lo, hi).
double uniformRand(double lo, double hi)
{
    return lo + (std::rand() / double(RAND_MAX)) * (hi - lo);
}
} // namespace

VoiceAnonymizer::VoiceAnonymizer(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Minimal 16-bit PCM WAV reader/writer.
// We only handle the canonical 44-byte-header PCM WAV that QAudioRecorder
// produces on Sailfish (codec audio/PCM, container wav). Anything else is
// rejected — the caller falls back to sending the original file.
// ---------------------------------------------------------------------------

namespace {

struct WavData {
    bool valid = false;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    QByteArray pcm;   // raw interleaved 16-bit little-endian samples
};

quint16 readU16(const char *p)
{
    return static_cast<quint16>(static_cast<quint8>(p[0]))
         | (static_cast<quint16>(static_cast<quint8>(p[1])) << 8);
}

quint32 readU32(const char *p)
{
    return static_cast<quint32>(static_cast<quint8>(p[0]))
         | (static_cast<quint32>(static_cast<quint8>(p[1])) << 8)
         | (static_cast<quint32>(static_cast<quint8>(p[2])) << 16)
         | (static_cast<quint32>(static_cast<quint8>(p[3])) << 24);
}

void writeU16(QByteArray &out, quint16 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
}

void writeU32(QByteArray &out, quint32 v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 24) & 0xFF));
}

WavData readWav(const QString &path)
{
    WavData result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return result;
    }
    const QByteArray data = f.readAll();
    f.close();

    // Canonical RIFF/WAVE header: "RIFF" <size> "WAVE" "fmt " <16> ...
    if (data.size() < 44
            || !data.startsWith("RIFF")
            || data.mid(8, 4) != "WAVE"
            || data.mid(12, 4) != "fmt ") {
        return result;
    }
    const quint16 audioFormat = readU16(data.constData() + 20);
    if (audioFormat != 1) {  // PCM only
        return result;
    }
    const quint16 channels = readU16(data.constData() + 22);
    const quint32 sampleRate = readU32(data.constData() + 24);
    const quint16 bitsPerSample = readU16(data.constData() + 34);
    if (channels == 0 || sampleRate == 0 || bitsPerSample != 16) {
        return result;
    }

    // Find the "data" chunk (it follows the fmt chunk; walk chunks to be safe).
    int pos = 12;
    QByteArray pcm;
    while (pos + 8 <= data.size()) {
        const QByteArray chunkId = data.mid(pos, 4);
        const quint32 chunkSize = readU32(data.constData() + pos + 4);
        if (chunkId == "data") {
            const int avail = qMin<int>(chunkSize, data.size() - pos - 8);
            pcm = data.mid(pos + 8, avail);
            break;
        }
        pos += 8 + chunkSize + (chunkSize & 1);  // chunks are word-aligned
    }
    if (pcm.isEmpty()) {
        return result;
    }

    result.valid = true;
    result.channels = channels;
    result.sampleRate = sampleRate;
    result.pcm = pcm;
    return result;
}

bool writeWav(const QString &path, quint16 channels, quint32 sampleRate,
              const QByteArray &pcm)
{
    QByteArray out;
    out.reserve(44 + pcm.size());
    out.append("RIFF");
    writeU32(out, 36 + static_cast<quint32>(pcm.size()));
    out.append("WAVE");
    out.append("fmt ");
    writeU32(out, 16);                 // fmt chunk size
    writeU16(out, 1);                  // PCM
    writeU16(out, channels);
    writeU32(out, sampleRate);
    writeU32(out, sampleRate * channels * 2);  // byte rate
    writeU16(out, channels * 2);       // block align
    writeU16(out, 16);                 // bits per sample
    out.append("data");
    writeU32(out, static_cast<quint32>(pcm.size()));
    out.append(pcm);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const qint64 written = f.write(out);
    f.close();
    return written == out.size();
}

} // namespace

// ---------------------------------------------------------------------------
// DSP helpers (exposed for unit tests)
// ---------------------------------------------------------------------------

QByteArray VoiceAnonymizer::shiftPitch(const QByteArray &pcm16, double factor)
{
    if (factor <= 0.0 || qFuzzyCompare(factor, 1.0)) {
        return pcm16;  // no-op
    }
    const int frameCount = pcm16.size() / 2;  // number of 16-bit samples
    const int outCount = static_cast<int>(frameCount / factor);
    QByteArray out;
    // NOTE: do NOT pre-resize — QByteArray::append grows the buffer, so a
    // pre-sized buffer plus appends would double the size.
    out.reserve(outCount * 2);

    const char *src = pcm16.constData();
    for (int i = 0; i < outCount; ++i) {
        // Linear interpolation between neighbouring samples.
        const double srcPos = i * factor;
        const int i0 = static_cast<int>(srcPos);
        const double frac = srcPos - i0;
        const int i1 = qMin(i0 + 1, frameCount - 1);
        const qint16 s0 = static_cast<qint16>(readU16(src + i0 * 2));
        const qint16 s1 = static_cast<qint16>(readU16(src + i1 * 2));
        const double v = s0 + (s1 - s0) * frac;
        const qint16 sv = static_cast<qint16>(qBound(-32768.0, v, 32767.0));
        writeU16(out, static_cast<quint16>(sv));
    }
    return out;
}

QByteArray VoiceAnonymizer::addNoise(const QByteArray &pcm16, double level)
{
    if (level <= 0.0) {
        return pcm16;  // no-op
    }
    QByteArray out = pcm16;  // copy; modified IN PLACE below
    const int count = out.size() / 2;
    char *d = out.data();
    for (int i = 0; i < count; ++i) {
        const qint16 s = static_cast<qint16>(readU16(d + i * 2));
        // Uniform white noise in [-level, +level] * 32767.
        const double noise = (2.0 * (std::rand() / double(RAND_MAX)) - 1.0)
                             * level * 32767.0;
        const double v = s + noise;
        const qint16 sv = static_cast<qint16>(qBound(-32768.0, v, 32767.0));
        // Write the sample back at its own offset (in-place), not append.
        d[i * 2] = static_cast<char>(sv & 0xFF);
        d[i * 2 + 1] = static_cast<char>((sv >> 8) & 0xFF);
    }
    return out;
}

QByteArray VoiceAnonymizer::spectralTilt(const QByteArray &pcm16,
                                         quint32 sampleRate,
                                         double tilt)
{
    if (tilt == 0.0 || sampleRate == 0 || pcm16.size() < 2) {
        return pcm16;  // no-op
    }
    QByteArray out = pcm16;  // copy; filtered IN PLACE below
    const int count = out.size() / 2;
    char *d = out.data();

    // One-pole filter with a = exp(-2*pi*fc/sr) (0 < a < 1, stable).
    // We map tilt to a cutoff frequency so that:
    //   tilt -0.35 (medium) → fc ≈ 0.044 * sr  (~700 Hz @ 16 kHz)
    //   tilt -0.55 (strong) → fc ≈ 0.069 * sr  (~1100 Hz @ 16 kHz)
    // A negative tilt low-passes (darker timbre, "radio voice"); a positive
    // tilt high-passes (brighter). Unlike ring modulation the harmonic
    // structure stays intact, which keeps the audio STT-friendly.
    const double fc = sampleRate * qAbs(tilt) / 8.0;
    const double a = qExp(-2.0 * M_PI * fc / double(sampleRate));
    double yPrev = 0.0;
    double xPrev = 0.0;
    for (int i = 0; i < count; ++i) {
        const double x = static_cast<qint16>(readU16(d + i * 2));
        double y;
        if (tilt < 0.0) {
            y = (1.0 - a) * x + a * yPrev;                  // low-pass
        } else {
            y = a * (yPrev + x - xPrev);                    // high-pass
        }
        yPrev = y;
        xPrev = x;
        const qint16 sv = static_cast<qint16>(qBound(-32768.0, y, 32767.0));
        d[i * 2] = static_cast<char>(sv & 0xFF);
        d[i * 2 + 1] = static_cast<char>((sv >> 8) & 0xFF);
    }
    return out;
}

QByteArray VoiceAnonymizer::bandLimit(const QByteArray &pcm16, quint32 sampleRate)
{
    if (sampleRate == 0 || pcm16.size() < 4) {
        return pcm16;  // no-op
    }
    QByteArray out = pcm16;  // copy; filtered IN PLACE below
    const int count = out.size() / 2;
    char *d = out.data();

    // Four RBJ biquads in series (Q = 0.707, Butterworth, Direct Form I):
    // two high-passes at the 300 Hz telephone lower edge and two low-passes
    // at the 3400 Hz upper edge — a 4th-order bandpass with steep skirts.
    // A single 2nd-order stage only attenuates ~10 dB at 6 kHz; cascading
    // doubles the slope. This strips the spectral detail outside the
    // speech band that voiceprint systems rely on, while speech-to-text
    // models (trained on telephone audio) handle it well.
    struct Biquad {
        double b0, b1, b2, a1, a2;
        double x1, x2, y1, y2;
    };
    const double nyquist = sampleRate / 2.0;
    auto makeBiquad = [nyquist, sampleRate](double f0, bool highpass) {
        Biquad bq = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (f0 <= 0.0) return bq;
        if (f0 >= 0.45 * nyquist) f0 = 0.45 * nyquist;  // stay stable
        const double w0 = 2.0 * M_PI * f0 / double(sampleRate);
        const double alpha = qSin(w0) / (2.0 * 0.7071);
        const double cosw0 = qCos(w0);
        const double a0 = 1.0 + alpha;
        if (highpass) {
            bq.b0 = (1.0 + cosw0) / 2.0 / a0;
            bq.b1 = -(1.0 + cosw0) / a0;
            bq.b2 = (1.0 + cosw0) / 2.0 / a0;
        } else {
            bq.b0 = (1.0 - cosw0) / 2.0 / a0;
            bq.b1 = (1.0 - cosw0) / a0;
            bq.b2 = (1.0 - cosw0) / 2.0 / a0;
        }
        bq.a1 = -2.0 * cosw0 / a0;
        bq.a2 = (1.0 - alpha) / a0;
        return bq;
    };
    Biquad hp1 = makeBiquad(300.0, true);
    Biquad hp2 = makeBiquad(300.0, true);
    Biquad lp1 = makeBiquad(3400.0, false);
    Biquad lp2 = makeBiquad(3400.0, false);

    for (int i = 0; i < count; ++i) {
        const double x = static_cast<qint16>(readU16(d + i * 2));
        double y = x;
        // Two high-pass stages, then two low-pass stages.
        Biquad *stages[4] = {&hp1, &hp2, &lp1, &lp2};
        for (int s = 0; s < 4; ++s) {
            Biquad *bq = stages[s];
            const double z = bq->b0 * y + bq->b1 * bq->x1 + bq->b2 * bq->x2
                           - bq->a1 * bq->y1 - bq->a2 * bq->y2;
            bq->x2 = bq->x1; bq->x1 = y; bq->y2 = bq->y1; bq->y1 = z;
            y = z;
        }
        const qint16 sv = static_cast<qint16>(qBound(-32768.0, y, 32767.0));
        d[i * 2] = static_cast<char>(sv & 0xFF);
        d[i * 2 + 1] = static_cast<char>((sv >> 8) & 0xFF);
    }
    return out;
}

// ---------------------------------------------------------------------------
// File-level anonymization
// ---------------------------------------------------------------------------

bool VoiceAnonymizer::anonymize(const QString &inputFile,
                                const QString &outputFile,
                                double pitchFactor,
                                double noiseLevel,
                                QString *errorMessage)
{
    const WavData wav = readWav(inputFile);
    if (!wav.valid) {
        if (errorMessage) {
            *errorMessage = tr("Not a 16-bit PCM WAV file");
        }
        qWarning() << "VoiceAnonymizer: cannot parse" << inputFile;
        return false;
    }

    qWarning() << "VoiceAnonymizer: loaded" << inputFile
               << wav.channels << "ch" << wav.sampleRate << "Hz"
               << wav.pcm.size() << "bytes PCM";

    QByteArray processed = wav.pcm;
    if (!qFuzzyCompare(pitchFactor, 1.0)) {
        processed = shiftPitch(processed, pitchFactor);
    }
    if (noiseLevel > 0.0) {
        processed = addNoise(processed, noiseLevel);
    }

    if (!writeWav(outputFile, wav.channels, wav.sampleRate, processed)) {
        if (errorMessage) {
            *errorMessage = tr("Could not write the anonymized recording");
        }
        qWarning() << "VoiceAnonymizer: cannot write" << outputFile;
        return false;
    }

    qWarning() << "VoiceAnonymizer: wrote" << outputFile
               << QFileInfo(outputFile).size() << "bytes"
               << "(pitch" << pitchFactor << ", noise" << noiseLevel << ")";
    return true;
}

QString VoiceAnonymizer::anonymizeFile(const QString &inputFile,
                                       const QString &outputFile)
{
    seedRandOnce();

    // The single fixed pipeline (on by default since v0.9.7): pitch ~1.24,
    // spectral tilt -0.55, telephone-band limiting and 1.2% noise.
    //
    // Anti-voiceprint jitter: pitch, tilt and noise level are varied by a
    // few percent per recording, so no two anonymized files share the same
    // transformation — averaging or inverting across recordings fails.
    // The jitter is small enough to stay STT-safe (verified on device).
    double pitchFactor = qBound(1.05, 1.24 * uniformRand(0.97, 1.03), 1.30);
    double tilt = qBound(-0.70, -0.55 * uniformRand(0.97, 1.03), -0.15);
    const double noiseLevel = qMax(0.0, 0.012 * uniformRand(0.97, 1.03));

    const WavData wav = readWav(inputFile);
    if (!wav.valid) {
        qWarning() << "VoiceAnonymizer: cannot parse" << inputFile;
        return tr("Not a 16-bit PCM WAV file");
    }

    qWarning() << "VoiceAnonymizer: loaded" << inputFile
               << wav.channels << "ch" << wav.sampleRate << "Hz"
               << wav.pcm.size() << "bytes PCM";

    QByteArray processed = wav.pcm;
    processed = shiftPitch(processed, pitchFactor);
    processed = spectralTilt(processed, wav.sampleRate, tilt);
    processed = bandLimit(processed, wav.sampleRate);
    processed = addNoise(processed, noiseLevel);

    if (!writeWav(outputFile, wav.channels, wav.sampleRate, processed)) {
        qWarning() << "VoiceAnonymizer: cannot write" << outputFile;
        return tr("Could not write the anonymized recording");
    }

    qWarning() << "VoiceAnonymizer: wrote" << outputFile
               << QFileInfo(outputFile).size() << "bytes"
               << "(pitch" << pitchFactor
               << ", tilt" << tilt
               << ", band-limited"
               << ", noise" << noiseLevel << ")";
    return QString();
}
