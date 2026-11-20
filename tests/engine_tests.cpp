// The generator, offline: structure, engines, grammars, mutation, groove, serialisation, export.
#include "engine/Generator.h"
#include "engine/Groove.h"
#include "engine/MidiExport.h"
#include "engine/Mutation.h"
#include "engine/Presets.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <iostream>
#include <set>

using namespace fm;

namespace
{
Settings kit()
{
    Settings s;
    s.pool = defaultPool();
    for (auto& e : s.pool) e.enabled = true;
    return s;
}

juce::String whyInvalid (const Pattern& p)
{
    if (! std::isfinite (p.length) || p.length <= 0.0) return "length";
    std::set<uint32_t> ids;
    for (const auto& e : p.events)
    {
        if (e.pitch < 0 || e.pitch > 127 || e.velocity < 1 || e.velocity > 127) return "pitch/velocity";
        if (! (e.time() >= 0.0 && e.time() < p.length)) return "time " + juce::String (e.time()) + " of " + juce::String (p.length);
        if (! (e.length >= Pattern::kMinLength)) return "note length " + juce::String (e.length);
        if (e.end() > p.length + 1.0e-9) return "ends past the pattern: " + juce::String (e.end(), 9) + " > " + juce::String (p.length);
        if (! (e.probability >= 0.0f && e.probability <= 1.0f)) return "probability";
        if (e.id == 0 || ! ids.insert (e.id).second) return "ids";
    }
    if ((int) p.events.size() > Pattern::kMaxEvents) return "too many";
    for (size_t i = 1; i < p.events.size(); ++i) if (p.events[i].time() < p.events[i - 1].time()) return "order";
    return {};
}
bool valid (const Pattern& p) { const auto w = whyInvalid (p); if (w.isNotEmpty()) UNSCOPED_INFO ("invalid: " << w); return w.isEmpty(); }

// Equal within 1e-9 beat (JSON keeps doubles to about 15-17 digits).
bool same (const Pattern& a, const Pattern& b)
{
    if (a.events.size() != b.events.size() || std::abs (a.length - b.length) > 1e-9 || a.seed != b.seed) return false;
    for (size_t i = 0; i < a.events.size(); ++i)
    {
        const auto& x = a.events[i];
        const auto& y = b.events[i];
        if (x.id != y.id || x.pitch != y.pitch || x.velocity != y.velocity || x.accent != y.accent || std::abs (x.probability - y.probability) > 1e-6f
            || std::abs (x.start - y.start) > 1e-9 || std::abs (x.offset - y.offset) > 1e-9 || std::abs (x.length - y.length) > 1e-9)
        {
            UNSCOPED_INFO ("event " << i << " differs");
            return false;
        }
    }
    return true;
}

// Share of onsets off the 8th-note grid weighted by how weak their position is: a syncopation index.
double syncopation (const Pattern& p)
{
    if (p.events.empty()) return 0.0;
    double s = 0.0;
    for (const auto& e : p.events) s += 1.0 - metricWeight (e.start, p.beatsPerBar);
    return s / (double) p.events.size();
}

// Distinct inter-onset intervals (rounded to 1/960 beat): a measure of rhythmic variety.
int intervalKinds (const Pattern& p)
{
    std::set<long long> k;
    std::vector<double> on;
    for (const auto& e : p.events) if (on.empty() || e.time() - on.back() > 1.0e-6) on.push_back (e.time());
    for (size_t i = 1; i < on.size(); ++i) k.insert (std::llround ((on[i] - on[i - 1]) * 960.0));
    return (int) k.size();
}

double densityIn (const Pattern& p, double a, double b)
{
    int n = 0;
    for (const auto& e : p.events) if (e.time() >= a && e.time() < b) ++n;
    return n / (b - a);
}

std::string roll (const Pattern& p, int cols = 64)
{
    std::set<int> pitches;
    for (const auto& e : p.events) pitches.insert (e.pitch);
    std::string out;
    for (auto it = pitches.rbegin(); it != pitches.rend(); ++it)
    {
        std::string row ((size_t) cols, '.');
        for (int c = 0; c < cols; c += cols / (int) std::max (1.0, p.length)) row[(size_t) c] = '|';
        for (const auto& e : p.events)
            if (e.pitch == *it)
            {
                const int c = std::clamp ((int) (e.time() / p.length * cols), 0, cols - 1);
                row[(size_t) c] = e.velocity > 100 ? '#' : e.velocity > 60 ? 'o' : '-';
            }
        out += juce::String (*it).paddedLeft (' ', 4).toStdString() + " " + row + "\n";
    }
    return out;
}
} // namespace

