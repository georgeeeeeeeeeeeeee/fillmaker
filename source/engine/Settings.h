#pragma once
// Everything the generator reads. Plain data: the plug-in fills it from its parameters, tests and the
// command-line tool fill it directly.

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <vector>

namespace fm
{
enum class Resolution { eighth = 0, sixteenth, thirtySecond, sixtyFourth, triplet, quintuplet, septuplet, custom };
inline constexpr std::array<const char*, 8> kResolutionNames { "1/8", "1/16", "1/32", "1/64", "Triplets", "Quintuplets", "Septuplets", "Custom" };

enum class LengthChoice { quarterBar = 0, halfBar, oneBar, twoBars, fourBars, custom };
inline constexpr std::array<const char*, 6> kLengthNames { "1/4 bar", "1/2 bar", "1 bar", "2 bars", "4 bars", "Custom" };

enum class VelocityCurve { flat = 0, crescendo, decrescendo, swell, dip, random };
inline constexpr std::array<const char*, 6> kCurveNames { "Flat", "Crescendo", "Decrescendo", "Swell", "Dip", "Random" };

enum class PitchMotion { automatic = 0, weighted, repeated, ascending, descending, alternating, random };
inline constexpr std::array<const char*, 7> kMotionNames { "Auto", "Weighted", "Repeated", "Ascending", "Descending", "Alternating", "Random" };

enum EngineId { kProbability = 0, kEuclidean, kRatchet, kPermutation, kPolyrhythm, kNumEngines };
inline constexpr std::array<const char*, kNumEngines> kEngineNames { "Probability", "Euclidean", "Ratchet", "Permutation", "Polyrhythm" };

enum GrammarId { kBuild = 0, kRelease, kBurst, kCallResponse, kAcceleration, kDeceleration, kInterruption, kEscalation, kDisplacement, kFragmentation, kNumGrammars };
inline constexpr std::array<const char*, kNumGrammars> kGrammarNames { "Build", "Release", "Burst", "Call + Response", "Acceleration", "Deceleration", "Interruption", "Escalation", "Displacement", "Fragmentation" };

inline constexpr std::array<int, 8> kRatchetCounts { 2, 3, 4, 5, 7, 9, 11, 13 };
// Polyrhythm: Auto, then "a against b".
inline constexpr std::array<std::pair<int, int>, 9> kPolyRatios { { { 0, 0 }, { 3, 4 }, { 5, 4 }, { 7, 8 }, { 11, 16 }, { 3, 2 }, { 5, 3 }, { 7, 4 }, { 9, 8 } } };
inline constexpr std::array<const char*, 9> kPolyNames { "Auto", "3:4", "5:4", "7:8", "11:16", "3:2", "5:3", "7:4", "9:8" };

struct PitchEntry
{
    int pitch = 36;
    float weight = 1.0f;       // how often it's chosen, relative to the others
    float probability = 1.0f;  // the chance each of its notes plays (stored on the note)
    bool enabled = true;
    bool operator== (const PitchEntry&) const = default;
};

struct Settings
{
    // Generation
    float density = 0.5f;      // 0..1
    float complexity = 0.4f;   // 0..1
    float behaviour = 0.25f;   // 0 conventional .. 1 unhinged
    LengthChoice lengthChoice = LengthChoice::oneBar;
    double customLength = 4.0; // beats (Custom)
    double beatsPerBar = 4.0;
    Resolution resolution = Resolution::sixteenth;
    int customSubdivision = 6; // steps per beat (Custom)
    float swing = 0.0f;        // 0..1: off-beat 16ths (8ths at 1/8) pushed late, up to a dotted feel
    float humanise = 0.0f;     // 0..1: small random timing displacement
    // Velocity
    int velocityBase = 96;
    float velocityVariation = 0.25f;
    float accent = 0.5f;
    float ghost = 0.25f;
    VelocityCurve curve = VelocityCurve::flat;
    float curveDepth = 0.5f;
    // Pitch
    std::vector<PitchEntry> pool;   // empty: the whole range lo..hi
    int pitchLow = 0, pitchHigh = 127;
    PitchMotion motion = PitchMotion::automatic;
    // Engines and grammars (each can be on or off; several engines share a fill)
    std::array<bool, kNumEngines> engines { true, true, true, false, false };
    std::array<bool, kNumGrammars> grammars {};
    int euclidSteps = 0, euclidPulses = 0, euclidRotation = 0;   // 0: automatic
    int ratchetMax = 7;            // the largest ratchet count allowed (from kRatchetCounts)
    int polyRatio = 0;             // index into kPolyRatios
    // Groove-aware mode
    bool grooveAware = false;
    float grooveStrength = 0.6f;   // how closely the fill relates to the source groove

    double lengthBeats() const noexcept;
    double stepsPerBeat() const noexcept;    // the visible grid
    std::vector<PitchEntry> activePool() const;   // enabled, inside lo..hi; never empty

    juce::var toVar() const;
    static Settings fromVar (const juce::var&);
};

// Mutation: how much of the original to keep, and which dimensions may change.
struct MutationSettings
{
    float preservation = 0.6f;   // 1: very subtle, 0: substantial reinvention
    float timing = 0.6f, velocity = 0.6f, pitch = 0.4f, density = 0.5f, structure = 0.4f, subdivision = 0.3f, probability = 0.2f;
    double regionStart = 0.0, regionEnd = -1.0;   // beats; end <= start: the whole pattern
    std::vector<uint32_t> onlyIds;               // non-empty: only these events (and new ones near them)
};

const char* stringOf (Resolution);
juce::String noteName (int midi);   // Live's naming: 60 = C3
} // namespace fm
