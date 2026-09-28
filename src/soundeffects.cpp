#include "soundeffects.h"
#include "music.h"

#include <QLoggingCategory>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(Q_OS_WASM)
#include <emscripten.h>

#include <QTimer>
#elif defined(PHONG_HAVE_MULTIMEDIA)
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QMutex>
#include <QMutexLocker>
#include <QSpan>
#include <QTimer>

#include <type_traits>
#endif

Q_LOGGING_CATEGORY(lcSound, "phong.sound", QtWarningMsg)

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

// The same noise every time, a sound must not change between two renders
class NoiseSource
{
public:
    float next()
    {
        m_state = m_state * 1664525u + 1013904223u;
        return float(m_state >> 8) / float(1u << 24) * 2.0f - 1.0f;
    }

private:
    quint32 m_state = 0x2545f491u;
};
}

#if defined(Q_OS_WASM)

// Browsers only let audio start from a user gesture. The context is created
// and resumed on every key press, click and touch, and when the tab shows
// again, a sound itself may follow a timer instead of a gesture.
EM_JS(void, phong_setup_audio, (), {
    const Context = globalThis.AudioContext || globalThis.webkitAudioContext;
    if (!Context || globalThis.phongAudioSetUp)
        return;
    globalThis.phongAudioSetUp = true;

    const wake = () => {
        if (!globalThis.phongAudio)
            globalThis.phongAudio = new Context();
        if (globalThis.phongAudio.state !== "running")
            globalThis.phongAudio.resume();
    };
    for (const type of ["keydown", "pointerdown", "touchend"])
        document.addEventListener(type, wake, { capture: true });
    document.addEventListener("visibilitychange", () => {
        if (document.visibilityState === "visible" && globalThis.phongAudio)
            wake();
    });
});

EM_JS(void, phong_play_tone, (int waveform, double start, double duration, double from,
                              double to, double volume), {
    const context = globalThis.phongAudio;
    if (!context)
        return;
    // Also "interrupted" on Safari, e.g. after a call
    if (context.state !== "running")
        context.resume();

    const time = context.currentTime + start;
    let source;
    if (waveform === 4) {
        // Noise from a looped buffer, slower playback darkens it like the
        // held levels of the desktop synthesis
        if (!globalThis.phongNoise) {
            const buffer = context.createBuffer(1, context.sampleRate, context.sampleRate);
            const data = buffer.getChannelData(0);
            for (let i = 0; i < data.length; ++i)
                data[i] = 2 * Math.random() - 1;
            globalThis.phongNoise = buffer;
        }
        source = context.createBufferSource();
        source.buffer = globalThis.phongNoise;
        source.loop = true;
        source.playbackRate.value = Math.min(1, 2 * from / context.sampleRate);
    }
    else {
        source = context.createOscillator();
        source.type = ["square", "triangle", "sawtooth", "sine"][waveform];
        source.frequency.setValueAtTime(from, time);
        source.frequency.exponentialRampToValueAtTime(Math.max(to, 1), time + duration);
    }

    const gain = context.createGain();
    gain.gain.setValueAtTime(0, time);
    gain.gain.linearRampToValueAtTime(volume, time + 0.003);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + duration);

    source.connect(gain);
    gain.connect(context.destination);
    source.start(time);
    source.stop(time + duration + 0.05);
});

// The audio clock, -1 while there is no running context
EM_JS(double, phong_audio_time, (), {
    const context = globalThis.phongAudio;
    return context && context.state === "running" ? context.currentTime : -1;
});

class SoundEffects::Backend
{
public:
    Backend()
    {
        phong_setup_audio();
        // The music is scheduled a little ahead on the audio clock, the
        // timer only has to keep up
        m_timer.setInterval(25);
        QObject::connect(&m_timer, &QTimer::timeout, [this] { scheduleMusic(); });
    }

    bool isAvailable() const
    {
        return true;
    }

    void play(const QList<Tone>& tones, qreal pitch, qreal start = 0.0)
    {
        for (const Tone& tone : tones)
            phong_play_tone(int(tone.waveform), start + tone.start, tone.duration, tone.from * pitch,
                            tone.to * pitch, tone.volume * masterVolume);
    }

    void setMusic(bool playing, int intensity, qreal bpm)
    {
        m_intensity = intensity;
        m_bpm = bpm;
        if (playing == m_timer.isActive())
            return;

        if (playing) {
            m_step = 0;
            m_nextStep = -1.0;
            m_timer.start();
        }
        else {
            m_timer.stop();
        }
    }

private:
    void scheduleMusic()
    {
        const qreal now = phong_audio_time();
        if (now < 0.0)
            return;

        // Starting, or the tab was in the background
        if (m_nextStep < now)
            m_nextStep = now + 0.05;

        while (m_nextStep < now + lookahead) {
            const qreal duration = Music::stepDuration(m_bpm);
            play(Music::step(m_step++, m_intensity, duration), 1.0, m_nextStep - now);
            m_nextStep += duration;
        }
    }