TEST_CASE ("the same settings and seed give the same fill; another seed gives another")
{
    auto s = kit();
    s.engines = { true, true, true, true, true };
    s.grammars[kBuild] = s.grammars[kBurst] = true;
    const auto a = generate (s, 42), b = generate (s, 42), c = generate (s, 43);
    REQUIRE (valid (a));
    CHECK (a == b);
    CHECK (! (a == c));
    CHECK (a.seed == 42);
}

TEST_CASE ("pattern lengths follow the length setting and the meter")
{
    auto s = kit();
    const std::pair<LengthChoice, double> cases[] = { { LengthChoice::quarterBar, 1.0 }, { LengthChoice::halfBar, 2.0 }, { LengthChoice::oneBar, 4.0 },
                                                      { LengthChoice::twoBars, 8.0 }, { LengthChoice::fourBars, 16.0 } };
    for (auto [choice, beats] : cases)
    {
        s.lengthChoice = choice;
        const auto p = generate (s, 7);
        CHECK (p.length == beats);
        CHECK (valid (p));
    }
    s.lengthChoice = LengthChoice::custom;
    s.customLength = 5.5;
    CHECK (generate (s, 7).length == 5.5);
    s.lengthChoice = LengthChoice::oneBar;
    s.beatsPerBar = 3.0;   // 3/4
    CHECK (generate (s, 7).length == 3.0);
}

TEST_CASE ("density: low is sparse with space, high is dense")
{
    auto s = kit();
    s.engines = { true, false, false, false, false };
    double lo = 0.0, hi = 0.0;
    for (uint32_t seed = 1; seed <= 20; ++seed)
    {
        s.density = 0.08f;
        lo += generate (s, seed).events.size();
        s.density = 0.92f;
        hi += generate (s, seed).events.size();
    }
    lo /= 20.0;
    hi /= 20.0;
    INFO ("mean notes per bar: low " << lo << ", high " << hi);
    CHECK (lo <= 4.0);
    CHECK (hi >= 12.0);
    s.density = 0.0f;
    CHECK (generate (s, 3).events.size() >= 1);   // never empty
}

TEST_CASE ("behaviour changes the generation itself: unhinged is more syncopated and more varied")
{
    auto s = kit();
    s.engines = { true, true, true, true, true };
    s.complexity = 0.6f;
    double synConv = 0.0, synWild = 0.0;
    int varConv = 0, varWild = 0;
    for (uint32_t seed = 1; seed <= 30; ++seed)
    {
        s.behaviour = 0.0f;
        const auto a = generate (s, seed);
        s.behaviour = 1.0f;
        const auto b = generate (s, seed);
        synConv += syncopation (a);
        synWild += syncopation (b);
        varConv += intervalKinds (a);
        varWild += intervalKinds (b);
        REQUIRE (valid (a));
        REQUIRE (valid (b));
    }
    INFO ("syncopation " << synConv / 30 << " vs " << synWild / 30 << "; interval kinds " << varConv / 30.0 << " vs " << varWild / 30.0);
    CHECK (synWild > synConv * 1.1);
    CHECK (varWild > varConv);
}

TEST_CASE ("resolutions: notes sit on quintuplet and septuplet positions; the grid isn't forced to 16ths")
{
    auto s = kit();
    s.engines = { true, false, false, false, false };
    s.complexity = 0.0f;
    s.behaviour = 0.0f;
    s.density = 0.9f;
    for (auto [res, n] : { std::pair { Resolution::quintuplet, 5 }, std::pair { Resolution::septuplet, 7 }, std::pair { Resolution::triplet, 6 } })
    {
        s.resolution = res;
        const auto p = generate (s, 11);
        int on = 0, off16 = 0;
        for (const auto& e : p.events)
        {
            const double k = e.start * n;
            on += std::abs (k - std::round (k)) < 1.0e-6;
            const double q = e.start * 4.0;
            off16 += std::abs (q - std::round (q)) > 1.0e-6;
        }
        INFO (stringOf (res));
        CHECK (on == (int) p.events.size());
        CHECK (off16 > 0);
    }
    s.resolution = Resolution::custom;
    s.customSubdivision = 13;
    const auto p = generate (s, 3);
    for (const auto& e : p.events) CHECK (std::abs (e.start * 13 - std::round (e.start * 13)) < 1.0e-6);
}

