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