    static constexpr qreal lookahead = 0.15;

    QTimer m_timer;
    int m_intensity = 0;
    qreal m_bpm = 120.0;
    int m_step = 0;
    qreal m_nextStep = -1.0;
};

#elif defined(PHONG_HAVE_MULTIMEDIA)

// Mixes the playing sounds into the output, silence in between. The music
// is sequenced here too, on the audio thread, to keep its time.
class Mixer
{
public:
    explicit Mixer(int sampleRate):
        m_sampleRate(sampleRate)
    {}

    void add(const QList<float>& samples)
    {
        QMutexLocker locker(&m_mutex);
        m_voices.append({ samples, 0 });
    }

    void setMusic(bool playing, int intensity, qreal bpm)
    {
        QMutexLocker locker(&m_mutex);
        if (playing && !m_musicPlaying) {
            m_step = 0;
            m_nextStep = qreal(m_frame);
        }
        m_musicPlaying = playing;
        m_intensity = intensity;
        m_bpm = bpm;
    }

    template<typename Sample>
    void render(QSpan<Sample> output, int channels)
    {
        QMutexLocker locker(&m_mutex);
        const qsizetype frames = output.size() / channels;
        sequence(frames);

        for (qsizetype frame = 0; frame < frames; ++frame) {
            float sample = 0.0f;
            for (Voice& voice : m_voices) {
                // Voices of the music may start later in the buffer
                if (voice.position >= 0 && voice.position < voice.samples.size())
                    sample += voice.samples.at(voice.position);
                ++voice.position;
            }

            const Sample value = convert<Sample>(std::clamp(sample, -1.0f, 1.0f));
            for (int channel = 0; channel < channels; ++channel)
                output[frame * channels + channel] = value;
        }

        m_frame += frames;
        m_voices.removeIf([](const Voice& voice) { return voice.position >= voice.samples.size(); });
    }

private:
    template<typename Sample>
    static Sample convert(float sample)
    {
        if constexpr (std::is_same_v<Sample, float>)
            return sample;
        else if constexpr (std::is_same_v<Sample, qint16>)
            return qint16(sample * 32767.0f);
        else if constexpr (std::is_same_v<Sample, qint32>)
            return qint32(sample * 2147483647.0f);
        else
            return quint8((sample + 1.0f) * 127.5f);
    }

    // Starts the steps of the music due within the next frames
    void sequence(qsizetype frames)
    {
        if (!m_musicPlaying)
            return;

        const qreal end = qreal(m_frame + frames);
        while (m_nextStep < end) {
            const qreal duration = Music::stepDuration(m_bpm);
            const QList<float> samples = SoundEffects::render(Music::step(m_step++, m_intensity, duration),
                                                              m_sampleRate);
            const qsizetype delay = std::max(qsizetype(m_nextStep) - qsizetype(m_frame), qsizetype(0));
            m_voices.append({ samples, -delay });
            m_nextStep += duration * m_sampleRate;
        }
    }

    struct Voice {
        QList<float> samples;
        qsizetype position; // negative while waiting to start
    };

    QMutex m_mutex;
    QList<Voice> m_voices;
    const int m_sampleRate;
    qint64 m_frame = 0;
    bool m_musicPlaying = false;
    int m_intensity = 0;
    qreal m_bpm = 120.0;
    int m_step = 0;
    qreal m_nextStep = 0.0;
};

#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
// Before Qt 6.11 a sink only pulls from a QIODevice, on the main thread
class MixerDevice : public QIODevice
{
public:
    MixerDevice(std::shared_ptr<Mixer> mixer, const QAudioFormat& format):
        m_mixer(std::move(mixer)),
        m_format(format)
    {
        open(QIODevice::ReadOnly);
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
        const qint64 frames = maxSize / m_format.bytesPerFrame();
        const qsizetype samples = frames * m_format.channelCount();
        if (m_format.sampleFormat() == QAudioFormat::Float)
            m_mixer->render(QSpan<float>(reinterpret_cast<float*>(data), samples), m_format.channelCount());
        else
            m_mixer->render(QSpan<qint16>(reinterpret_cast<qint16*>(data), samples), m_format.channelCount());
        return frames * m_format.bytesPerFrame();
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

private:
    std::shared_ptr<Mixer> m_mixer;
    QAudioFormat m_format;
};
#endif

class SoundEffects::Backend
{
public:
    Backend()
    {
        // Follow the default output, e.g. when headphones are plugged in
        QObject::connect(&m_devices, &QMediaDevices::audioOutputsChanged, &m_context, [this] {
            qCDebug(lcSound) << "Audio outputs changed";
            restart();
        });
        restart();
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
        // Don't wait for the retry if the sound went away in the meantime
        if (!m_sink || m_sink->state() == QAudio::StoppedState)
            restart();

        if (m_mixer)
            m_mixer->add(SoundEffects::render(tones, m_sampleRate, pitch));
    }