TEST_CASE ("euclid: Bjorklund patterns")
{
    const auto str = [] (const std::vector<bool>& v) { std::string s; for (bool b : v) s += b ? 'x' : '.'; return s; };
    CHECK (str (euclid (3, 8)) == "x..x..x.");
    CHECK (str (euclid (5, 8)) == "x.xx.xx.");
    CHECK (str (euclid (4, 16)) == "x...x...x...x...");
    CHECK (str (euclid (3, 8, 1)) == ".x..x..x");
    CHECK (str (euclid (0, 5)) == ".....");
    CHECK (str (euclid (7, 7)) == "xxxxxxx");
    int pulses = 0;
    for (bool b : euclid (11, 16)) pulses += b;
    CHECK (pulses == 11);
    // The Euclidean engine with fixed steps and pulses: exactly those onsets.
    auto s = kit();
    s.engines = { false, true, false, false, false };
    s.lengthChoice = LengthChoice::quarterBar;   // one span of one beat... at most two layers
    s.euclidSteps = 8;
    s.euclidPulses = 3;
    s.complexity = 0.0f;
    s.behaviour = 0.0f;
    const auto p = generate (s, 5);
    std::set<long long> on;
    for (const auto& e : p.events) on.insert (std::llround (e.start * 8.0));
    CHECK (on.size() <= 3);
    for (auto k : on) CHECK ((k == 0 || k == 3 || k == 6));
}

TEST_CASE ("ratchets: the counts asked for, inside their interval, ramped")
{
    auto s = kit();
    GrammarPlan none;
    none.length = 4.0;
    PitchPicker pp (s, nullptr);
    Ctx c { s, none, pp, nullptr, 4.0, 4.0 };
    Rng rng (9);
    for (int n : kRatchetCounts)
    {
        std::vector<Proto> out;
        addRatchet (out, 1.0, 0.5, n, 40, c, rng, false);
        REQUIRE ((int) out.size() == n);
        for (const auto& p : out)
        {
            CHECK (p.time >= 1.0 - 1.0e-9);
            CHECK (p.time < 1.5);
            CHECK (p.pitch == 40);
        }
    }
    // The engine at high complexity and behaviour reaches the odd counts.
    s.engines = { false, false, true, false, false };
    s.complexity = 0.9f;
    s.behaviour = 0.9f;
    s.ratchetMax = 13;
    std::set<int> seen;
    Rng r2 (1);
    for (int i = 0; i < 300; ++i) seen.insert (pickRatchetCount (r2, 0.9, 0.9, 13));
    CHECK (seen.count (11) + seen.count (13) + seen.count (9) > 0);
    Rng r3 (1);
    for (int i = 0; i < 300; ++i) CHECK (pickRatchetCount (r3, 0.0, 0.0, 4) <= 4);
}

TEST_CASE ("permutations keep the motif's material: same hits, same accents, same set of gaps")
{
    std::vector<Proto> m;
    const double times[] = { 0.0, 0.25, 0.75, 1.0, 1.5 };
    for (int i = 0; i < 5; ++i) { Proto p; p.time = times[i]; p.pitch = 36 + i; p.accent = i == 2; p.step = 0.25; m.push_back (p); }
    Rng rng (3);
    for (auto op : { PermOp::rotate, PermOp::reverse, PermOp::swapHalves, PermOp::intervalShuffle, PermOp::pitchShuffle })
    {
        const auto out = permute (m, 2.0, op, rng, 0.25);
        INFO (nameOf (op));
        REQUIRE (out.size() == m.size());
        int accents = 0;
        std::multiset<int> pitches;
        for (const auto& p : out) { accents += p.accent; pitches.insert (p.pitch); CHECK (p.time >= 0.0); CHECK (p.time < 2.0); }
        CHECK (accents == 1);
        CHECK (pitches == std::multiset<int> { 36, 37, 38, 39, 40 });
    }
    // Interval shuffle: the multiset of gaps (with the wrap) is unchanged.
    const auto gapsOf = [] (std::vector<Proto> v) {
        std::sort (v.begin(), v.end(), [] (const Proto& a, const Proto& b) { return a.time < b.time; });
        std::multiset<long long> g;
        for (size_t i = 0; i < v.size(); ++i) g.insert (std::llround (((i + 1 < v.size() ? v[i + 1].time : v[0].time + 2.0) - v[i].time) * 960));
        return g;
    };
    CHECK (gapsOf (permute (m, 2.0, PermOp::intervalShuffle, rng, 0.25)) == gapsOf (m));
}

