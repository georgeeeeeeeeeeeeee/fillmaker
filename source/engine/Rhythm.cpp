#include "engine/Rhythm.h"

#include <algorithm>
#include <cmath>

namespace fm
{
namespace
{
bool onMultiple (double x, double unit) noexcept
{
    const double r = x / unit;
    return std::abs (r - std::round (r)) < 1.0e-6;
}
} // namespace

double metricWeight (double beat, double beatsPerBar) noexcept
{
    const double bar = beatsPerBar > 0.0 ? beatsPerBar : 4.0;
    double pos = std::fmod (beat, bar);
    if (pos < 0.0) pos += bar;
    if (onMultiple (pos, bar)) return 1.0;
    // Half bar in even meters (beat 3 of 4/4).
    if (std::fmod (bar, 2.0) < 1.0e-9 && onMultiple (pos, bar * 0.5)) return 0.85;
    if (onMultiple (pos, 1.0)) return 0.75;
    if (onMultiple (pos, 0.5)) return 0.55;
    if (onMultiple (pos, 0.25)) return 0.38;
    if (onMultiple (pos, 1.0 / 3.0)) return 0.42;   // triplet 8ths
    if (onMultiple (pos, 1.0 / 6.0)) return 0.3;
    if (onMultiple (pos, 0.125)) return 0.25;
    if (onMultiple (pos, 0.2) || onMultiple (pos, 1.0 / 7.0)) return 0.3;   // quintuplets, septuplets
    return 0.18;
}

std::vector<bool> euclid (int pulses, int steps, int rotation)
{
    steps = std::max (1, steps);
    pulses = std::clamp (pulses, 0, steps);
    std::vector<bool> out ((size_t) steps, false);
    if (pulses == 0) return out;
    // Bjorklund's algorithm (as in Toussaint's paper): repeatedly pair the remainder sequences with
    // the leading ones until at most one remainder is left. Gives the canonical forms, starting on a
    // pulse: E(3,8) = x..x..x., E(5,8) = x.xx.xx.
    std::vector<std::vector<bool>> a ((size_t) pulses, std::vector<bool> { true });
    std::vector<std::vector<bool>> b ((size_t) (steps - pulses), std::vector<bool> { false });
    while (b.size() > 1)
    {
        const size_t n = std::min (a.size(), b.size());
        std::vector<std::vector<bool>> na, nb;
        for (size_t i = 0; i < n; ++i) { auto x = a[i]; x.insert (x.end(), b[i].begin(), b[i].end()); na.push_back (x); }
        if (a.size() > n) nb.assign (a.begin() + (long) n, a.end());
        else nb.assign (b.begin() + (long) n, b.end());
        a.swap (na);
        b.swap (nb);
    }
    std::vector<bool> seq;
    for (const auto& x : a) seq.insert (seq.end(), x.begin(), x.end());
    for (const auto& x : b) seq.insert (seq.end(), x.begin(), x.end());
    rotation = ((rotation % steps) + steps) % steps;
    for (int i = 0; i < steps; ++i) out[(size_t) ((i + rotation) % steps)] = seq[(size_t) i];
    return out;
}

std::vector<GridPoint> buildGrid (double start, double length, double stepsPerBeat, double beatsPerBar, double patternLength,
                                  Warp warp, const void* warpCtx)
{
    std::vector<GridPoint> g;
    stepsPerBeat = std::clamp (stepsPerBeat, 0.25, 64.0);
    const double step = 1.0 / stepsPerBeat, end = start + length;
    if (warp == nullptr)
    {
        const int n = std::max (1, (int) std::floor (length / step + 1.0e-6));
        for (int i = 0; i < n; ++i)
        {
            const double t = start + i * step;
            g.push_back ({ t, metricWeight (t, beatsPerBar), i });
        }
        return g;
    }
    // Warped: walk forward, each step's size the base step times the warp at that point. Snapped to a
    // 1/960-beat lattice, the way it will be exported.
    double t = start;
    int i = 0;
    while (t < end - 1.0e-6 && i < 4096)
    {
        const double snapped = std::round (t * 960.0) / 960.0;
        g.push_back ({ snapped, metricWeight (snapped, beatsPerBar), i++ });
        const double m = std::clamp (warp (patternLength > 0.0 ? t / patternLength : 0.0, warpCtx), 0.2, 5.0);
        t += step * m;
    }
    return g;
}
} // namespace fm