    void setMusic(bool playing, int intensity, qreal bpm)
    {
        m_musicPlaying = playing;
        m_intensity = intensity;
        m_bpm = bpm;
        if (m_mixer)
            m_mixer->setMusic(playing, intensity, bpm);
    }

private:
    // An error stops a sink for good, e.g. a write error when the output
    // device changes or is suspended, so a new one takes over
    void restart()
    {
        m_restartPending = false;
        if (m_sink) {
            QObject::disconnect(m_sink.get(), nullptr, &m_context, nullptr);
            m_sink->stop();
        }
        m_sink.reset();
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
        m_device.reset();
#endif

        const QAudioDevice device = QMediaDevices::defaultAudioOutput();
        if (device.isNull()) {
            qCDebug(lcSound) << "No audio output";
            return;
        }

        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Float);
        if (!device.isFormatSupported(format))
            format = device.preferredFormat();

        m_sampleRate = format.sampleRate();
        m_mixer = std::make_shared<Mixer>(m_sampleRate);
        m_mixer->setMusic(m_musicPlaying, m_intensity, m_bpm);
        m_sink = std::make_unique<QAudioSink>(device, format);
        m_sink->setBufferSize(format.bytesForDuration(60000));

        QObject::connect(m_sink.get(), &QAudioSink::stateChanged, &m_context,
                         [this](QAudio::State state) {
            qCDebug(lcSound) << "Audio state" << state << "error" << m_sink->error();
            if (state == QAudio::StoppedState && m_sink->error() != QAudio::NoError)
                scheduleRestart();
        });

        const int channels = format.channelCount();
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
        // The audio thread mixes, a busy main thread can't starve the sound
        const std::shared_ptr<Mixer> mixer = m_mixer;
        switch (format.sampleFormat()) {
            case QAudioFormat::Float:
                m_sink->start([mixer, channels](QSpan<float> output) { mixer->render(output, channels); });
                break;
            case QAudioFormat::Int16:
                m_sink->start([mixer, channels](QSpan<qint16> output) { mixer->render(output, channels); });
                break;
            case QAudioFormat::Int32:
                m_sink->start([mixer, channels](QSpan<qint32> output) { mixer->render(output, channels); });
                break;
            case QAudioFormat::UInt8:
                m_sink->start([mixer, channels](QSpan<quint8> output) { mixer->render(output, channels); });
                break;
            default:
                m_sink.reset();
                return;
        }
#else
        if (format.sampleFormat() != QAudioFormat::Float && format.sampleFormat() != QAudioFormat::Int16) {
            m_sink.reset();
            return;
        }
        Q_UNUSED(channels)
        m_device = std::make_unique<MixerDevice>(m_mixer, format);
        m_sink->start(m_device.get());
#endif
        qCDebug(lcSound) << "Playing on" << device.description() << format;
    }

    void scheduleRestart()
    {
        if (m_restartPending)
            return;

        m_restartPending = true;
        QTimer::singleShot(250, &m_context, [this] { restart(); });
    }

    QObject m_context;
    QMediaDevices m_devices;
    bool m_restartPending = false;
    int m_sampleRate = 44100;
    bool m_musicPlaying = false;
    int m_intensity = 0;
    qreal m_bpm = 120.0;
    std::shared_ptr<Mixer> m_mixer;
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
    std::unique_ptr<MixerDevice> m_device;
#endif
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

