#include "soundeffects.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#elif defined(PHONG_HAVE_MULTIMEDIA)
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QMutex>
#include <QMutexLocker>
#endif

namespace {
// Everything a bit quieter, square waves are loud
constexpr qreal masterVolume = 0.6;
// Short fade in against clicks
constexpr qreal attack = 0.003;
// The level the volume decays to by the end of a tone
constexpr qreal silence = 0.0001;

qreal waveform(SoundEffects::Waveform waveform, qreal phase)
{
    // phase in cycles, 0 to 1
    switch (waveform) {
        case SoundEffects::Waveform::Square:
            return phase < 0.5 ? 1.0 : -1.0;
        case SoundEffects::Waveform::Triangle:
            return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
        case SoundEffects::Waveform::Sawtooth:
            return 2.0 * phase - 1.0;
        case SoundEffects::Waveform::Sine:
        default:
            return std::sin(2.0 * M_PI * phase);
    }
}
}

#if defined(Q_OS_WASM)

EM_JS(void, phong_play_tone, (int waveform, double start, double duration, double from,
                              double to, double volume), {
    const Context = globalThis.AudioContext || globalThis.webkitAudioContext;
    if (!Context)
        return;
    if (!globalThis.phongAudio)
        globalThis.phongAudio = new Context();

    // Browsers only start audio after a user gesture, sounds follow key
    // presses and clicks, so resuming here works
    const context = globalThis.phongAudio;
    if (context.state === "suspended")
        context.resume();

    const time = context.currentTime + start;
    const oscillator = context.createOscillator();
    oscillator.type = ["square", "triangle", "sawtooth", "sine"][waveform];
    oscillator.frequency.setValueAtTime(from, time);
    oscillator.frequency.exponentialRampToValueAtTime(Math.max(to, 1), time + duration);

    const gain = context.createGain();
    gain.gain.setValueAtTime(0, time);
    gain.gain.linearRampToValueAtTime(volume, time + 0.003);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + duration);

    oscillator.connect(gain);
    gain.connect(context.destination);
    oscillator.start(time);
    oscillator.stop(time + duration + 0.05);
});

class SoundEffects::Backend
{
public:
    bool isAvailable() const
    {
        return true;
    }

    void play(const QList<Tone>& tones, qreal pitch)
    {
        for (const Tone& tone : tones)
            phong_play_tone(int(tone.waveform), tone.start, tone.duration, tone.from * pitch,
                            tone.to * pitch, tone.volume * masterVolume);
    }
};

#elif defined(PHONG_HAVE_MULTIMEDIA)

// Mixes the playing sounds for an audio sink that pulls, silence in between
class Mixer : public QIODevice
{
public:
    explicit Mixer(const QAudioFormat& format, QObject* parent = nullptr):
        QIODevice(parent),
        m_format(format)
    {
        open(QIODevice::ReadOnly);
    }

    void add(const QList<float>& samples)
    {
        QMutexLocker locker(&m_mutex);
        m_voices.append({ samples, 0 });
    }

    bool isSequential() const override
    {
        return true;
    }

    qint64 bytesAvailable() const override
    {
        return m_format.bytesForDuration(100000) + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        const int bytesPerSample = m_format.bytesPerSample();
        const int channels = m_format.channelCount();
        const qint64 frames = maxSize / m_format.bytesPerFrame();

        QMutexLocker locker(&m_mutex);
        for (qint64 frame = 0; frame < frames; ++frame) {
            float sample = 0.0f;
            for (Voice& voice : m_voices) {
                if (voice.position < voice.samples.size())
                    sample += voice.samples.at(voice.position++);
            }
            sample = std::clamp(sample, -1.0f, 1.0f);

            for (int channel = 0; channel < channels; ++channel) {
                char* target = data + (frame * channels + channel) * bytesPerSample;
                if (m_format.sampleFormat() == QAudioFormat::Float) {
                    std::memcpy(target, &sample, sizeof(float));
                }
                else {
                    const qint16 value = qint16(sample * 32767.0f);
                    std::memcpy(target, &value, sizeof(qint16));
                }
            }
        }

        m_voices.removeIf([](const Voice& voice) { return voice.position >= voice.samples.size(); });
        return frames * m_format.bytesPerFrame();
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

private:
    struct Voice {
        QList<float> samples;
        qsizetype position;
    };

    QAudioFormat m_format;
    QMutex m_mutex;
    QList<Voice> m_voices;
};

class SoundEffects::Backend
{
public:
    Backend()
    {
        const QAudioDevice device = QMediaDevices::defaultAudioOutput();
        if (device.isNull())
            return;

        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);
        if (!device.isFormatSupported(format))
            format = device.preferredFormat();

        if (format.sampleFormat() != QAudioFormat::Int16
            && format.sampleFormat() != QAudioFormat::Float)
            return;

        m_sampleRate = format.sampleRate();
        m_mixer = std::make_unique<Mixer>(format);
        m_sink = std::make_unique<QAudioSink>(device, format);
        // Short buffer, a hit should sound when it happens
        m_sink->setBufferSize(format.bytesForDuration(40000));
        m_sink->start(m_mixer.get());
    }