TEST_CASE ("polyrhythm: a 5:4 cross over one bar puts 5 evenly spaced hits on one pitch")
{
    auto s = kit();
    s.engines = { false, false, false, false, true };
    s.polyRatio = 2;   // 5:4
    s.complexity = 0.0f;
    s.behaviour = 0.0f;
    s.density = 1.0f;
    bool found = false;
    for (uint32_t seed = 1; seed < 40 && ! found; ++seed)
    {
        GenerationReport rep;
        const auto p = generate (s, seed, nullptr, &rep);
        if (! rep.summary().contains ("5:4")) continue;
        std::map<int, std::vector<double>> by;
        for (const auto& e : p.events) by[e.pitch].push_back (e.start);
        for (const auto& [pitch, ts] : by)
        {
            if (ts.size() != 5) continue;
            bool even = true;
            for (size_t i = 0; i < 5; ++i) even = even && std::abs (ts[i] - i * 0.8) < 1.0e-6;
            found = found || even;
        }
    }
    CHECK (found);
}

TEST_CASE ("grammars shape the fill: Build ends denser, Release starts denser, Interruption leaves its gaps empty")
{
    auto s = kit();
    s.engines = { true, false, true, false, false };
    s.lengthChoice = LengthChoice::twoBars;
    s.density = 0.55f;
    double bFirst = 0, bLast = 0, rFirst = 0, rLast = 0;
    for (uint32_t seed = 1; seed <= 30; ++seed)
    {
        s.grammars = {};
        s.grammars[kBuild] = true;
        auto p = generate (s, seed);
        bFirst += densityIn (p, 0, 4);
        bLast += densityIn (p, 4, 8);
        s.grammars = {};
        s.grammars[kRelease] = true;
        p = generate (s, seed);
        rFirst += densityIn (p, 0, 4);
        rLast += densityIn (p, 4, 8);
    }
    CHECK (bLast > bFirst * 1.4);
    CHECK (rFirst > rLast * 1.4);

    s.grammars = {};
    s.grammars[kInterruption] = true;
    s.behaviour = 0.8f;
    for (uint32_t seed = 1; seed <= 20; ++seed)
    {
        Rng planRng = Rng (seed, 0x5eed).fork (1);
        const auto plan = GrammarPlan::make (s, s.lengthBeats(), planRng);
        REQUIRE (! plan.gaps.empty());
        const auto p = generate (s, seed);
        for (const auto& e : p.events) CHECK (! plan.inGap (e.start));
    }
}

TEST_CASE ("Acceleration shortens the gaps between hits; Deceleration lengthens them")
{
    auto s = kit();
    s.engines = { true, false, false, false, false };
    s.density = 1.0f;
    s.complexity = 0.0f;
    s.behaviour = 0.0f;
    s.lengthChoice = LengthChoice::twoBars;
    const auto meanGap = [] (const Pattern& p, double a, double b) {
        std::vector<double> on;
        for (const auto& e : p.events) if (e.start >= a && e.start < b && (on.empty() || e.start - on.back() > 1e-6)) on.push_back (e.start);
        return on.size() > 1 ? (on.back() - on.front()) / (on.size() - 1) : 1.0;
    };
    double accFirst = 0, accLast = 0;
    for (uint32_t seed = 1; seed <= 10; ++seed)
    {
        s.grammars = {};
        s.grammars[kAcceleration] = true;
        const auto p = generate (s, seed);
        accFirst += meanGap (p, 0, 3);
        accLast += meanGap (p, 5, 8);
    }
    CHECK (accLast < accFirst * 0.6);
    double decFirst = 0, decLast = 0;
    for (uint32_t seed = 1; seed <= 10; ++seed)
    {
        s.grammars = {};
        s.grammars[kDeceleration] = true;
        const auto p = generate (s, seed);
        decFirst += meanGap (p, 0, 3);
        decLast += meanGap (p, 5, 8);
    }
    CHECK (decLast > decFirst * 1.6);
}

