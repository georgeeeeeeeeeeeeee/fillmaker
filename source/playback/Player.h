#pragma once
// Playback, on the audio thread. The message thread turns a Pattern into a flat Snapshot and
// publishes it through a triple buffer (lock-free, no allocation on the audio thread); a Player turns
// the snapshot into note-ons and note-offs, sample-accurately, from a musical position in beats (the
// host's PPQ for MIDI out, or the audition clock).
//
// No duplicates: each block plays the window [blockStart, blockEnd) of beats, and a block that
// repeats part of the last window (some hosts do) only plays what's new; a jump back (a loop) starts
// afresh. No stuck notes: every note-on has its note-off scheduled in samples; a retrigger of a
// sounding pitch ends it first; stop() ends everything, on the channel each note was started on.

#include "engine/Pattern.h"
#include "engine/Random.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

namespace fm
{
struct PlayNote
{
    double on = 0.0, off = 0.0;   // beats from the pattern start
    uint8_t pitch = 0, velocity = 0;
    float probability = 1.0f;
};

struct Snapshot
{
    static constexpr int kMax = Pattern::kMaxEvents;
    std::array<PlayNote, kMax> notes {};
    int count = 0;
    double length = 4.0;
    uint32_t version = 0;
    static void fill (Snapshot&, const Pattern&, uint32_t version);
};

// One writer (message thread), one reader (audio thread). The reader always gets the newest complete
// snapshot; nobody waits.
class SnapshotExchange
{
public:
    void publish (const Pattern& p, uint32_t version);   // writer
    const Snapshot& read() noexcept;                     // reader

private:
    std::array<Snapshot, 3> buf_ {};
    std::atomic<int> middle_ { 1 };   // index | 4 when fresh
    int back_ = 2, front_ = 0;
};

class Player
{
public:
    struct Placement
    {
        double anchor = 0.0;       // where (in source beats) pattern repetitions are counted from
        double phraseBeats = 0.0;  // 0: loop the pattern; > 0: play it once at the end of each phrase of this many beats
        bool once = false;         // play one pass from the anchor, then nothing (audition without loop)
    };

    void prepare (double sampleRate) noexcept;
    void setChannel (int channel) noexcept { channel_ = std::clamp (channel, 1, 16); }
    // One block: the source is at `beat` at the block's first sample, moving at `bpm`.
    void render (const Snapshot&, double beat, double bpm, int numSamples, juce::MidiBuffer& out, const Placement&, Rng&) noexcept;
    void stop (juce::MidiBuffer& out, int sample = 0) noexcept;   // every sounding note off
    bool anyActive() const noexcept;
    bool running() const noexcept { return running_; }
    double patternPosition() const noexcept { return position_; }   // where it is in the pattern (-1: not in it)
    bool finished() const noexcept { return finished_; }            // a `once` pass has ended

private:
    struct Active { int64_t offAt = 0; uint8_t channel = 1; bool on = false; };
    void noteOff (juce::MidiBuffer&, int pitch, int sample) noexcept;
    std::array<Active, 128> active_ {};
    double sr_ = 48000.0, lastEnd_ = -1.0e18, position_ = -1.0;
    int64_t clock_ = 0;
    int channel_ = 1;
    bool running_ = false, finished_ = false;
};
} // namespace fm
