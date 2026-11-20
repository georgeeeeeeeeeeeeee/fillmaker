#pragma once
// Groove-aware mode: what a source drum pattern does, measured, so a fill can answer it.

#include "engine/Pattern.h"

#include <utility>
#include <vector>

namespace fm
{
struct GrooveProfile
{
    static constexpr int kSlotsPerBeat = 12;   // fine enough for 16ths and triplets alike
    double beatsPerBar = 4.0;
    int events = 0;
    double bars = 0.0;
    std::vector<double> onset;      // per slot of one bar: how often a hit lands there (0..1, folded over bars)
    std::vector<double> velocity;   // per slot: mean velocity there (0 when empty)
    double density = 0.0;           // hits per beat
    double ternary = 0.0;           // 0..1: share of hits on triplet-only positions (a shuffled or 12/8 feel)
    double fine = 0.0;              // 0..1: share of hits on 32nds or finer
    double meanVelocity = 90.0;
    double repetition = 0.0;        // 0..1: how alike its bars are
    std::vector<std::pair<int, double>> pitches;   // pitch, share of hits (most used first)
    Pattern source;                 // the analysed notes (Permutation borrows motifs from it)

    bool valid() const noexcept { return events >= 2; }
    int slots() const noexcept { return (int) onset.size(); }
    double onsetAt (double beat) const noexcept;    // the onset share nearest that position in the bar
    double emptiness (double beat) const noexcept;  // 1 where the groove leaves space
    juce::String describe() const;
};

GrooveProfile analyseGroove (const Pattern& source);

// The MIDI-file fallback for hosts that don't send MIDI to the plug-in: every note in the file, from
// its first bar, as a pattern (lengths kept, all channels).
Pattern importMidiFile (const juce::File&, juce::String* error = nullptr, double* bpm = nullptr);
} // namespace fm
