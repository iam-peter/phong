#ifndef SOUNDEFFECTS_H
#define SOUNDEFFECTS_H

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

// Retro sound effects synthesized from a few tones, no audio files. On the
// desktop they are mixed into an audio sink, in the browser they are
// played with the Web Audio API.
class SoundEffects : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)

public:
    enum Sound {
        PaddleHit = 0,
        WallHit,
        Bounce,
        Goal,
        Serve,
        CountdownTick,
        Pickup,
        Curse,
        ShieldHit,
        MultiBall,
        Smash,
        RallyMilestone,
        MenuMove,
        MenuSelect,
        Win,
        Lose,
        Dash,
        Perfect,
        Catch
    };
    Q_ENUM(Sound)

    enum Waveform {
        Square = 0,
        Triangle,
        Sawtooth,
        Sine
    };

    // Frequency glides exponentially from from to to, the volume decays
    // exponentially over the duration
    struct Tone {
        Waveform waveform;
        qreal start;    // seconds after the sound starts
        qreal duration;
        qreal from;     // Hz
        qreal to;
        qreal volume;
    };

    static QList<Tone> tones(Sound sound);
    // Mono samples between -1 and 1
    static QList<float> render(const QList<Tone>& tones, int sampleRate, qreal pitch = 1.0);

    explicit SoundEffects(QObject* parent = nullptr);
    ~SoundEffects() override;

    // pitch scales all frequencies, e.g. higher for faster balls
    Q_INVOKABLE void play(SoundEffects::Sound sound, qreal pitch = 1.0);

    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Whether there is a way to play sound at all
    bool isAvailable() const;

signals:
    void enabledChanged(bool);

private:
    class Backend;

    bool m_enabled;
    std::unique_ptr<Backend> m_backend;
};

#endif // SOUNDEFFECTS_H
