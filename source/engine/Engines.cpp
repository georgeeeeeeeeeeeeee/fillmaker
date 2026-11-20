#include "engine/Engines.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace fm
{
namespace
{
// A fixed pseudo-random value for a position (so "unhinged" weights are irregular but repeatable).
double hash01 (double beat, uint32_t salt) noexcept
{
    auto x = (uint64_t) (int64_t) std::llround (beat * 960.0) * 0x9e3779b97f4a7c15ULL ^ (uint64_t) salt * 0xbf58476d1ce4e5b9ULL;
    x = Rng::mix (x);
    return (double) (x >> 11) * (1.0 / 9007199254740992.0);
}

double warpOf (double t, const void* ctx) { return static_cast<const GrammarPlan*> (ctx)->stepScale (t); }

std::vector<GridPoint> gridFor (const Ctx& c, const Span& s)
{
    return buildGrid (s.start, s.length, s.stepsPerBeat, c.bar, c.L, c.g.warped() ? &warpOf : nullptr, &c.g);
}

Proto make (const Ctx& c, double time, double step, int pitch, Rng& rng, int engine)
{
    Proto p;
    p.time = time;
    p.step = step;
    p.pitch = pitch;
    p.metric = metricWeight (time, c.bar);
    p.length = c.noteLength (step, rng);
    p.offset = c.displacement (time, step, rng);
    p.engine = engine;
    return p;
}
} // namespace

// ---------------------------------------------------------------------------------------------- context
double Ctx::density (double beat) const noexcept
{
    double d = s.density;
    if (groovy())
    {
        // Fills run denser than the groove they answer: aim around 1.6x the groove's hits per step.
        const double perStep = groove->density / std::max (1.0, s.stepsPerBeat());
        d += (std::clamp (perStep * 1.6, 0.08, 0.95) - d) * 0.5 * s.grooveStrength;
    }
    return std::clamp (d * g.density (t (beat)) * densityScale, 0.0, 1.0);
}

double Ctx::weight (double beat, double metric) const noexcept
{
    // Conventional: strong beats strong. Syncopation hands the weight to the off-beats; behaviour adds
    // a fixed irregularity of its own (the same positions each time, for a given seed's spans).
    double w = metric;
    w += ((1.0 - metric) * 0.9 + 0.1 - w) * syncopation * 0.7;
    w += (hash01 (beat, 77) - w) * s.behaviour * 0.5;
    if (groovy())
    {
        // Related to the groove: lean on its strong positions, and on the spaces it leaves.
        const double target = 0.6 * groove->onsetAt (beat) + 0.4 * groove->emptiness (beat);
        w += (target - w) * 0.55 * s.grooveStrength;
    }
    return std::clamp (w, 0.02, 1.0);
}

double Ctx::displacement (double beat, double step, Rng& rng) const noexcept
{
    const double a = g.displacement (t (beat));
    if (a <= 0.0 || ! rng.chance (a)) return 0.0;
    // Mild: a whole step early or late (anticipation, drag). Wild: odd fractions of a step.
    if (! rng.chance (s.behaviour * 0.8))
        return (rng.chance (0.6) ? -1.0 : 1.0) * step * (rng.chance (0.7) ? 1.0 : 0.5);
    static const double fr[] = { 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.2, 0.4, 1.0 / 7.0, 1.5 };
    return (rng.chance (0.5) ? -1.0 : 1.0) * step * fr[rng.below (7)] * (0.6 + a);
}

double Ctx::noteLength (double step, Rng& rng) const noexcept
{
    double len = step * (0.55 + 0.2 * rng.uniform());
    if (rng.chance (s.behaviour * 0.35)) len = step * rng.uniform (0.15, 2.5);   // stabs and smears
    return std::max (1.0 / 64.0, len);
}

// ---------------------------------------------------------------------------------------------- probability
void probabilityEngine (const Ctx& c, Span& span, Rng& rng, std::vector<Proto>& out)
{
    const auto grid = gridFor (c, span);
    if (grid.empty()) return;
    double meanW = 0.0;
    for (const auto& p : grid) meanW += c.weight (p.time, p.metric);
    meanW = std::max (0.05, meanW / (double) grid.size());
    const double b = c.s.behaviour, cx = c.s.complexity, frag = c.g.fragmentation();
    const double cluster = 1.0 + 1.4 * b * (0.4 + cx);       // unhinged: hits beget hits
    const int fragPeriod = 2 + rng.below (4);                   // Fragmentation: short cells with holes
    bool prev = false;
    int misses = 0, fired = 0;
    for (size_t i = 0; i < grid.size(); ++i)
    {
        const auto& gp = grid[i];
        const double step = i + 1 < grid.size() ? grid[i + 1].time - gp.time : 1.0 / span.stepsPerBeat;
        if (c.gap (gp.time)) { prev = false; continue; }
        const double d = c.density (gp.time);
        double prob = d * c.weight (gp.time, gp.metric) / meanW;
        if (prev) prob *= d > 0.7 ? 1.0 + 0.4 * (1.0 - b) : cluster * (0.75 + 0.25 * d);   // rolls at high density
        else prob *= 1.0 + misses * 0.07 * (1.0 - b);            // conventional fills don't leave long holes
        if (frag > 0.0 && (gp.index / fragPeriod) % 2 == 1) prob *= 1.0 - 0.85 * frag;
        if (c.g.inBurst (c.t (gp.time))) prob = std::max (prob, 0.8);
        prob = std::clamp (prob, 0.0, 0.97);
        if (rng.chance (prob))
        {
            auto p = make (c, gp.time, step, c.pitch.next (rng, gp.metric), rng, kProbability);
            p.accent = gp.metric >= 0.75 && rng.chance (0.5 * (1.0 - b));
            out.push_back (p);
            // A grace note (flam or drag) before it, at higher complexity.
            if (rng.chance (0.12 * cx * (0.5 + d)))
            {
                const int graces = rng.chance (0.6) ? 1 : 2;
                for (int k = graces; k >= 1; --k)
                {
                    auto gr = make (c, gp.time - step * 0.28 * k, step * 0.25, p.pitch, rng, kProbability);
                    gr.offset = 0.0;
                    gr.level = 0.45;
                    gr.ghostable = false;
                    out.push_back (gr);
                }
            }
            prev = true;
            misses = 0;
            ++fired;
        }
        else
        {
            prev = false;
            ++misses;
        }
    }
    span.note = "prob " + juce::String (fired) + (c.g.warped() ? " warped" : "");
}

// ---------------------------------------------------------------------------------------------- euclidean
void euclideanEngine (const Ctx& c, Span& span, Rng& rng, std::vector<Proto>& out)
{
    const double cx = c.s.complexity, b = c.s.behaviour;
    const int gridSteps = std::max (1, (int) std::lround (span.length * span.stepsPerBeat));
    int n = c.s.euclidSteps > 0 ? c.s.euclidSteps : gridSteps;
    // Complex: a step count that disagrees with the grid (7 or 11 steps across a beat-grid span).
    if (c.s.euclidSteps == 0 && rng.chance (0.5 * cx + 0.3 * b))
    {
        static const int odd[] = { 5, 7, 9, 11, 13 };
        n = std::max (3, odd[rng.below (5)] * std::max (1, (int) std::lround (span.length / 2.0)));
    }
    n = std::clamp (n, 1, 64);
    const double mid = span.start + span.length * 0.5;
    int k = c.s.euclidPulses > 0 ? std::min (c.s.euclidPulses, n) : (int) std::lround (n * std::clamp (c.density (mid) * 1.1, 0.08, 0.95));
    k = std::clamp (k, 1, n);
    const int rot = c.s.euclidRotation > 0 || c.s.euclidSteps > 0 ? c.s.euclidRotation : (rng.chance (0.35 + 0.5 * b) ? rng.below (n) : 0);
    const auto pat = euclid (k, n, rot);
    const double step = span.length / n;
    const int p1 = c.pitch.pickWeighted (rng);
    int placed = 0;
    for (int i = 0; i < n; ++i)
    {
        if (! pat[(size_t) i]) continue;
        const double tm = span.start + i * step;
        if (c.gap (tm)) continue;
        if (! rng.chance (std::clamp (0.55 + 0.45 * c.g.density (c.t (tm)), 0.0, 1.0))) continue;
        auto p = make (c, tm, step, rng.chance (0.3 + 0.5 * b) ? c.pitch.next (rng, metricWeight (tm, c.bar)) : p1, rng, kEuclidean);
        p.accent = placed == 0;
        out.push_back (p);
        ++placed;
    }
    juce::String note = "E(" + juce::String (k) + "," + juce::String (n) + (rot ? "," + juce::String (rot) : juce::String()) + ")";
    // Two cycles against each other: a second, different Euclid on another pitch.
    if (rng.chance (0.6 * cx) && c.pitch.size() > 1)
    {
        const int n2 = std::clamp (n + (rng.chance (0.5) ? 1 : -1) * rng.range (1, 3), 2, 64);
        const int k2 = std::clamp ((int) std::lround (n2 * c.density (mid) * 0.6), 1, n2);
        const auto pat2 = euclid (k2, n2, rng.below (n2));
        const int p2 = c.pitch.pickOther (rng, p1);
        const double step2 = span.length / n2;
        for (int i = 0; i < n2; ++i)
        {
            if (! pat2[(size_t) i]) continue;
            const double tm = span.start + i * step2;
            if (c.gap (tm)) continue;
            auto p = make (c, tm, step2, p2, rng, kEuclidean);
            p.level = 0.8;
            out.push_back (p);
        }
        note << " x E(" << k2 << "," << n2 << ")";
    }
    span.note = note;
}

// ---------------------------------------------------------------------------------------------- ratchet
int pickRatchetCount (Rng& rng, double cx, double b, int maxCount)
{
    // Centred on 2-4 when conventional, 5-7 when complex, 9-13 when unhinged.
    const double target = 2.5 + 4.0 * cx + 7.0 * b;
    std::vector<double> w;
    for (int n : kRatchetCounts)
        w.push_back (n <= maxCount ? std::exp (-(n - target) * (n - target) / (2.0 * (1.5 + 3.0 * b) * (1.5 + 3.0 * b))) : 0.0);
    const int i = rng.weighted (w);
    return i >= 0 ? kRatchetCounts[(size_t) i] : 2;
}

void addRatchet (std::vector<Proto>& out, double start, double dur, int count, int pitch, const Ctx& c, Rng& rng, bool movePitch)
{
    const double b = c.s.behaviour, cx = c.s.complexity;
    count = std::clamp (count, 1, 32);
    // Irregular grouping: 7 as 3+2+2, 9 as 4+5 ... each group opens with an accent and a hair of space.
    std::vector<int> groups { count };
    if (count >= 5 && rng.chance (0.3 + 0.5 * cx))
    {
        groups.clear();
        int left = count;
        while (left > 0)
        {
            const int g = std::min (left, rng.chance (0.5) ? 2 : 3 + (rng.chance (b) ? 1 : 0));
            groups.push_back (g);
            left -= g;
        }
    }
    // Spacing: even, or accelerating / decelerating (u^gamma).
    const double gamma = rng.chance (0.25 + 0.5 * b) ? rng.uniform (0.6, 1.7) : 1.0;
    // Dynamics: flat, rising (into the next beat), falling, or up and down.
    const int shape = rng.weighted ({ 0.3, 0.35 + 0.1 * (1.0 - b), 0.2, 0.15 + 0.2 * b });
    const double gate = rng.uniform (0.45, 0.9);
    int i = 0;
    for (size_t g = 0; g < groups.size(); ++g)
        for (int k = 0; k < groups[g]; ++k, ++i)
        {
            const double u = std::pow ((double) i / count, gamma);
            const double next = std::pow ((double) (i + 1) / count, gamma);
            Proto p;
            p.time = start + dur * u;
            p.step = std::max (1.0 / 256.0, dur * (next - u));
            p.length = std::max (1.0 / 128.0, p.step * gate * (1.0 + (rng.uniform() - 0.5) * 0.4 * b));
            p.pitch = pitch;
            p.metric = metricWeight (p.time, c.bar);
            p.engine = kRatchet;
            const double x = count > 1 ? (double) i / (count - 1) : 1.0;
            p.level = shape == 1 ? 0.55 + 0.5 * x : shape == 2 ? 1.05 - 0.5 * x : shape == 3 ? 0.6 + 0.45 * std::sin (x * 3.14159) : 0.9;
            p.accent = k == 0 && groups.size() > 1;
            p.ghostable = false;
            out.push_back (p);
            if (movePitch && c.pitch.size() > 1 && rng.chance (0.5)) pitch = c.pitch.next (rng, p.metric);
        }
}

void ratchetEngine (const Ctx& c, Span& span, Rng& rng, std::vector<Proto>& out)
{
    const double b = c.s.behaviour, cx = c.s.complexity;
    // Cells: a beat (or half a beat in short fills); complex fills use odd cell lengths.
    double cell = span.length <= 1.0 ? std::max (0.25, span.length * 0.5) : 1.0;
    if (rng.chance (0.4 * cx)) cell = rng.chance (0.5) ? 0.75 : 1.5;
    cell = std::min (cell, span.length);
    juce::StringArray made;
    for (double at = span.start; at < span.start + span.length - 1.0e-6; at += cell)
    {
        const double len = std::min (cell, span.start + span.length - at);
        if (c.gap (at)) continue;
        const bool burst = c.g.inBurst (c.t (at));
        if (! burst && ! rng.chance (std::clamp (c.density (at) * 1.25, 0.15, 1.0)))
        {
            // A single hit holds the place.
            if (rng.chance (0.5 * (1.0 - b))) out.push_back (make (c, at, len, c.pitch.next (rng, metricWeight (at, c.bar)), rng, kRatchet));
            continue;
        }
        int n = pickRatchetCount (rng, cx, b, c.s.ratchetMax);
        // Acceleration and Deceleration change how many fit in a cell.
        const double scale = c.g.stepScale (c.t (at));
        if (std::abs (scale - 1.0) > 0.05)
        {
            const double want = n / scale;
            int best = n;
            for (int k : kRatchetCounts) if (k <= c.s.ratchetMax && std::abs (k - want) < std::abs (best - want)) best = k;
            n = best;
        }
        // How much of the cell it fills: all of it, the second half (leading into the beat), or a
        // wild fraction somewhere inside.
        double dur = len, off = 0.0;
        const int place = rng.weighted ({ 0.5 - 0.3 * b, 0.35, 0.15 + 0.4 * b });
        if (place == 1) { dur = len * 0.5; off = len * 0.5; }
        else if (place == 2) { dur = len * rng.uniform (0.35, 0.9); off = (len - dur) * rng.uniform(); }
        const int pitch = c.pitch.next (rng, metricWeight (at, c.bar));
        addRatchet (out, at + off, dur, n, pitch, c, rng, rng.chance (0.25 + 0.45 * b));
        made.add (juce::String (n));
    }
    span.note = "ratchet " + (made.isEmpty() ? juce::String ("-") : made.joinIntoString (","));
}

// ---------------------------------------------------------------------------------------------- permutation
const char* nameOf (PermOp op)
{
    static const char* const n[] = { "as is", "rotated", "reversed", "halves swapped", "intervals shuffled", "pitches permuted", "fragmented" };
    return n[(int) op];
}

PermOp pickPermutation (Rng& rng, double b, double cx, bool allowIdentity)
{
    const std::vector<double> w { allowIdentity ? 0.45 * (1.0 - b) : 0.0,
                                  0.2 + 0.1 * cx, 0.08 + 0.2 * b, 0.1 + 0.1 * cx, 0.05 + 0.35 * b * (0.5 + cx),
                                  0.18, 0.04 + 0.3 * b };
    return (PermOp) std::max (0, rng.weighted (w));
}

std::vector<Proto> permute (std::vector<Proto> m, double len, PermOp op, Rng& rng, double step)
{
    if (m.empty() || len <= 0.0) return m;
    std::sort (m.begin(), m.end(), [] (const Proto& a, const Proto& b) { return a.time < b.time; });
    const auto wrap = [len] (double t) { t = std::fmod (t, len); return t < 0.0 ? t + len : t; };
    switch (op)
    {
        case PermOp::identity: break;
        case PermOp::rotate:
        {
            const int steps = std::max (1, (int) std::lround (len / std::max (step, 1.0e-3)));
            const double by = step * (1 + rng.below (std::max (1, steps - 1)));
            for (auto& p : m) p.time = wrap (p.time + by);
            break;
        }
        case PermOp::reverse:
            for (auto& p : m) p.time = wrap (len - p.time - std::min (p.step, len * 0.5)) ;
            break;
        case PermOp::swapHalves:
            for (auto& p : m) p.time = wrap (p.time + len * 0.5);
            break;
        case PermOp::intervalShuffle:
        {
            // Onsets as groups; the gaps between them (with the wrap back to the start) shuffled.
            std::vector<double> onsets;
            for (const auto& p : m) if (onsets.empty() || p.time - onsets.back() > 1.0e-6) onsets.push_back (p.time);
            if (onsets.size() < 2) break;
            std::vector<double> gaps;
            for (size_t i = 0; i < onsets.size(); ++i)
                gaps.push_back ((i + 1 < onsets.size() ? onsets[i + 1] : onsets[0] + len) - onsets[i]);
            rng.shuffle (gaps);
            std::vector<double> moved (onsets.size());
            double t = onsets[0];
            for (size_t i = 0; i < onsets.size(); ++i) { moved[i] = wrap (t); t += gaps[i]; }
            for (auto& p : m)
            {
                const auto it = std::lower_bound (onsets.begin(), onsets.end(), p.time - 1.0e-6);
                const size_t gi = (size_t) std::min<long> ((long) (it - onsets.begin()), (long) onsets.size() - 1);
                p.time = moved[gi];
            }
            break;
        }
        case PermOp::pitchShuffle:
        {
            std::vector<int> pitches;
            for (const auto& p : m) pitches.push_back (p.pitch);
            rng.shuffle (pitches);
            for (size_t i = 0; i < m.size(); ++i) m[i].pitch = pitches[i];
            break;
        }
        case PermOp::fragment:
        {
            // A slice of the motif, repeated (a stutter of itself).
            const double a = std::floor (rng.uniform (0.0, 0.6) * len / step) * step;
            const double w = std::max (step, std::floor (rng.uniform (0.25, 0.5) * len / step) * step);
            std::vector<Proto> slice;
            for (const auto& p : m) if (p.time >= a - 1.0e-6 && p.time < a + w - 1.0e-6) slice.push_back (p);
            if (slice.empty()) break;
            std::vector<Proto> rep;
            for (double at = 0.0; at < len - 1.0e-6; at += w)
                for (auto p : slice)
                {
                    p.time = at + (p.time - a);
                    if (p.time < len - 1.0e-6) rep.push_back (p);
                }
            m.swap (rep);
            break;
        }
    }
    std::sort (m.begin(), m.end(), [] (const Proto& a, const Proto& b) { return a.time < b.time; });
    return m;
}

void permutationEngine (const Ctx& c, Span& span, Rng& rng, std::vector<Proto>& out)
{
    const double b = c.s.behaviour, cx = c.s.complexity;
    // The motif: a beat (conventional), or half / one and a half / two beats as complexity rises.
    double len = 1.0;
    if (rng.chance (0.5 * cx + 0.2 * b)) len = std::array<double, 4> { 0.5, 0.75, 1.5, 2.0 }[(size_t) rng.below (4)];
    len = std::min (len, span.length);
    const double step = 1.0 / span.stepsPerBeat;
    std::vector<Proto> motif;
    bool borrowed = false;
    if (c.groovy() && rng.chance (0.35 + 0.6 * c.s.grooveStrength))
    {
        // From the groove itself: its last stretch of this length, so the fill is recognisably its own.
        const auto& src = c.groove->source;
        const double from = std::max (0.0, src.length - len);
        for (const auto& e : src.events)
            if (e.time() >= from - 1.0e-6 && e.time() < from + len - 1.0e-6)
            {
                Proto p;
                p.time = e.time() - from;
                p.length = std::min (e.length, len);
                p.pitch = e.pitch;
                p.level = std::clamp (e.velocity / 100.0, 0.4, 1.2);
                p.step = step;
                p.engine = kPermutation;
                motif.push_back (p);
            }
        borrowed = motif.size() >= 2;
        if (! borrowed) motif.clear();
    }
    if (motif.empty())
    {
        Span m { span.start, len, span.stepsPerBeat, kProbability };
        Ctx denser = c;
        denser.densityScale = c.densityScale * 1.25;
        probabilityEngine (denser, m, rng, motif);
        for (auto& p : motif) p.time -= span.start;
        if (motif.empty())
        {
            auto p = make (c, 0.0, step, c.pitch.pickWeighted (rng), rng, kPermutation);
            p.offset = 0.0;
            motif.push_back (p);
        }
    }
    juce::StringArray ops;
    int rep = 0;
    for (double at = span.start; at < span.start + span.length - 1.0e-6; at += len, ++rep)
    {
        const PermOp op = rep == 0 && ! span.response ? PermOp::identity : pickPermutation (rng, b, cx, rep > 0);
        auto m = permute (motif, len, op, rng, step);
        if (op != PermOp::identity) ops.add (nameOf (op));
        for (auto p : m)
        {
            p.time += at;
            p.engine = kPermutation;
            if (p.time >= span.start + span.length - 1.0e-6 || c.gap (p.time)) continue;
            p.metric = metricWeight (p.time, c.bar);
            p.offset = c.displacement (p.time, step, rng);
            out.push_back (p);
        }
    }
    span.note = juce::String (borrowed ? "groove motif " : "motif ") + juce::String (len, 2) + (ops.isEmpty() ? juce::String() : ": " + ops.joinIntoString (", "));
}

// ---------------------------------------------------------------------------------------------- polyrhythm
void polyrhythmEngine (const Ctx& c, Span& span, Rng& rng, std::vector<Proto>& out)
{
    const double b = c.s.behaviour, cx = c.s.complexity;
    std::pair<int, int> r = kPolyRatios[(size_t) std::clamp (c.s.polyRatio, 0, (int) kPolyRatios.size() - 1)];
    if (r.first == 0)
    {
        // Auto: plain cross-rhythms when conventional, odd ones as complexity and behaviour rise.
        const std::vector<double> w { 0.6 - 0.4 * b, 0.25 + 0.3 * cx, 0.1 + 0.4 * cx * b, 0.02 + 0.5 * b, 0.3 * (1.0 - b), 0.15 + 0.2 * cx, 0.1 + 0.3 * b, 0.05 + 0.3 * cx };
        r = kPolyRatios[(size_t) (1 + std::max (0, rng.weighted (w)))];
    }
    const int pa = c.pitch.pickWeighted (rng), pb = c.pitch.pickOther (rng, pa);
    juce::String note;
    if (rng.chance (0.35 + 0.3 * cx))
    {
        // A repeating group of `a` steps against the meter (3 16ths over 4/4 crosses the bar lines):
        // its first step accented, the rest filled by density. Anchored at the pattern start.
        const int group = std::clamp (r.first, 3, 11);
        const double step = 1.0 / span.stepsPerBeat;
        const int first = (int) std::ceil (span.start / step - 1.0e-6), last = (int) std::floor ((span.start + span.length) / step - 1.0e-6);
        for (int i = first; i <= last; ++i)
        {
            const double tm = i * step;
            if (c.gap (tm) || tm >= span.start + span.length - 1.0e-6) continue;
            const bool head = i % group == 0;
            if (head || rng.chance (c.density (tm) * 0.55))
            {
                auto p = make (c, tm, step, head ? pa : (rng.chance (0.6) ? pb : c.pitch.next (rng, 0.3)), rng, kPolyrhythm);
                p.accent = head;
                p.level = head ? 1.0 : 0.7;
                out.push_back (p);
            }
        }
        note = "groups of " + juce::String (group);
    }
    else
    {
        // a against b over a cycle: a bar when conventional; longer (1.5 or 2 bars) when not, so the
        // cycles run across bar lines and only meet again later.
        double cycle = c.bar * (rng.chance (0.3 + 0.4 * b) ? (rng.chance (0.5) ? 1.5 : 2.0) : 1.0);
        if (span.length < c.bar * 0.75) cycle = std::max (0.5, span.length);
        const auto layer = [&] (int count, int pitch, double level, bool accentFirst) {
            const double step = cycle / count;
            const int first = (int) std::ceil (span.start / step - 1.0e-6);
            for (int i = first;; ++i)
            {
                const double tm = i * step;
                if (tm >= span.start + span.length - 1.0e-6) break;
                if (c.gap (tm)) continue;
                if (! rng.chance (std::clamp (0.5 + 0.5 * c.g.density (c.t (tm)), 0.0, 1.0))) continue;
                auto p = make (c, tm, step, pitch, rng, kPolyrhythm);
                p.level = level;
                p.accent = accentFirst && i % count == 0;
                out.push_back (p);
            }
        };
        layer (r.first, pa, 1.0, true);
        if (c.density (span.start) > 0.25 || cx > 0.4) layer (r.second, pb, 0.75, false);
        note = juce::String (r.first) + ":" + juce::String (r.second) + " over " + juce::String (cycle, 1) + " beats";
    }
    span.note = "poly " + note;
}
} // namespace fm
