#ifndef MUSIC_H
#define MUSIC_H

#include "soundeffects.h"

// A procedural chiptune loop over four chords, made of the same tones as
// the sound effects. The layers come in with the intensity: a bass line,
// then hi-hats and a busier bass, drums, an arpeggio and a lead.
class Music
{
public:
    static constexpr int stepsPerBar = 16;
    static constexpr int bars = 4;
    static constexpr int loopSteps = stepsPerBar * bars;
    static constexpr int maxIntensity = 4;

    // Seconds of a sixteenth step at bpm
    static qreal stepDuration(qreal bpm);

    // Tones of the step, starting relative to its beginning
    static QList<SoundEffects::Tone> step(int index, int intensity, qreal stepDuration);
};

#endif // MUSIC_H