    void setMusic(bool, int, qreal)
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
            return { { Square, 0.0, 0.08, 880, 1320, 0.16 } };
        case Sound::CountdownTick:
            return { { Square, 0.0, 0.04, 587, 587, 0.12 } };
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
        case Sound::Smash:
            return { { Sawtooth, 0.00, 0.18, 220, 880, 0.26 },
                     { Square, 0.00, 0.10, 660, 330, 0.20 } };
        case Sound::RallyMilestone:
            return { { Square, 0.00, 0.06, 784, 784, 0.16 },
                     { Square, 0.06, 0.06, 988, 988, 0.16 },
                     { Square, 0.12, 0.12, 1175, 1175, 0.16 },
                     { Triangle, 0.12, 0.24, 1568, 1568, 0.2 } };
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
        case Sound::Dash:
            return { { Triangle, 0.0, 0.12, 300, 1200, 0.30 },
                     { Square, 0.0, 0.08, 150, 600, 0.08 } };
        case Sound::Perfect:
            return { { Square, 0.00, 0.05, 1318, 1318, 0.16 },
                     { Triangle, 0.04, 0.25, 2637, 2637, 0.22 } };
        case Sound::Catch:
            return { { Triangle, 0.0, 0.10, 900, 300, 0.35 },
                     { Square, 0.0, 0.06, 120, 90, 0.12 } };
        case Sound::Portal:
            return { { Sine, 0.00, 0.22, 200, 1600, 0.35 },
                     { Triangle, 0.10, 0.20, 1600, 400, 0.20 } };
        case Sound::Achievement:
            return { { Square, 0.00, 0.08, 659, 659, 0.14 },
                     { Square, 0.08, 0.08, 880, 880, 0.14 },
                     { Square, 0.16, 0.08, 1109, 1109, 0.14 },
                     { Triangle, 0.24, 0.40, 1319, 1319, 0.22 },
                     { Triangle, 0.24, 0.40, 1760, 1760, 0.12 } };
        case Sound::BrickBreak:
            return { { Square, 0.00, 0.06, 900, 500, 0.22 },
                     { Sawtooth, 0.03, 0.14, 300, 120, 0.20 } };
        case Sound::Freeze:
            return { { Triangle, 0.00, 0.08, 2400, 2000, 0.18 },
                     { Triangle, 0.06, 0.08, 3000, 2600, 0.14 },
                     { Sine, 0.12, 0.30, 1800, 1500, 0.18 } };
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
        NoiseSource noise;
        float held = noise.next();
        for (qsizetype i = 0; i < count && first + i < samples.size(); ++i) {
            const qreal t = qreal(i) / sampleRate;
            const qreal progress = t / tone.duration;
            const qreal frequency = from * std::pow(to / from, progress);
            const qreal envelope = volume * std::pow(silence / volume, progress)
                                   * std::min(1.0, t / attack);

            const qreal value = tone.waveform == Noise ? held : waveform(tone.waveform, phase);
            samples[first + i] += float(envelope * value);

            // Noise holds a level for a cycle
            phase += frequency / sampleRate;
            if (phase >= 1.0) {
                phase = std::fmod(phase, 1.0);
                held = noise.next();
            }
        }
    }

    return samples;
}

SoundEffects::SoundEffects(QObject* parent):
    QObject(parent),
    m_enabled(true),
    m_musicEnabled(true),
    m_musicPlaying(false),
    m_musicIntensity(0),
    m_musicTempo(120.0),
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
    updateMusic();
}

void SoundEffects::setMusicEnabled(bool musicEnabled)
{
    if (m_musicEnabled == musicEnabled)
        return;

    m_musicEnabled = musicEnabled;
    emit musicEnabledChanged(musicEnabled);
    updateMusic();
}

bool SoundEffects::isMusicEnabled() const
{
    return m_musicEnabled;
}

void SoundEffects::setMusicPlaying(bool musicPlaying)
{
    if (m_musicPlaying == musicPlaying)
        return;

    m_musicPlaying = musicPlaying;
    emit musicPlayingChanged(musicPlaying);
    updateMusic();
}

bool SoundEffects::isMusicPlaying() const
{
    return m_musicPlaying;
}

void SoundEffects::setMusicIntensity(int musicIntensity)
{
    musicIntensity = std::clamp(musicIntensity, 0, Music::maxIntensity);
    if (m_musicIntensity == musicIntensity)
        return;

    m_musicIntensity = musicIntensity;
    emit musicIntensityChanged(musicIntensity);
    updateMusic();
}

int SoundEffects::musicIntensity() const
{
    return m_musicIntensity;
}

void SoundEffects::setMusicTempo(qreal musicTempo)
{
    musicTempo = std::clamp(musicTempo, 60.0, 240.0);
    if (m_musicTempo == musicTempo)
        return;

    m_musicTempo = musicTempo;
    emit musicTempoChanged(musicTempo);
    updateMusic();
}

qreal SoundEffects::musicTempo() const
{
    return m_musicTempo;
}

void SoundEffects::updateMusic()
{
    m_backend->setMusic(m_enabled && m_musicEnabled && m_musicPlaying, m_musicIntensity, m_musicTempo);
}

bool SoundEffects::isEnabled() const
{
    return m_enabled;
}

bool SoundEffects::isAvailable() const
{
    return m_backend->isAvailable();
}