TEST_CASE ("pitch: only pool pitches inside the range; motions move the way they say")
{
    auto s = kit();
    s.pitchLow = 38;
    s.pitchHigh = 44;
    s.engines = { true, true, true, true, true };
    for (uint32_t seed = 1; seed <= 30; ++seed)
    {
        s.behaviour = (float) ((seed % 3) * 0.5);
        for (const auto& e : generate (s, seed).events)
        {
            CHECK (e.pitch >= 38);
            CHECK (e.pitch <= 44);
        }
    }
    // No pool: anything in the range, including the extremes of MIDI.
    Settings open;
    open.pitchLow = 0;
    open.pitchHigh = 127;
    for (const auto& e : generate (open, 1).events) CHECK ((e.pitch >= 0 && e.pitch <= 127));
    // A weight of zero is never chosen; per-pitch probability is stored on the notes.
    s = kit();
    for (auto& e : s.pool) e.weight = e.pitch == 40 ? 1.0f : 0.0f;
    s.pool[4].probability = 0.5f;   // pitch 40
    for (const auto& e : generate (s, 2).events) { CHECK (e.pitch == 40); CHECK (e.probability == 0.5f); }
    // Descending motion: within a phrase, pitches go down (with wrap-arounds).
    s = kit();
    s.motion = PitchMotion::descending;
    s.engines = { true, false, false, false, false };
    s.density = 0.9f;
    s.behaviour = 0.0f;
    const auto p = generate (s, 4);
    int down = 0, up = 0;
    for (size_t i = 1; i < p.events.size(); ++i)
    {
        if (p.events[i].pitch < p.events[i - 1].pitch) ++down;
        if (p.events[i].pitch > p.events[i - 1].pitch) ++up;
    }
    CHECK (down > up);
}

TEST_CASE ("velocity: inside 1..127, accents louder than ghosts, crescendo rises")
{
    auto s = kit();
    s.accent = 1.0f;
    s.ghost = 1.0f;
    s.engines = { true, true, true, true, true };
    double acc = 0, ghost = 0;
    int na = 0, ng = 0;
    for (uint32_t seed = 1; seed <= 20; ++seed)
        for (const auto& e : generate (s, seed).events)
        {
            CHECK (e.velocity >= 1);
            CHECK (e.velocity <= 127);
            if (e.accent) { acc += e.velocity; ++na; }
            else if (e.velocity < 50) { ghost += e.velocity; ++ng; }
        }
    REQUIRE (na > 0);
    REQUIRE (ng > 0);
    CHECK (acc / na > ghost / ng + 40);
    s = kit();
    s.curve = VelocityCurve::crescendo;
    s.curveDepth = 1.0f;
    s.accent = 0.0f;
    s.ghost = 0.0f;
    s.velocityVariation = 0.0f;
    s.engines = { true, false, false, false, false };
    s.density = 0.9f;
    const auto p = generate (s, 8);
    double first = 0, last = 0;
    int nf = 0, nl = 0;
    for (const auto& e : p.events) { if (e.time() < 1.0) { first += e.velocity; ++nf; } if (e.time() >= 3.0) { last += e.velocity; ++nl; } }
    CHECK (last / nl > first / nf + 25);
}

