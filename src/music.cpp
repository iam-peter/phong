#include "music.h"

#include <QtMath>

#include <algorithm>
#include <array>

namespace {
using Tone = SoundEffects::Tone;

qreal frequency(int note)
{
    // MIDI note number, 69 is A4 at 440 Hz
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

struct Chord {
    int root;   // MIDI note of the bass
    int third;  // semitones above the root
};

// Am, F, C, G
constexpr std::array<Chord, Music::bars> chords = { { { 45, 3 }, { 41, 4 }, { 48, 4 }, { 43, 4 } } };

// A minor pentatonic lead, a note per step or -1 for a rest, one phrase
// per bar
constexpr std::array<std::array<int, Music::stepsPerBar>, Music::bars> lead = { {
    { 76, -1, -1, 74, -1, -1, 72, -1, 69, -1, 72, -1, 74, -1, -1, -1 },
    { 72, -1, -1, 69, -1, -1, 67, -1, 69, -1, -1, -1, 72, -1, 74, -1 },
    { 76, -1, -1, 79, -1, -1, 76, -1, 74, -1, 72, -1, 74, -1, 76, -1 },
    { 74, -1, -1, 72, -1, -1, 69, -1, 67, -1, 69, -1, 71, -1, -1, -1 }
} };
}

qreal Music::stepDuration(qreal bpm)
{
    return 60.0 / std::max(bpm, 30.0) / 4.0;
}

QList<SoundEffects::Tone> Music::step(int index, int intensity, qreal duration, qreal volume)
{
    constexpr auto Noise = SoundEffects::Noise;
    constexpr auto Sine = SoundEffects::Sine;
    constexpr auto Square = SoundEffects::Square;
    constexpr auto Triangle = SoundEffects::Triangle;

    index = ((index % loopSteps) + loopSteps) % loopSteps;
    intensity = std::clamp(intensity, 0, maxIntensity);
    const int bar = index / stepsPerBar;
    const int beat = index % stepsPerBar;
    const Chord chord = chords.at(bar);

    QList<Tone> tones;
    const auto note = [&](SoundEffects::Waveform waveform, int midi, qreal steps, qreal volume) {
        const qreal f = frequency(midi);
        tones.append({ waveform, 0.0, steps * duration, f, f, volume });
    };

    // Bass: half notes at first, driving eighths with the octave later
    if (intensity == 0) {
        if (beat % 8 == 0)
            note(Triangle, chord.root, 7.5, 0.16);
    }
    else if (beat % 2 == 0) {
        note(Triangle, chord.root + (beat % 4 == 2 ? 12 : 0), 1.8, 0.15);
    }

    // Hi-hats on the off beats, later on every eighth
    if (intensity >= 1 && (beat % 4 == 2 || (intensity >= 3 && beat % 2 == 0)))
        tones.append({ Noise, 0.0, 0.04, 9000, 9000, 0.05 });

    // Kick on the beats, snare on two and four
    if (intensity >= 2) {
        if (beat % 8 == 0 || (beat == 10 && bar % 2 == 1))
            tones.append({ Sine, 0.0, 0.14, 150, 45, 0.3 });
        if (beat == 4 || beat == 12)
            tones.append({ Noise, 0.0, 0.12, 3500, 3500, 0.12 });
    }

    // Arpeggio up and down the chord
    if (intensity >= 3) {
        static constexpr std::array<int, 4> pattern = { 0, 1, 2, 1 };
        const std::array<int, 3> triad = { 0, chord.third, 7 };
        note(Square, chord.root + 24 + triad.at(pattern.at(beat % 4)), 0.9, 0.035);
    }

    if (intensity >= 4 && lead.at(bar).at(beat) >= 0)
        note(Square, lead.at(bar).at(beat), 2.5, 0.06);

    volume = std::clamp(volume, 0.0, 1.0);
    for (Tone& tone : tones)
        tone.volume *= volume;
    tones.removeIf([](const Tone& tone) { return tone.volume <= 0.0; });
    return tones;
}
