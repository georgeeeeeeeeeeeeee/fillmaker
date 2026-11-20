#pragma once
// Rhythm basics shared by the engines: how strong a moment is in the bar, Euclidean patterns, and the
// grid of candidate positions a span of a fill is built on (regular, or warped when a grammar
// accelerates or decelerates it).

#include "engine/Random.h"

#include <vector>

namespace fm
{
// How strong a position is: 1 on the downbeat, falling through beats, half beats, quarter beats, ...
// Tuplet positions (thirds, fifths, sevenths of a beat) count as weak but not weakest.
double metricWeight (double beat, double beatsPerBar) noexcept;

// Bjorklund: `pulses` onsets spread as evenly as possible over `steps`, rotated. E(3, 8) = x..x..x.
std::vector<bool> euclid (int pulses, int steps, int rotation = 0);

struct GridPoint
{
    double time;     // beats from the pattern start
    double metric;   // metricWeight at that time
    int index;       // step number within its span
};

// Candidate positions from `start` to `start + length` at `stepsPerBeat`. With a warp (a function of
// pattern position 0..1 giving a step-size multiplier), the steps stretch or shrink along the way.
using Warp = double (*) (double t, const void* ctx);
std::vector<GridPoint> buildGrid (double start, double length, double stepsPerBeat, double beatsPerBar, double patternLength,
                                  Warp warp = nullptr, const void* warpCtx = nullptr);
} // namespace fm
