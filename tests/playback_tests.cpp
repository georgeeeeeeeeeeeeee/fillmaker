// Playback on the audio thread's terms: blocks of samples, a moving musical position.
#include "engine/Generator.h"
#include "engine/Presets.h"
#include "playback/Player.h"
#include "playback/PreviewSynth.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <map>

using namespace fm;

namespace
{
struct Ev { long long sample; bool on; int pitch, vel, ch; };

// Runs a player over `seconds` of a host transport at `bpm` from `startBeat`, in blocks of `block`,
// and returns every event with its absolute sample.
std::vector<Ev> play (Player& pl, const Snapshot& snap, double bpm, double seconds, int block, double startBeat = 0.0,
                      Player::Placement place = {}, double sr = 48000.0, long long* clock = nullptr)
{
    std::vector<Ev> out;
    Rng rng (1);
    const long long total = (long long) (seconds * sr);
    long long at = clock ? *clock : 0;
    double beat = startBeat;
    for (long long done = 0; done < total; done += block)
    {
        const int n = (int) std::min<long long> (block, total - done);
        juce::MidiBuffer mb;
        pl.render (snap, beat, bpm, n, mb, place, rng);
        for (const auto m : mb)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOnOrOff()) out.push_back ({ at + m.samplePosition, msg.isNoteOn(), msg.getNoteNumber(), msg.getVelocity(), msg.getChannel() });
        }
        beat += n * bpm / 60.0 / sr;
        at += n;
    }
    if (clock) *clock = at;
    return out;
}

Pattern simple()
{
    Pattern p;
    p.length = 4.0;
    for (int i = 0; i < 4; ++i) { Event e; e.start = i; e.length = 0.25; e.pitch = 36 + i; e.velocity = 100; p.add (e); }
    { Event e; e.start = 1.5 + 1.0 / 5.0; e.length = 0.1; e.pitch = 50; e.velocity = 77; p.add (e); }   // a quintuplet position
    p.sanitise();
    return p;
}

// Every note-on has a matching note-off later, and nothing is still on at the end.
bool balanced (const std::vector<Ev>& ev)
{
    std::map<std::pair<int, int>, int> on;
    for (const auto& e : ev)
    {
        auto& c = on[{ e.ch, e.pitch }];
        if (e.on) { if (c != 0) return false; c = 1; }   // never two note-ons on a pitch without an off between
        else c = 0;
    }
    for (const auto& [k, c] : on) if (c != 0) return false;
    return true;
}
} // namespace

TEST_CASE ("notes land on their exact samples, whatever the block size")
{
    Snapshot snap;
    Snapshot::fill (snap, simple(), 1);
    const double sr = 48000.0, bpm = 120.0;   // a beat is 24000 samples
    std::vector<std::vector<Ev>> runs;
    for (int block : { 64, 256, 441, 512, 1024, 4096 })
    {
        Player pl;
        pl.prepare (sr);
        auto ev = play (pl, snap, bpm, 4.0, block);   // two passes of the pattern
        juce::MidiBuffer mb;
        pl.stop (mb);
        runs.push_back (ev);
    }
    for (size_t i = 1; i < runs.size(); ++i)
    {
        REQUIRE (runs[i].size() == runs[0].size());
        for (size_t k = 0; k < runs[0].size(); ++k) { CHECK (runs[i][k].sample == runs[0][k].sample); CHECK (runs[i][k].pitch == runs[0][k].pitch); }
    }
    std::vector<long long> ons;
    for (const auto& e : runs[0]) if (e.on) ons.push_back (e.sample);
    REQUIRE (ons.size() == 10);
    CHECK (ons[0] == 0);
    CHECK (ons[1] == 24000);
    CHECK (ons[2] == 24000 + 12000 + 4800);   // beat 1.7 (a fifth of a beat after 1.5)
    CHECK (ons[5] == 96000);                 // the second pass
    // The note-off is the note's length later (0.25 beat = 6000 samples).
    CHECK (runs[0][1].sample == 6000);
    CHECK (! runs[0][1].on);
}

TEST_CASE ("every note-on has a note-off; stop leaves nothing hanging")
{
    auto s = Settings();
    s.pool = defaultPool();
    s.engines = { true, true, true, true, true };
    s.complexity = 1.0f;
    s.behaviour = 1.0f;
    s.density = 1.0f;
    s.ratchetMax = 13;
    for (uint32_t seed = 1; seed <= 20; ++seed)
    {
        Snapshot snap;
        Snapshot::fill (snap, generate (s, seed), seed);
        Player pl;
        pl.prepare (44100.0);
        long long clock = 0;
        auto ev = play (pl, snap, 137.0, 3.3, 333, 0.0, {}, 44100.0, &clock);
        juce::MidiBuffer mb;
        pl.stop (mb, 0);
        for (const auto m : mb) ev.push_back ({ clock, false, m.getMessage().getNoteNumber(), 0, m.getMessage().getChannel() });
        INFO ("seed " << seed);
        CHECK (balanced (ev));
        CHECK (! pl.anyActive());
    }
}

