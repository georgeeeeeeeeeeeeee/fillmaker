#pragma once
// The pattern: what Generate makes, the editor shows, Audition and MIDI out play, and export writes.
// It knows nothing of the plug-in, the UI or playback; times are musical (beats = quarter notes) in
// double precision, so any subdivision (quintuplets, a 13-note ratchet, a 7-step cycle) is exact
// enough, far finer than any grid.

#include <juce_core/juce_core.h>

#include <cstdint>
#include <vector>

namespace fm
{
struct Event
{
    uint32_t id = 0;          // stable through edits and mutation (0 until added to a pattern)
    double start = 0.0;       // beats from the pattern start: the rhythmic position
    double offset = 0.0;      // timing displacement (swing, push, pull), beats; plays at start + offset
    double length = 0.125;    // beats
    int pitch = 36;           // 0..127, any meaning (a kit, samples, a synth)
    int velocity = 100;       // 1..127
    float probability = 1.0f; // chance it plays on each pass (1 = always)
    bool accent = false;

    double time() const noexcept { return start + offset; }
    double end() const noexcept { return time() + length; }
    bool operator== (const Event&) const = default;
};

struct Pattern
{
    static constexpr int kMaxEvents = 1024;
    static constexpr double kMinLength = 1.0 / 256.0;   // shortest note, beats

    double length = 4.0;        // beats
    double beatsPerBar = 4.0;   // for display and bar-aware generation
    std::vector<Event> events;
    uint32_t nextId = 1;
    uint32_t seed = 0;          // the seed it was generated from (0: made by hand)

    Event& add (Event e);                 // gives it an id
    Event* find (uint32_t id) noexcept;
    bool remove (uint32_t id);
    void sort();                          // by time, then pitch
    // Keeps every event valid: pitches 0..127, velocities 1..127, probabilities 0..1, times inside the
    // pattern, lengths at least kMinLength and not past the end; drops exact duplicates (same pitch,
    // same time) and anything beyond kMaxEvents (the quietest go first). Sorts.
    void sanitise();
    double density() const noexcept { return length > 0.0 ? (double) events.size() / length : 0.0; }   // events per beat

    juce::var toVar() const;
    static Pattern fromVar (const juce::var&);
    juce::String toJson() const;
    static Pattern fromJson (const juce::String&);
    bool operator== (const Pattern&) const = default;
};
} // namespace fm
