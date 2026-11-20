#pragma once
// Picking pitches: from the user's pool (weights, per-pitch probability), in a motion (repeated,
// ascending, descending, alternating, random) chosen per phrase. No assumptions about what a pitch
// means: the pool's order is just low to high.

#include "engine/Groove.h"
#include "engine/Random.h"
#include "engine/Settings.h"

namespace fm
{
class PitchPicker
{
public:
    PitchPicker (const Settings&, const GrooveProfile* groove);

    PitchMotion resolve (Rng&) const;             // Auto -> a motion, by behaviour
    void beginPhrase (PitchMotion, Rng&);         // a span starts: new motion state
    int next (Rng&, double metric);               // the next pitch
    int pickWeighted (Rng&) const;                // one pitch by weight alone
    int pickOther (Rng&, int notThis) const;      // a different pitch (when there is one)
    float probabilityOf (int pitch) const noexcept;
    int size() const noexcept { return (int) pool_.size(); }
    int lowest() const noexcept { return pool_.front().pitch; }
    int highest() const noexcept { return pool_.back().pitch; }
    const std::vector<PitchEntry>& pool() const noexcept { return pool_; }

private:
    std::vector<PitchEntry> pool_;   // sorted low to high, never empty
    std::vector<double> weights_;
    double behaviour_ = 0.0;
    PitchMotion motion_ = PitchMotion::weighted;
    int last_ = -1, cursor_ = 0, altA_ = 0, altB_ = 0, phraseCount_ = 0;
};
} // namespace fm
