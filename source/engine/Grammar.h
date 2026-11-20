#pragma once
// Rhythmic grammars: how a fill develops over its length. They are not effects applied afterwards: the
// engines ask the plan, at every position they consider, how dense to be, how long a step is, how
// hard to play, how far to displace, and whether this moment is a gap or a burst. Grammars combine
// (their curves multiply), so BUILD + DISPLACEMENT + BURST opens plainly, drifts off the grid as it
// goes, and ends in a cluster.

#include "engine/Random.h"
#include "engine/Settings.h"

#include <utility>
#include <vector>

namespace fm
{
struct GrammarPlan
{
    std::array<bool, kNumGrammars> on {};
    double length = 4.0, behaviour = 0.0, complexity = 0.0;
    std::vector<std::pair<double, double>> bursts;   // windows, as fractions of the pattern
    std::vector<std::pair<double, double>> gaps;     // windows, in beats
    double densityNorm = 1.0;

    static GrammarPlan make (const Settings&, double length, Rng&);

    double density (double t) const noexcept;        // multiplier (averages about 1 over the fill)
    double stepScale (double t) const noexcept;      // 1: the grid's step; < 1 faster (Acceleration)
    bool warped() const noexcept { return on[kAcceleration] || on[kDeceleration]; }
    double intensity (double t) const noexcept;      // velocity multiplier (Escalation, Build, Release)
    double displacement (double t) const noexcept;   // 0..1: how far and how often events leave the grid
    double fragmentation() const noexcept;           // 0..1
    bool callAndResponse() const noexcept { return on[kCallResponse]; }
    bool inGap (double beat) const noexcept;
    bool inBurst (double t) const noexcept;
    juce::String describe() const;

private:
    double rawDensity (double t) const noexcept;
};
} // namespace fm
