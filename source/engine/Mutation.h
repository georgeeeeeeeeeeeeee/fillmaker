#pragma once
// Mutate: a pattern changed while keeping some of its identity. Preservation decides how much (100%
// very subtle, 0% close to a new fill); each dimension can be turned down separately; a region or a
// set of selected notes limits where it happens. Untouched notes keep their ids.

#include "engine/Generator.h"

namespace fm
{
Pattern mutate (const Pattern&, const Settings&, const MutationSettings&, uint32_t seed, juce::String* whatChanged = nullptr);

// How alike two patterns are: the share of notes in `a` that `b` still has (same pitch, within a
// 64th note), averaged both ways. 1 = identical, 0 = nothing in common.
double similarity (const Pattern& a, const Pattern& b);

juce::var toVar (const MutationSettings&);
MutationSettings mutationFromVar (const juce::var&);
} // namespace fm