    ~Backend()
    {
        if (m_sink)
            m_sink->stop();
    }

    bool isAvailable() const
    {
        return bool(m_sink);
    }

    void play(const QList<Tone>& tones, qreal pitch)
    {
        if (m_mixer)
            m_mixer->add(SoundEffects::render(tones, m_sampleRate, pitch));
    }

private:
    int m_sampleRate = 44100;
    std::unique_ptr<Mixer> m_mixer;
    std::unique_ptr<QAudioSink> m_sink;
};

#else

class SoundEffects::Backend
{
public:
    bool isAvailable() const
    {
        return false;
    }

    void play(const QList<Tone>&, qreal)
    {}
};

#endif

QList<SoundEffects::Tone> SoundEffects::tones(Sound sound)
{
    switch (sound) {
        case Sound::PaddleHit:
            return { { Square, 0.0, 0.07, 560, 440, 0.30 } };
        case Sound::WallHit:
            return { { Square, 0.0, 0.05, 300, 260, 0.22 } };
        case Sound::Bounce:
            return { { Triangle, 0.0, 0.12, 700, 350, 0.40 } };
        case Sound::Goal:
            return { { Sawtooth, 0.0, 0.45, 440, 110, 0.30 },
                     { Square, 0.0, 0.30, 220, 70, 0.15 } };
        case Sound::Serve:
            return { { Square, 0.0, 0.03, 880, 880, 0.12 } };
        case Sound::Pickup:
            return { { Square, 0.00, 0.06, 660, 660, 0.18 },
                     { Square, 0.06, 0.06, 990, 990, 0.18 },
                     { Square, 0.12, 0.10, 1320, 1320, 0.18 } };
        case Sound::Curse:
            return { { Sawtooth, 0.00, 0.35, 520, 180, 0.22 },
                     { Sawtooth, 0.05, 0.30, 390, 130, 0.15 } };
        case Sound::ShieldHit:
            return { { Triangle, 0.0, 0.18, 1200, 600, 0.40 } };
        case Sound::MultiBall:
            return { { Square, 0.00, 0.05, 440, 440, 0.16 },
                     { Square, 0.05, 0.05, 660, 660, 0.16 },
                     { Square, 0.10, 0.05, 880, 880, 0.16 },
                     { Square, 0.15, 0.08, 1100, 1100, 0.16 } };
        case Sound::MenuMove:
            return { { Square, 0.0, 0.03, 740, 740, 0.10 } };
        case Sound::MenuSelect:
            return { { Square, 0.0, 0.08, 740, 1100, 0.14 } };
        case Sound::Win:
            return { { Square, 0.00, 0.12, 523, 523, 0.18 },
                     { Square, 0.12, 0.12, 659, 659, 0.18 },
                     { Square, 0.24, 0.12, 784, 784, 0.18 },
                     { Square, 0.36, 0.30, 1046, 1046, 0.18 } };
        case Sound::Lose:
            return { { Square, 0.00, 0.18, 392, 392, 0.18 },
                     { Square, 0.18, 0.18, 330, 330, 0.18 },
                     { Square, 0.36, 0.40, 262, 196, 0.18 } };
    }
    return {};
}

QList<float> SoundEffects::render(const QList<Tone>& tones, int sampleRate, qreal pitch)
{
    qreal length = 0.0;
    for (const Tone& tone : tones)
        length = std::max(length, tone.start + tone.duration);

    // Rounded, not up, 0.15 s at 8 kHz must not become 1201 samples
    QList<float> samples(qsizetype(std::lround(length * sampleRate)), 0.0f);
    for (const Tone& tone : tones) {
        const qsizetype first = qsizetype(tone.start * sampleRate);
        const qsizetype count = qsizetype(tone.duration * sampleRate);
        const qreal volume = tone.volume * masterVolume;
        const qreal from = tone.from * pitch;
        const qreal to = tone.to * pitch;

        qreal phase = 0.0;
        for (qsizetype i = 0; i < count && first + i < samples.size(); ++i) {
            const qreal t = qreal(i) / sampleRate;
            const qreal progress = t / tone.duration;
            const qreal frequency = from * std::pow(to / from, progress);
            const qreal envelope = volume * std::pow(silence / volume, progress)
                                   * std::min(1.0, t / attack);

            samples[first + i] += float(envelope * waveform(tone.waveform, phase));
            phase = std::fmod(phase + frequency / sampleRate, 1.0);
        }
    }

    return samples;
}

SoundEffects::SoundEffects(QObject* parent):
    QObject(parent),
    m_enabled(true),
    m_backend(std::make_unique<Backend>())
{}

SoundEffects::~SoundEffects() = default;

void SoundEffects::play(Sound sound, qreal pitch)
{
    if (m_enabled && pitch > 0.0)
        m_backend->play(tones(sound), pitch);
}

void SoundEffects::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    emit enabledChanged(enabled);
}

bool SoundEffects::isEnabled() const
{
    return m_enabled;
}

bool SoundEffects::isAvailable() const
{
    return m_backend->isAvailable();
}
