#include "music.h"
#include "soundeffects.h"

#include <QMetaEnum>
#include <QTest>

#include <algorithm>
#include <cmath>

class tst_SoundEffects : public QObject
{
    Q_OBJECT

private slots:
    void everySoundHasTones()
    {
        const QMetaEnum sounds = QMetaEnum::fromType<SoundEffects::Sound>();
        for (int i = 0; i < sounds.keyCount(); ++i) {
            const auto sound = SoundEffects::Sound(sounds.value(i));
            const QList<SoundEffects::Tone> tones = SoundEffects::tones(sound);
            QVERIFY2(!tones.isEmpty(), sounds.key(i));

            // Short effects, and quiet enough to mix a few
            for (const SoundEffects::Tone& tone : tones) {
                QVERIFY(tone.start + tone.duration < 1.0);
                QVERIFY(tone.volume > 0.0 && tone.volume <= 0.5);
                QVERIFY(tone.from > 20.0 && tone.to > 20.0);
            }
        }
    }

    void render()
    {
        const QList<SoundEffects::Tone> tones = { { SoundEffects::Square, 0.0, 0.1, 440, 440, 0.3 },
                                                  { SoundEffects::Sine, 0.05, 0.1, 880, 440, 0.3 } };
        const QList<float> samples = SoundEffects::render(tones, 8000);
        QCOMPARE(samples.size(), 1200);

        const auto [min, max] = std::minmax_element(samples.cbegin(), samples.cend());
        QVERIFY(*min >= -1.0f && *max <= 1.0f);
        QVERIFY(*max > 0.1f);

        // Fades out
        QVERIFY(std::abs(samples.at(790)) < 0.01f);

        // Pitch shortens the period, not the sound
        QCOMPARE(SoundEffects::render(tones, 8000, 1.5).size(), samples.size());
    }

    void noiseIsTheSameEveryTime()
    {
        const QList<SoundEffects::Tone> tones = { { SoundEffects::Noise, 0.0, 0.1, 4000, 4000, 0.3 } };
        const QList<float> samples = SoundEffects::render(tones, 8000);
        QCOMPARE(SoundEffects::render(tones, 8000), samples);

        const auto [min, max] = std::minmax_element(samples.cbegin(), samples.cend());
        QVERIFY(*min >= -1.0f && *max <= 1.0f);
        QVERIFY(*min < -0.02f && *max > 0.02f);
    }

    void musicLoops()
    {
        QCOMPARE(Music::stepDuration(120.0), 0.125);

        for (int intensity = 0; intensity <= Music::maxIntensity; ++intensity) {
            for (int step = 0; step < Music::loopSteps; ++step) {
                const QList<SoundEffects::Tone> tones = Music::step(step, intensity, 0.125);
                const QList<SoundEffects::Tone> again = Music::step(step + Music::loopSteps, intensity, 0.125);
                QCOMPARE(tones.size(), again.size());
                for (qsizetype i = 0; i < tones.size(); ++i)
                    QCOMPARE(tones.at(i).from, again.at(i).from);

                // Under the sound effects, and in the step or a few after it
                for (const SoundEffects::Tone& tone : tones) {
                    QCOMPARE(tone.start, 0.0);
                    QVERIFY(tone.duration > 0.0 && tone.duration <= 8 * 0.125);
                    QVERIFY(tone.volume > 0.0 && tone.volume <= 0.3);
                    QVERIFY(tone.from > 20.0 && tone.to > 20.0);
                }
            }
        }
    }

    void musicGetsBusier()
    {
        int previous = -1;
        for (int intensity = 0; intensity <= Music::maxIntensity; ++intensity) {
            int count = 0;
            for (int step = 0; step < Music::loopSteps; ++step)
                count += int(Music::step(step, intensity, 0.125).size());
            QVERIFY2(count > previous, qPrintable(QString::number(intensity)));
            previous = count;
        }

        // Out of range intensities are clamped
        QCOMPARE(Music::step(0, 99, 0.125).size(), Music::step(0, Music::maxIntensity, 0.125).size());
        QCOMPARE(Music::step(-1, 0, 0.125).size(), Music::step(Music::loopSteps - 1, 0, 0.125).size());
    }

    void musicFollowsTheSwitches()
    {
        SoundEffects sounds;
        QVERIFY(!sounds.isMusicPlaying());
        sounds.setMusicPlaying(true);
        sounds.setMusicIntensity(9);
        QCOMPARE(sounds.musicIntensity(), Music::maxIntensity);
        sounds.setMusicTempo(1000.0);
        QCOMPARE(sounds.musicTempo(), 240.0);
        sounds.setMusicEnabled(false);
        QVERIFY(!sounds.isMusicEnabled());
    }

    void disabledIsSilent()
    {
        SoundEffects sounds;
        sounds.setEnabled(false);
        sounds.play(SoundEffects::Goal);
        QVERIFY(!sounds.isEnabled());
    }
};

QTEST_GUILESS_MAIN(tst_SoundEffects)
#include "tst_soundeffects.moc"