TEST_CASE ("mutation: high preservation keeps the fill, low reinvents it; a region limits it; seeds repeat")
{
    auto s = kit();
    s.engines = { true, true, true, true, true };
    s.lengthChoice = LengthChoice::twoBars;
    double keep = 0, reinvent = 0;
    for (uint32_t seed = 1; seed <= 20; ++seed)
    {
        const auto p = generate (s, seed);
        MutationSettings m;
        m.preservation = 1.0f;
        keep += similarity (p, mutate (p, s, m, seed));
        m.preservation = 0.0f;
        reinvent += similarity (p, mutate (p, s, m, seed));
        REQUIRE (valid (mutate (p, s, m, seed)));
    }
    keep /= 20;
    reinvent /= 20;
    INFO ("similarity: preservation 100% " << keep << ", 0% " << reinvent);
    CHECK (keep > 0.85);
    CHECK (reinvent < 0.6);

    const auto p = generate (s, 5);
    MutationSettings m;
    m.preservation = 0.0f;
    m.regionStart = 4.0;
    m.regionEnd = 8.0;
    const auto q = mutate (p, s, m, 9);
    // The first bar is untouched.
    std::vector<Event> a, b;
    for (const auto& e : p.events) if (e.start < 4.0) a.push_back (e);
    for (const auto& e : q.events) if (e.start < 4.0) b.push_back (e);
    CHECK (a == b);
    CHECK (mutate (p, s, m, 9) == q);
}

TEST_CASE ("groove-aware: a fill related to the groove, using its pitches and spaces")
{
    // A straight rock groove: hats (42) on 8ths, kick (36) on 1 and 3, snare (38) on 2 and 4, two bars.
    Pattern g;
    g.length = 8.0;
    for (int bar = 0; bar < 2; ++bar)
    {
        for (int i = 0; i < 8; ++i) { Event e; e.start = bar * 4 + i * 0.5; e.pitch = 42; e.velocity = 70; g.add (e); }
        for (double b : { 0.0, 2.0 }) { Event e; e.start = bar * 4 + b; e.pitch = 36; e.velocity = 110; g.add (e); }
        for (double b : { 1.0, 3.0 }) { Event e; e.start = bar * 4 + b; e.pitch = 38; e.velocity = 105; g.add (e); }
    }
    g.sanitise();
    const auto prof = analyseGroove (g);
    CHECK (prof.valid());
    CHECK (prof.events == 24);
    CHECK (prof.repetition > 0.99);
    CHECK (prof.ternary < 0.01);
    CHECK (prof.pitches[0].first == 42);
    CHECK (prof.onsetAt (0.0) > 0.99);
    CHECK (prof.emptiness (0.0) < 0.05);     // on a hit
    CHECK (prof.emptiness (0.25) > 0.5);     // the 16th between two 8th hats: room for new material
    Settings s;   // no pool: the groove's pitches come in
    s.grooveAware = true;
    s.grooveStrength = 1.0f;
    s.engines = { true, false, false, true, false };
    std::set<int> used;
    for (uint32_t seed = 1; seed <= 10; ++seed)
        for (const auto& e : generate (s, seed, &prof).events) used.insert (e.pitch);
    CHECK ((used.count (36) + used.count (38) + used.count (42)) >= 2);
    // Strength 0: groove ignored, same as independent mode.
    Settings ind = s;
    ind.grooveAware = false;
    s.grooveStrength = 0.0f;
    CHECK (similarity (generate (ind, 3), generate (ind, 3, &prof)) == 1.0);

    // A shuffled groove is recognised as ternary.
    Pattern sh;
    sh.length = 4.0;
    for (int i = 0; i < 4; ++i) for (double o : { 0.0, 2.0 / 3.0 }) { Event e; e.start = i + o; e.pitch = 42; sh.add (e); }
    sh.sanitise();
    CHECK (analyseGroove (sh).ternary > 0.4);
}

TEST_CASE ("serialisation: patterns and settings survive JSON exactly")
{
    auto s = kit();
    s.engines = { true, true, true, true, true };
    s.grammars[kBuild] = s.grammars[kDisplacement] = true;
    s.resolution = Resolution::septuplet;
    s.swing = 0.3f;
    const auto p = generate (s, 77);
    const auto back = Pattern::fromJson (p.toJson());
    CHECK (same (back, p));
    const auto s2 = Settings::fromVar (juce::JSON::parse (juce::JSON::toString (s.toVar())));
    CHECK (same (generate (s2, 77), p));
    // Garbage and hostile values are cleaned up.
    const auto junk = Pattern::fromJson (R"({"length": -3, "events": [{"pitch": 900, "velocity": -5, "start": 99, "length": 0}]})");
    CHECK (valid (junk));
    CHECK (Pattern::fromJson ("not json").events.empty());
}

