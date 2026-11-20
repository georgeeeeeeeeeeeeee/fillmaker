#include "engine/Pitch.h"

#include <algorithm>
#include <cmath>

namespace fm
{
PitchPicker::PitchPicker (const Settings& s, const GrooveProfile* groove) : behaviour_ (s.behaviour)
{
    pool_ = s.activePool();
    for (const auto& e : pool_) weights_.push_back (std::max (0.0f, e.weight));
    // Groove-aware: the source's own pitches gain weight (and join the pool if they're in range and
    // the user hasn't defined a pool), so the fill speaks the groove's language.
    if (groove != nullptr && s.grooveAware && groove->valid())
    {
        const double k = s.grooveStrength;
        const int lo = std::min (s.pitchLow, s.pitchHigh), hi = std::max (s.pitchLow, s.pitchHigh);
        for (const auto& [p, share] : groove->pitches)
        {
            if (p < lo || p > hi) continue;
            auto it = std::find_if (pool_.begin(), pool_.end(), [p = p] (const PitchEntry& e) { return e.pitch == p; });
            if (it != pool_.end()) weights_[(size_t) (it - pool_.begin())] *= 1.0 + 3.0 * k * share;
            else if (s.pool.empty() && pool_.size() < 16)
            {
                PitchEntry e;
                e.pitch = p;
                pool_.push_back (e);
                weights_.push_back (3.0 * k * share);
            }
        }
        // Keep the low-to-high order the motions rely on.
        std::vector<size_t> order (pool_.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort (order.begin(), order.end(), [this] (size_t a, size_t b) { return pool_[a].pitch < pool_[b].pitch; });
        std::vector<PitchEntry> p2;
        std::vector<double> w2;
        for (auto i : order) { p2.push_back (pool_[i]); w2.push_back (weights_[i]); }
        pool_.swap (p2);
        weights_.swap (w2);
    }
}

PitchMotion PitchPicker::resolve (Rng& rng) const
{
    // Conventional fills mostly use weighted choices, falling runs (tom rolls) and two-pitch
    // alternation; unhinged ones jump at random and climb.
    const double b = behaviour_;
    const std::vector<double> w { 0.42 - 0.2 * b,  // weighted
                                  0.16 - 0.06 * b, // repeated
                                  0.08 + 0.12 * b, // ascending
                                  0.2 - 0.08 * b,  // descending
                                  0.14,            // alternating
                                  0.02 + 0.4 * b }; // random
    const int i = rng.weighted (w);
    return (PitchMotion) (1 + std::max (0, i));
}

void PitchPicker::beginPhrase (PitchMotion m, Rng& rng)
{
    motion_ = m == PitchMotion::automatic ? resolve (rng) : m;
    const int n = size();
    cursor_ = motion_ == PitchMotion::descending ? n - 1 : 0;
    if (motion_ == PitchMotion::ascending || motion_ == PitchMotion::descending)
    {
        // Start somewhere sensible: a run starts near its top (descending) or bottom (ascending).
        const int span = std::max (1, n / 3);
        cursor_ = motion_ == PitchMotion::descending ? n - 1 - rng.below (span) : rng.below (span);
    }
    altA_ = pickWeighted (rng);
    altB_ = pickOther (rng, altA_);
    last_ = -1;
    ++phraseCount_;
}

int PitchPicker::pickWeighted (Rng& rng) const
{
    const int i = rng.weighted (weights_);
    return pool_[(size_t) std::max (0, i)].pitch;
}

int PitchPicker::pickOther (Rng& rng, int notThis) const
{
    if (size() < 2) return notThis;
    for (int tries = 0; tries < 8; ++tries)
        if (const int p = pickWeighted (rng); p != notThis) return p;
    for (const auto& e : pool_) if (e.pitch != notThis) return e.pitch;
    return notThis;
}

int PitchPicker::next (Rng& rng, double metric)
{
    const int n = size();
    int p = pool_[0].pitch;
    switch (motion_)
    {
        case PitchMotion::repeated:
            p = last_ >= 0 && rng.chance (0.85) ? last_ : pickWeighted (rng);
            break;
        case PitchMotion::ascending:
        case PitchMotion::descending:
        {
            p = pool_[(size_t) std::clamp (cursor_, 0, n - 1)].pitch;
            // Usually one step at a time; now and then a pitch is repeated (a double on one drum).
            if (! rng.chance (0.25 - 0.15 * behaviour_))
            {
                const int dir = motion_ == PitchMotion::ascending ? 1 : -1;
                const int jump = rng.chance (behaviour_ * 0.4) ? 2 : 1;
                cursor_ += dir * jump;
                if (cursor_ >= n || cursor_ < 0) cursor_ = dir > 0 ? rng.below (std::max (1, n / 2)) : n - 1 - rng.below (std::max (1, n / 2));
            }
            break;
        }
        case PitchMotion::alternating:
            p = (last_ == altA_) ? altB_ : altA_;
            if (rng.chance (0.08 + 0.2 * behaviour_)) p = pickWeighted (rng);
            break;
        case PitchMotion::random:
            p = pool_[(size_t) rng.below (n)].pitch;   // weights ignored: anything goes
            break;
        case PitchMotion::weighted:
        case PitchMotion::automatic:
        default:
            // Strong positions lean to the pool's lower half, weak ones to the upper: no meaning is
            // assumed, but low/high contrast reads as structure on almost any kit or sample set.
            if (n > 2 && rng.chance (0.35 * (1.0 - behaviour_)))
            {
                const int half = n / 2;
                p = metric > 0.6 ? pool_[(size_t) rng.below (std::max (1, half))].pitch : pool_[(size_t) (half + rng.below (n - half))].pitch;
            }
            else
                p = pickWeighted (rng);
            break;
    }
    last_ = p;
    return p;
}

float PitchPicker::probabilityOf (int pitch) const noexcept
{
    for (const auto& e : pool_) if (e.pitch == pitch) return e.probability;
    return 1.0f;
}
} // namespace fm
