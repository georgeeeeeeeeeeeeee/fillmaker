#pragma once
// Generate: settings + seed (+ an optional groove) -> a pattern. Deterministic: the same settings and
// seed always give the same fill, on any machine.

#include "engine/Engines.h"
#include "engine/Pattern.h"

namespace fm
{
struct GenerationReport
{
    std::vector<Span> spans;   // how the fill was built, span by span
    juce::String grammars;
    juce::String summary() const;   // "1/16 | prob 5 | ratchet 7 | poly 5:4 over 4.0 beats"
};

Pattern generate (const Settings&, uint32_t seed, const GrooveProfile* groove = nullptr, GenerationReport* report = nullptr);

// Swing for a note at `time`: the off-beat 16ths (8ths at 1/8 and triplet grids) pushed late.
double swingOffset (double time, const Settings&) noexcept;
} // namespace fm