TEST_CASE ("MIDI export: timing, lengths, pitches, velocities and tempo come back from the file")
{
    auto s = kit();
    s.resolution = Resolution::quintuplet;
    s.swing = 0.0f;
    auto p = generate (s, 12);
    for (auto& e : p.events) e.probability = 1.0f;
    auto f = juce::File::createTempFile (".mid");
    REQUIRE (writeMidiFile (f, p, 133.0, 10));
    juce::FileInputStream in (f);
    juce::MidiFile mf;
    REQUIRE (mf.readFrom (in));
    double bpm = 0.0;
    const auto back = patternFromMidiFile (mf, &bpm);
    CHECK (std::abs (bpm - 133.0) < 0.01);
    REQUIRE (back.events.size() == p.events.size());
    for (size_t i = 0; i < p.events.size(); ++i)
    {
        CHECK (std::abs (back.events[i].time() - p.events[i].time()) <= 0.5 / kTicksPerBeat);
        CHECK (back.events[i].pitch == p.events[i].pitch);
        CHECK (back.events[i].velocity == p.events[i].velocity);
    }
    // Channel 10, note-offs paired, no same-pitch overlap.
    juce::MidiMessageSequence seq (*mf.getTrack (0));
    seq.updateMatchedPairs();
    int ons = 0;
    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto& m = seq.getEventPointer (i)->message;
        if (! m.isNoteOn()) continue;
        ++ons;
        CHECK (m.getChannel() == 10);
        CHECK (seq.getEventPointer (i)->noteOffObject != nullptr);
    }
    CHECK (ons == (int) p.events.size());
    f.deleteFile();
}

TEST_CASE ("extreme settings: thousands of random combinations never produce invalid MIDI")
{
    Rng r (2024);
    for (int i = 0; i < 1500; ++i)
    {
        Settings s;
        if (r.chance (0.7)) s.pool = defaultPool();
        s.density = (float) r.uniform();
        s.complexity = (float) r.uniform();
        s.behaviour = (float) r.uniform();
        s.lengthChoice = (LengthChoice) r.below (6);
        s.customLength = r.uniform (0.25, 32.0);
        s.beatsPerBar = r.range (2, 7);
        s.resolution = (Resolution) r.below (8);
        s.customSubdivision = r.range (1, 32);
        s.swing = (float) r.uniform();
        s.humanise = (float) r.uniform();
        s.velocityBase = r.range (1, 127);
        s.velocityVariation = (float) r.uniform();
        s.accent = (float) r.uniform();
        s.ghost = (float) r.uniform();
        s.curve = (VelocityCurve) r.below (6);
        s.pitchLow = r.range (0, 127);
        s.pitchHigh = r.range (0, 127);
        s.motion = (PitchMotion) r.below (7);
        for (auto& e : s.engines) e = r.chance (0.5);
        for (auto& g : s.grammars) g = r.chance (0.4);
        s.euclidSteps = r.chance (0.3) ? r.range (1, 64) : 0;
        s.euclidPulses = r.chance (0.3) ? r.range (1, 64) : 0;
        s.ratchetMax = r.range (2, 13);
        s.polyRatio = r.below ((int) kPolyRatios.size());
        const auto p = generate (s, r.next());
        INFO ("case " << i);
        REQUIRE (valid (p));
        REQUIRE (! p.events.empty());
        MutationSettings m;
        m.preservation = (float) r.uniform();
        REQUIRE (valid (mutate (p, s, m, r.next())));
    }
}

TEST_CASE ("factory presets all generate valid, distinct fills")
{
    std::set<std::string> seen;
    auto base = kit();
    for (const auto& pr : factoryPresets())
    {
        const auto s = applyPreset (pr, base);
        CHECK (s.pool == base.pool);
        const auto p = generate (s, 1234);
        INFO (pr.name);
        CHECK (valid (p));
        seen.insert (p.toJson().toStdString());
    }
    CHECK (seen.size() == factoryPresets().size());
}

// Hidden: print fills as text piano rolls, to judge them by eye (run with "[.show]").
TEST_CASE ("show some fills", "[.show]")
{
    for (const auto& pr : factoryPresets())
    {
        GenerationReport rep;
        const auto s = applyPreset (pr, kit());
        const auto p = generate (s, 99, nullptr, &rep);
        std::cout << "\n== " << pr.name << "  (" << p.events.size() << " notes)\n" << rep.summary() << "\n" << roll (p, (int) (p.length * 16));
    }
}
