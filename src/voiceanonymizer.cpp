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

QByteArray VoiceAnonymizer::ringModulate(const QByteArray &pcm16,
                                         quint32 sampleRate,
                                         double frequency,
                                         double depth)
{
    if (depth <= 0.0 || sampleRate == 0 || frequency <= 0.0) {
        return pcm16;  // no-op
    }
    QByteArray out = pcm16;  // copy; modified IN PLACE below
    const int count = out.size() / 2;
    char *d = out.data();
    const double w = 2.0 * M_PI * frequency / double(sampleRate);
    for (int i = 0; i < count; ++i) {
        const qint16 s = static_cast<qint16>(readU16(d + i * 2));
        // Carrier in [1-depth, 1]: depth 1.0 gives full ring modulation.
        const double carrier = 1.0 - depth + depth * qCos(w * i);
        const double v = s * carrier;
        const qint16 sv = static_cast<qint16>(qBound(-32768.0, v, 32767.0));
        d[i * 2] = static_cast<char>(sv & 0xFF);
        d[i * 2 + 1] = static_cast<char>((sv >> 8) & 0xFF);
    }
    return out;
}

VoiceAnonymizer::Strength VoiceAnonymizer::strengthFromString(const QString &name)
{
    const QString n = name.toLower().trimmed();
    if (n == "subtle") return Subtle;
    if (n == "strong") return Strong;
    return Medium;  // "medium" and anything unknown
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
        Biquad bq = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
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

bool VoiceAnonymizer::anonymizeWithStrength(const QString &inputFile,
                                            const QString &outputFile,
                                            Strength strength,
                                            QString *errorMessage)
{
    seedRandOnce();

    // Strength presets: pitch factor, ring modulation (frequency in Hz,
    // depth 0..1), telephone band limiting and white-noise level. Chosen
    // so that each step is a clearly audible change while transcription
    // stays accurate.
    //
    // Ring modulation pitfall (found in on-device testing): a low carrier
    // frequency (<= ~220 Hz) sits inside the fundamental range of the
    // human voice (85-255 Hz) and creates sidebands that overlap the
    // speech spectrum — the speech-to-text model then hears nonsense
    // ("Spark, anonymous, ein SM" instead of the spoken sentence).
    // The carrier must stay ABOVE the speech range: 300 Hz and up.
    //
    // Anti-voiceprint jitter: pitch, ring frequency and noise level are
    // varied by a few percent per recording, so no two anonymized files
    // share the same transformation — averaging or inverting across
    // recordings fails. The jitter is small enough to stay STT-safe.
    double pitchFactor = 1.10;
    double ringFreq = 0.0;
    double ringDepth = 0.0;
    bool bandLimited = false;
    double noiseLevel = 0.005;
    switch (strength) {
    case Subtle:
        pitchFactor = 1.10;
        noiseLevel = 0.005;
        break;
    case Medium:
        pitchFactor = 1.18;
        ringFreq = 320.0;   // above the voice fundamental range
        ringDepth = 0.30;
        noiseLevel = 0.008;
        break;
    case Strong:
        // Maximum effort: same proven-safe ring settings as Medium, more
        // pitch shift and noise, PLUS telephone-band limiting (strips the
        // out-of-speech-band spectral detail voiceprint systems use).
        // Even this can reduce transcription accuracy — Medium is the
        // recommended everyday level.
        pitchFactor = 1.24;
        ringFreq = 320.0;   // above the voice fundamental range
        ringDepth = 0.30;
        bandLimited = true;
        noiseLevel = 0.012;
        break;
    }

    // Per-recording jitter (±3% on pitch/noise, ±5% on ring frequency).
    pitchFactor = qBound(1.05, pitchFactor * uniformRand(0.97, 1.03), 1.30);
    noiseLevel = qMax(0.0, noiseLevel * uniformRand(0.97, 1.03));
    if (ringFreq > 0.0) {
        ringFreq = qBound(300.0, ringFreq * uniformRand(0.95, 1.05), 400.0);
    }

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
    if (ringDepth > 0.0) {
        processed = ringModulate(processed, wav.sampleRate, ringFreq, ringDepth);
    }
    if (bandLimited) {
        processed = bandLimit(processed, wav.sampleRate);
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
               << "(strength" << strength
               << ", pitch" << pitchFactor
               << ", ring" << ringFreq << "Hz @" << ringDepth
               << (bandLimited ? ", band-limited" : "")
               << ", noise" << noiseLevel << ")";
    return true;
}

QString VoiceAnonymizer::anonymizeFile(const QString &inputFile,
                                       const QString &outputFile,
                                       const QString &strength)
{
    QString error;
    const bool ok = anonymizeWithStrength(inputFile, outputFile,
                                          strengthFromString(strength), &error);
    return ok ? QString() : error;
}
