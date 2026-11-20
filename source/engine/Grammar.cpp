#include "engine/Grammar.h"

#include <algorithm>
#include <cmath>

namespace fm
{
GrammarPlan GrammarPlan::make (const Settings& s, double length, Rng& rng)
{
    GrammarPlan g;
    g.on = s.grammars;
    g.length = length;
    g.behaviour = s.behaviour;
    g.complexity = s.complexity;
    const double wild = s.behaviour;

    if (g.on[kBurst])
    {
        // Conventional: one cluster at the end, leading into the downbeat. Unhinged: clusters anywhere,
        // more of them, some short and violent.
        const int n = 1 + (rng.chance (0.3 + 0.5 * wild) ? 1 : 0) + (rng.chance (wild * 0.5) ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            const double w = rng.uniform (0.08, 0.22) * (1.0 - 0.4 * wild * rng.uniform());
            const double at = (i == 0 && ! rng.chance (wild)) ? 1.0 - w : rng.uniform (0.05, 1.0 - w);
            g.bursts.push_back ({ at, at + w });
        }
    }
    if (g.on[kInterruption])
    {
        // A held breath before the downbeat (conventional), or holes punched anywhere (unhinged).
        const double unit = 0.25;
        if (! rng.chance (wild * 0.6))
        {
            const double len = unit * rng.range (1, 2);
            g.gaps.push_back ({ length - len - unit * rng.range (0, 2), length - unit * rng.range (0, 1) });
        }
        const int extra = (int) std::floor (wild * 2.5 + s.complexity * rng.uniform());
        for (int i = 0; i < extra; ++i)
        {
            const double len = unit * rng.range (1, 1 + (int) (3 * wild + 1));
            const double at = std::round (rng.uniform (0.0, std::max (unit, length - len)) / unit) * unit;
            g.gaps.push_back ({ at, at + len });
        }
        // Never a gap over the very first step: the fill should start.
        for (auto& gp : g.gaps) gp.first = std::max (gp.first, unit);
    }
    // Normalise the density curve to average 1, so Density stays the overall amount and the grammars
    // only move it around in time.
    double sum = 0.0;
    for (int i = 0; i < 128; ++i) sum += g.rawDensity ((i + 0.5) / 128.0);
    g.densityNorm = sum > 1.0e-9 ? 128.0 / sum : 1.0;
    return g;
}

double GrammarPlan::rawDensity (double t) const noexcept
{
    double d = 1.0;
    if (on[kBuild]) d *= 0.3 + 1.4 * std::pow (t, 1.2);
    if (on[kRelease]) d *= 0.3 + 1.4 * std::pow (1.0 - t, 1.2);
    if (on[kEscalation]) d *= 0.65 + 0.7 * t;
    if (on[kBurst]) d *= inBurst (t) ? 3.0 : 0.55;
    if (on[kFragmentation]) d *= 0.85 + 0.3 * std::sin (t * 37.0 + complexity * 5.0) * std::sin (t * 11.0);
    return std::max (0.05, d);
}

double GrammarPlan::density (double t) const noexcept { return rawDensity (std::clamp (t, 0.0, 1.0)) * densityNorm; }

double GrammarPlan::stepScale (double t) const noexcept
{
    t = std::clamp (t, 0.0, 1.0);
    if (on[kAcceleration] && on[kDeceleration])   // rush to the middle, then pull back
    {
        const double u = 1.0 - std::abs (2.0 * t - 1.0);
        return 1.7 * std::pow (0.3, u);
    }
    if (on[kAcceleration]) return 1.8 * std::pow (0.45 / 1.8, std::pow (t, 0.8));
    if (on[kDeceleration]) return 0.45 * std::pow (1.8 / 0.45, std::pow (t, 0.8));
    return 1.0;
}

double GrammarPlan::intensity (double t) const noexcept
{
    t = std::clamp (t, 0.0, 1.0);
    double v = 1.0;
    if (on[kEscalation]) v *= 0.62 + 0.62 * t;
    if (on[kBuild]) v *= 0.86 + 0.26 * t;
    if (on[kRelease]) v *= 1.12 - 0.26 * t;
    if (on[kBurst] && inBurst (t)) v *= 1.12;
    return v;
}

double GrammarPlan::displacement (double t) const noexcept
{
    // A little from behaviour alone (unhinged playing doesn't sit on the grid); the Displacement
    // grammar adds an amount that grows through the fill.
    double d = 0.12 * behaviour * complexity;
    if (on[kDisplacement]) d += 0.1 + 0.75 * std::pow (std::clamp (t, 0.0, 1.0), 0.9);
    return std::clamp (d, 0.0, 1.0);
}

double GrammarPlan::fragmentation() const noexcept
{
    return on[kFragmentation] ? 0.45 + 0.5 * complexity : 0.0;
}

bool GrammarPlan::inGap (double beat) const noexcept
{
    for (const auto& [a, b] : gaps) if (beat >= a - 1.0e-9 && beat < b - 1.0e-9) return true;
    return false;
}

bool GrammarPlan::inBurst (double t) const noexcept
{
    for (const auto& [a, b] : bursts) if (t >= a && t < b) return true;
    return false;
}

juce::String GrammarPlan::describe() const
{
    juce::StringArray a;
    for (int i = 0; i < kNumGrammars; ++i) if (on[(size_t) i]) a.add (kGrammarNames[(size_t) i]);
    return a.joinIntoString (" + ");
}
} // namespace fm