TEST_CASE ("a host repeating part of a block doesn't double notes; a loop jump plays them again")
{
    Snapshot snap;
    Snapshot::fill (snap, simple(), 1);
    Player pl;
    pl.prepare (48000.0);
    Rng rng (1);
    int ons = 0;
    const auto count = [&] (juce::MidiBuffer& mb) { for (const auto m : mb) ons += m.getMessage().isNoteOn(); };
    // Block 1 covers beat 0..0.0213; then the host sends the same position again.
    for (int rep = 0; rep < 3; ++rep)
    {
        juce::MidiBuffer mb;
        pl.render (snap, 0.0, 120.0, 512, mb, {}, rng);
        count (mb);
    }
    CHECK (ons == 1);
    // Play on to beat 3.5, then the loop brace jumps back to 0: beat 0's note plays again.
    double beat = 512 / 24000.0;
    while (beat < 3.5) { juce::MidiBuffer mb; pl.render (snap, beat, 120.0, 512, mb, {}, rng); count (mb); beat += 512 / 24000.0; }
    const int before = ons;
    juce::MidiBuffer mb;
    pl.render (snap, 0.0, 120.0, 512, mb, {}, rng);
    count (mb);
    CHECK (ons == before + 1);
}

TEST_CASE ("transport: starting mid-pattern plays from there; a tempo change keeps the musical timing")
{
    Snapshot snap;
    Snapshot::fill (snap, simple(), 1);
    Player pl;
    pl.prepare (48000.0);
    auto ev = play (pl, snap, 120.0, 0.6, 256, 1.9);   // from beat 1.9: next note on beat 2 (0.1 beat = 2400 samples)
    REQUIRE (! ev.empty());
    CHECK (ev[0].on);
    CHECK (ev[0].pitch == 38);
    CHECK (ev[0].sample == 2400);
    Player p2;
    p2.prepare (48000.0);
    auto ev2 = play (p2, snap, 60.0, 1.2, 256);   // at 60 BPM beat 1 is a second in
    std::vector<long long> ons;
    for (const auto& e : ev2) if (e.on) ons.push_back (e.sample);
    REQUIRE (ons.size() >= 2);
    CHECK (ons[1] == 48000);
}

TEST_CASE ("phrase placement: the fill plays only in the last bar of each 4-bar phrase")
{
    Snapshot snap;
    Snapshot::fill (snap, simple(), 1);
    Player pl;
    pl.prepare (48000.0);
    Player::Placement place;
    place.phraseBeats = 16.0;
    auto ev = play (pl, snap, 120.0, 17.0, 512, 0.0, place);   // 34 beats
    for (const auto& e : ev)
        if (e.on)
        {
            const double beat = e.sample / 24000.0;
            CHECK (std::fmod (beat, 16.0) >= 12.0 - 1.0e-9);
        }
    int ons = 0;
    for (const auto& e : ev) ons += e.on;
    CHECK (ons == 10);   // bars 4 and 8: two passes of five notes
}

TEST_CASE ("audition once: one pass, then finished")
{
    Snapshot snap;
    Snapshot::fill (snap, simple(), 1);
    Player pl;
    pl.prepare (48000.0);
    Player::Placement once;
    once.once = true;
    auto ev = play (pl, snap, 120.0, 5.0, 512, 0.0, once);
    int ons = 0;
    for (const auto& e : ev) ons += e.on;
    CHECK (ons == 5);
    CHECK (pl.finished());
}

TEST_CASE ("probability: a 50% note plays about half the passes")
{
    Pattern p;
    p.length = 1.0;
    Event e;
    e.start = 0.0;
    e.length = 0.1;
    e.pitch = 40;
    e.probability = 0.5f;
    p.add (e);
    Snapshot snap;
    Snapshot::fill (snap, p, 1);
    Player pl;
    pl.prepare (48000.0);
    auto ev = play (pl, snap, 240.0, 100.0, 512);   // 400 passes
    int ons = 0;
    for (const auto& x : ev) ons += x.on;
    CHECK (ons > 150);
    CHECK (ons < 250);
}

TEST_CASE ("the triple buffer hands over the newest snapshot")
{
    SnapshotExchange x;
    CHECK (x.read().count == 0);
    auto p = simple();
    x.publish (p, 1);
    p.events.pop_back();
    x.publish (p, 2);
    CHECK (x.read().version == 2);
    CHECK (x.read().count == 4);
    CHECK (x.read().version == 2);   // reading again doesn't go back
}

TEST_CASE ("the preview voice makes sound for notes and silence after them")
{
    PreviewSynth syn;
    syn.prepare (48000.0);
    juce::AudioBuffer<float> buf (2, 48000);
    buf.clear();
    juce::MidiBuffer mb;
    for (int p : { 24, 36, 50, 64, 80, 100, 127 }) mb.addEvent (juce::MidiMessage::noteOn (1, p, (juce::uint8) 120), 0);
    mb.addEvent (juce::MidiMessage::noteOff (1, 64), 2400);
    syn.render (buf, 0, 48000, mb);
    CHECK (buf.getMagnitude (0, 0, 4800) > 0.05f);
    CHECK (buf.getMagnitude (0, 0, 48000) <= 1.0f);
    juce::AudioBuffer<float> later (2, 48000);
    later.clear();
    syn.render (later, 0, 48000, {});
    CHECK (later.getMagnitude (0, 43200, 4800) < 1.0e-3f);   // all decayed within two seconds
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 48000; ++i) REQUIRE (std::isfinite (buf.getSample (ch, i)));
}
