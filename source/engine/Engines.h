#pragma once
// The five generation engines. Each fills one span of the fill with its own logic; the generator
// decides which engine plays which span (or layers two) and then shapes velocity and timing.
//
//   Probability  weighted chances per grid step: metric strength, density, grammar, groove, what just
//                happened (clusters, rolls, silences), flams
//   Euclidean    evenly spread pulses (Bjorklund) over steps that needn't match the bar; a second
//                cycle against the first at higher complexity
//   Ratchet      repeats inside an interval: 2..13 hits, irregular groupings (7 = 3+2+2), ramps,
//                accelerating spacing
//   Permutation  a motif (made fresh, or borrowed from the groove) rearranged: rotated, reversed,
//                halves swapped, its intervals shuffled, pitches permuted, fragmented
//   Polyrhythm   competing cycles (3:4, 5:4, 7:8, 11:16 ...) anchored to the pattern start, so they run
//                across bar lines; or a repeating 3/5/7-step grouping against the meter

#include "engine/Grammar.h"
#include "engine/Pitch.h"
#include "engine/Rhythm.h"

namespace fm
{
struct Proto
{
    double time = 0.0;      // the rhythmic position (beats from the pattern start)
    double offset = 0.0;    // displacement off it
    double length = 0.1;
    int pitch = 36;
    double level = 1.0;     // velocity multiplier hint (ratchet ramps, layers, graces)
    bool accent = false;    // the engine wants this one accented
    bool ghostable = true;  // may become a ghost note
    double metric = 0.5;
    double step = 0.25;     // the grid step it sits on
    int engine = 0;
};

struct Span
{
    double start = 0.0, length = 1.0;
    double stepsPerBeat = 4.0;
    int engine = kProbability;
    int secondary = -1;     // a second engine layered over it (-1: none)
    int index = 0;
    bool response = false;  // Call + Response: this span answers the one before
    juce::String note;      // what it did (for the structure readout)
};

struct Ctx
{
    const Settings& s;
    const GrammarPlan& g;
    PitchPicker& pitch;
    const GrooveProfile* groove;
    double L = 4.0, bar = 4.0;
    double syncopation = 0.0;   // 0..1, from complexity and behaviour
    double densityScale = 1.0;  // a layered engine plays quieter and sparser

    double t (double beat) const noexcept { return L > 0.0 ? beat / L : 0.0; }
    double density (double beat) const noexcept;
    double weight (double beat, double metric) const noexcept;
    bool gap (double beat) const noexcept { return g.inGap (beat); }
    double displacement (double beat, double step, Rng&) const noexcept;
    double noteLength (double step, Rng&) const noexcept;
    bool groovy() const noexcept { return groove != nullptr && s.grooveAware && groove->valid(); }
};

void probabilityEngine (const Ctx&, Span&, Rng&, std::vector<Proto>&);
void euclideanEngine (const Ctx&, Span&, Rng&, std::vector<Proto>&);
void ratchetEngine (const Ctx&, Span&, Rng&, std::vector<Proto>&);
void permutationEngine (const Ctx&, Span&, Rng&, std::vector<Proto>&);
void polyrhythmEngine (const Ctx&, Span&, Rng&, std::vector<Proto>&);

// Shared with mutation and Call + Response.
enum class PermOp { identity = 0, rotate, reverse, swapHalves, intervalShuffle, pitchShuffle, fragment };
const char* nameOf (PermOp);
// A motif's events (times relative to its start, 0..len) rearranged. Onsets move as groups (notes on
// the same moment stay together), accents travel with their notes; intervalShuffle keeps the set of
// gaps between onsets and only changes their order.
std::vector<Proto> permute (std::vector<Proto> motif, double len, PermOp, Rng&, double step);
PermOp pickPermutation (Rng&, double behaviour, double complexity, bool allowIdentity);
int pickRatchetCount (Rng&, double complexity, double behaviour, int maxCount);
// `count` hits inside [start, start + dur): optionally grouped irregularly, ramped and accelerating.
void addRatchet (std::vector<Proto>&, double start, double dur, int count, int pitch, const Ctx&, Rng&, bool movePitch);
} // namespace fm
