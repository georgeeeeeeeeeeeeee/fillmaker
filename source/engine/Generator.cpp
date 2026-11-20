#include "engine/Generator.h"

#include <algorithm>
#include <cmath>

namespace fm
{
namespace
{
using EngineFn = void (*) (const Ctx&, Span&, Rng&, std::vector<Proto>&);
constexpr EngineFn kEngines[kNumEngines] = { probabilityEngine, euclideanEngine, ratchetEngine, permutationEngine, polyrhythmEngine };

int pickEngine (const Settings& s, Rng& rng, bool burst, int avoid = -1)
{
    const double b = s.behaviour, cx = s.complexity;
    std::array<double, kNumEngines> w { 1.0 - 0.3 * b,               // probability
                                        0.55 + 0.25 * cx,             // euclidean
                                        0.5 + 0.4 * cx + 0.4 * b,     // ratchet
                                        0.35 + 0.5 * cx,              // permutation
                                        0.15 + 0.6 * cx + 0.5 * b };  // polyrhythm
    if (burst) { w[kRatchet] *= 4.0; w[kProbability] *= 2.0; }
    std::vector<double> v;
    for (int i = 0; i < kNumEngines; ++i) v.push_back (s.engines[(size_t) i] && i != avoid ? w[(size_t) i] : 0.0);
    const int i = rng.weighted (v);
    if (i >= 0) return i;
    for (int k = 0; k < kNumEngines; ++k) if (s.engines[(size_t) k]) return k;
    return kProbability;   // nothing ticked: Probability
}

// The subdivision a span uses: the chosen resolution, or (as complexity and behaviour rise) a tuplet
// or a doubled rate for a span. Groove-aware with a shuffled source: triplets.
double spanSubdivision (const Settings& s, Rng& rng, const GrooveProfile* groove)
{
    const double base = s.stepsPerBeat();
    if (groove != nullptr && s.grooveAware && groove->valid() && groove->ternary > 0.45 && rng.chance (s.grooveStrength))
        return base >= 5.0 ? 6.0 : 3.0;
    if (! rng.chance (0.15 * s.complexity + 0.35 * s.complexity * s.complexity + 0.25 * s.behaviour)) return base;
    const double b = s.behaviour;
    static const double alt[] = { 3.0, 6.0, 5.0, 7.0, 8.0, 9.0, 10.0, 11.0 };
    const std::vector<double> w { 0.3, 0.35 - 0.1 * b, 0.15 + 0.2 * s.complexity, 0.1 + 0.25 * b, 0.25 * (1.0 - b), 0.05 + 0.2 * b, 0.03 + 0.1 * b, 0.02 + 0.15 * b };
    const int i = rng.weighted (w);
    const double d = alt[std::max (0, i)];
    return std::abs (d - base) < 1.0e-9 ? base * 2.0 : d;
}

std::vector<Span> planSpans (const Settings& s, double L, Rng& rng, const GrammarPlan& g, const GrooveProfile* groove)
{
    std::vector<Span> spans;
    const double b = s.behaviour, cx = s.complexity;
    const double baseStep = 1.0 / s.stepsPerBeat();
    const bool irregular = rng.chance (std::clamp (0.05 + 0.55 * b + 0.3 * cx - 0.1, 0.0, 0.9));
    if (irregular)
    {
        // Groupings that don't line up with the beat: 3+3+2, 5+4, 7 ... in grid steps.
        static const int sizes[] = { 2, 3, 4, 5, 6, 7 };
        const std::vector<double> w { 0.15, 0.3, 0.15, 0.2 + 0.1 * b, 0.1, 0.1 + 0.2 * b };
        const double unitSteps = std::max (1.0, s.stepsPerBeat() / 4.0 * 2.0);   // a 'step' here is ~an 8th
        double at = 0.0;
        while (at < L - 1.0e-6)
        {
            const double len = std::min (L - at, sizes[std::max (0, rng.weighted (w))] * baseStep * unitSteps);
            spans.push_back ({ at, len });
            at += len;
        }
    }
    else
    {
        const double unit = L <= 1.0 ? 0.5 : (L >= 16.0 ? 2.0 : 1.0);
        for (double at = 0.0; at < L - 1.0e-6; at += unit) spans.push_back ({ at, std::min (unit, L - at) });
    }
    // Engines and subdivisions per span; then neighbours with the same engine merge, so a motif or a
    // polyrhythm can run on (a cycle across bar lines).
    for (size_t i = 0; i < spans.size(); ++i)
    {
        auto& sp = spans[i];
        const bool burst = g.inBurst ((sp.start + sp.length * 0.5) / L);
        sp.engine = pickEngine (s, rng, burst);
        sp.stepsPerBeat = spanSubdivision (s, rng, groove);
        // Continuity: conventional fills keep an engine going for a while.
        if (i > 0 && rng.chance (0.55 * (1.0 - b) + 0.15)) { sp.engine = spans[i - 1].engine; sp.stepsPerBeat = spans[i - 1].stepsPerBeat; }
    }
    std::vector<Span> merged;
    for (const auto& sp : spans)
    {
        if (! merged.empty() && merged.back().engine == sp.engine && std::abs (merged.back().stepsPerBeat - sp.stepsPerBeat) < 1.0e-9
            && (sp.engine == kPolyrhythm || sp.engine == kPermutation || sp.engine == kEuclidean)
            && merged.back().length + sp.length <= std::max (4.0, s.beatsPerBar) + 1.0e-6)
            merged.back().length += sp.length;
        else
            merged.push_back (sp);
    }
    // Layering: a second engine over some spans (combinations of engines in one fill).
    int enabled = 0;
    for (bool e : s.engines) enabled += e ? 1 : 0;
    for (size_t i = 0; i < merged.size(); ++i)
    {
        merged[i].index = (int) i;
        if (enabled > 1 && rng.chance (0.1 + 0.45 * b * (0.4 + cx)))
            merged[i].secondary = pickEngine (s, rng, false, merged[i].engine);
        merged[i].response = g.callAndResponse() && i % 2 == 1;
    }
    return merged;
}

double curveFactor (const Settings& s, double t, uint32_t seed) noexcept
{
    const double d = s.curveDepth;
    switch (s.curve)
    {
        case VelocityCurve::crescendo: return 1.0 - 0.6 * d + 0.85 * d * t;
        case VelocityCurve::decrescendo: return 1.0 + 0.25 * d - 0.85 * d * t;
        case VelocityCurve::swell: return 1.0 - 0.55 * d * (1.0 - std::sin (t * 3.14159265));
        case VelocityCurve::dip: return 1.0 - 0.55 * d * std::sin (t * 3.14159265);
        case VelocityCurve::random:
        {
            // A smooth random contour: a few control points, eased between.
            const double x = t * 4.0;
            const int i = (int) std::floor (x);
            const auto at = [seed] (int k) { return (double) (Rng::mix ((uint64_t) seed * 131 + (uint64_t) k) >> 11) * (1.0 / 9007199254740992.0); };
            const double u = x - i, e = 0.5 - 0.5 * std::cos (u * 3.14159265);
            return 1.0 - 0.6 * d + 0.8 * d * (at (i) + (at (i + 1) - at (i)) * e);
        }
        case VelocityCurve::flat: default: return 1.0;
    }
}
} // namespace

double swingOffset (double time, const Settings& s) noexcept
{
    if (s.swing <= 0.0f) return 0.0;
    const double unit = s.stepsPerBeat() >= 4.0 && s.resolution != Resolution::triplet ? 0.25 : 0.5;
    const double k = time / unit;
    if (std::abs (k - std::round (k)) > 1.0e-6) return 0.0;      // only on-grid notes swing
    if (((long long) std::llround (k)) % 2 == 0) return 0.0;      // the on-beats stay put
    return s.swing * unit * 0.5;                                   // up to a dotted feel
}

Pattern generate (const Settings& sIn, uint32_t seed, const GrooveProfile* groove, GenerationReport* report)
{
    Settings s = sIn;
    const double L = s.lengthBeats();
    Rng root (seed, 0x5eed);
    Rng planRng = root.fork (1), pitchRng = root.fork (2), shapeRng = root.fork (3);
    const auto plan = GrammarPlan::make (s, L, planRng);
    PitchPicker picker (s, groove);
    Ctx ctx { s, plan, picker, groove, L, s.beatsPerBar };
    ctx.syncopation = std::clamp (0.6 * s.complexity + 0.4 * s.behaviour, 0.0, 1.0) * (0.5 + 0.5 * s.complexity);

    auto spans = planSpans (s, L, planRng, plan, groove);

    std::vector<Proto> all;
    std::vector<Proto> lastCall;
    double lastCallStart = 0.0, lastCallLen = 0.0;
    for (auto& sp : spans)
    {
        Rng er = root.fork (100 + (uint64_t) sp.index * 31);
        picker.beginPhrase (s.motion, pitchRng);
        std::vector<Proto> got;
        if (sp.response && ! lastCall.empty())
        {
            // Call + Response: the answer is the call, transformed and moved to another part of the
            // pitch pool (or another pitch, in a small pool).
            std::vector<Proto> call;
            for (auto p : lastCall) { p.time -= lastCallStart; call.push_back (p); }
            const auto op = pickPermutation (er, s.behaviour, s.complexity, false);
            auto ans = permute (call, std::max (lastCallLen, 1.0e-3), op, er, 1.0 / sp.stepsPerBeat);
            const int shift = picker.size() > 2 ? std::max (1, picker.size() / 3) : 0;
            for (auto p : ans)
            {
                p.time = sp.start + p.time * (sp.length / std::max (lastCallLen, 1.0e-3));
                if (p.time >= sp.start + sp.length - 1.0e-6 || ctx.gap (p.time)) continue;
                if (shift > 0)
                {
                    const auto& pool = picker.pool();
                    auto it = std::find_if (pool.begin(), pool.end(), [&p] (const PitchEntry& e) { return e.pitch == p.pitch; });
                    const int idx = it == pool.end() ? 0 : (int) (it - pool.begin());
                    p.pitch = pool[(size_t) ((idx + shift) % picker.size())].pitch;
                }
                else if (er.chance (0.5)) p.pitch = picker.pickOther (er, p.pitch);
                p.metric = metricWeight (p.time, ctx.bar);
                got.push_back (p);
            }
            sp.note = juce::String ("response: ") + nameOf (op);
        }
        else
            kEngines[sp.engine] (ctx, sp, er, got);
        if (sp.secondary >= 0)
        {
            Ctx layer = ctx;
            layer.densityScale = 0.5;
            Span s2 = sp;
            std::vector<Proto> extra;
            kEngines[sp.secondary] (layer, s2, er, extra);
            for (auto& p : extra) p.level *= 0.8;
            got.insert (got.end(), extra.begin(), extra.end());
            sp.note << " + " << s2.note;
        }
        if (! sp.response) { lastCall = got; lastCallStart = sp.start; lastCallLen = sp.length; }
        all.insert (all.end(), got.begin(), got.end());
    }

    // Shape every note the same way, whichever engine made it.
    Pattern pat;
    pat.length = L;
    pat.beatsPerBar = s.beatsPerBar;
    pat.seed = seed;
    std::sort (all.begin(), all.end(), [] (const Proto& a, const Proto& b) { return a.time != b.time ? a.time < b.time : a.pitch < b.pitch; });
    const double b = s.behaviour;
    for (auto& p : all)
    {
        // Interruption's gaps hold for every note (a ratchet or a grace note can reach into one).
        if (p.time < 0.0 || p.time >= L - 1.0e-9 || plan.inGap (p.time)) continue;
        const double t = p.time / L;
        double v = s.velocityBase * curveFactor (s, t, seed) * plan.intensity (t) * p.level;
        bool accent = p.accent;
        // Conventional accents sit on strong beats; unhinged ones land anywhere, and some notes
        // collapse to almost nothing.
        if (! accent && p.metric >= 0.75 && shapeRng.chance (s.accent * (1.0 - b) * 0.8)) accent = true;
        if (! accent && shapeRng.chance (s.accent * b * 0.3)) accent = true;
        if (accent) v += s.accent * (28.0 + 26.0 * b * shapeRng.uniform());
        else if (p.ghostable && p.metric < 0.45 && shapeRng.chance (s.ghost * 0.65)) v = 16.0 + 30.0 * shapeRng.uniform();
        if (shapeRng.chance (0.12 * b)) v *= shapeRng.uniform (0.2, 0.45);
        v += shapeRng.gauss() * s.velocityVariation * (10.0 + 22.0 * b);
        Event e;
        e.start = p.time;
        e.offset = p.offset + swingOffset (p.time, s) + shapeRng.gauss() * s.humanise * (0.010 + 0.02 * b);
        e.length = p.length;
        e.pitch = std::clamp (p.pitch, 0, 127);
        e.velocity = std::clamp ((int) std::lround (v), 1, 127);
        e.accent = accent;
        e.probability = picker.probabilityOf (e.pitch);
        pat.add (e);
    }
    pat.sanitise();
    if (pat.events.empty())
    {
        // Density 0 (or every hit gapped out): a single downbeat, so there is always something.
        Event e;
        e.pitch = picker.pickWeighted (pitchRng);
        e.velocity = std::clamp (s.velocityBase, 1, 127);
        e.length = 0.25;
        pat.add (e);
    }
    if (report != nullptr)
    {
        report->spans = spans;
        report->grammars = plan.describe();
    }
    return pat;
}

juce::String GenerationReport::summary() const
{
    juce::StringArray a;
    for (const auto& s : spans)
    {
        const double spb = s.stepsPerBeat;
        juce::String sub = std::abs (spb - 4.0) < 1e-9 ? "1/16" : std::abs (spb - 2.0) < 1e-9 ? "1/8" : std::abs (spb - 8.0) < 1e-9 ? "1/32"
                         : std::abs (spb - 16.0) < 1e-9 ? "1/64" : std::abs (spb - 3.0) < 1e-9 ? "1/8T" : std::abs (spb - 6.0) < 1e-9 ? "1/16T"
                         : juce::String ((int) std::lround (spb)) + "/beat";
        a.add (sub + " " + s.note);
    }
    return a.joinIntoString ("  |  ") + (grammars.isNotEmpty() ? "  [" + grammars + "]" : juce::String());
}
} // namespace fm
