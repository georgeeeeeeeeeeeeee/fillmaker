#include "engine/Settings.h"

#include <algorithm>
#include <cmath>

namespace fm
{
double Settings::lengthBeats() const noexcept
{
    const double bar = beatsPerBar > 0.0 ? beatsPerBar : 4.0;
    switch (lengthChoice)
    {
        case LengthChoice::quarterBar: return bar * 0.25;
        case LengthChoice::halfBar: return bar * 0.5;
        case LengthChoice::oneBar: return bar;
        case LengthChoice::twoBars: return bar * 2.0;
        case LengthChoice::fourBars: return bar * 4.0;
        case LengthChoice::custom: default: return std::clamp (std::isfinite (customLength) ? customLength : 4.0, 0.25, 64.0);
    }
}

double Settings::stepsPerBeat() const noexcept
{
    switch (resolution)
    {
        case Resolution::eighth: return 2.0;
        case Resolution::sixteenth: return 4.0;
        case Resolution::thirtySecond: return 8.0;
        case Resolution::sixtyFourth: return 16.0;
        case Resolution::triplet: return 6.0;      // 16th-note triplets
        case Resolution::quintuplet: return 5.0;
        case Resolution::septuplet: return 7.0;
        case Resolution::custom: default: return (double) std::clamp (customSubdivision, 1, 32);
    }
}

std::vector<PitchEntry> Settings::activePool() const
{
    const int lo = std::clamp (std::min (pitchLow, pitchHigh), 0, 127), hi = std::clamp (std::max (pitchLow, pitchHigh), 0, 127);
    std::vector<PitchEntry> out;
    for (const auto& e : pool)
        if (e.enabled && e.weight > 0.0f && e.pitch >= lo && e.pitch <= hi) out.push_back (e);
    if (out.empty())
    {
        // No pool (or nothing in range): up to 8 pitches spread over the range.
        const int n = std::min (8, hi - lo + 1);
        for (int i = 0; i < n; ++i)
        {
            PitchEntry e;
            e.pitch = n == 1 ? lo : lo + (int) std::lround ((double) i * (hi - lo) / (n - 1));
            out.push_back (e);
        }
    }
    std::sort (out.begin(), out.end(), [] (const PitchEntry& a, const PitchEntry& b) { return a.pitch < b.pitch; });
    out.erase (std::unique (out.begin(), out.end(), [] (const PitchEntry& a, const PitchEntry& b) { return a.pitch == b.pitch; }), out.end());
    return out;
}

juce::var Settings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("density", density);
    o->setProperty ("complexity", complexity);
    o->setProperty ("behaviour", behaviour);
    o->setProperty ("length", (int) lengthChoice);
    o->setProperty ("customLength", customLength);
    o->setProperty ("beatsPerBar", beatsPerBar);
    o->setProperty ("resolution", (int) resolution);
    o->setProperty ("customSubdivision", customSubdivision);
    o->setProperty ("swing", swing);
    o->setProperty ("humanise", humanise);
    o->setProperty ("velocityBase", velocityBase);
    o->setProperty ("velocityVariation", velocityVariation);
    o->setProperty ("accent", accent);
    o->setProperty ("ghost", ghost);
    o->setProperty ("curve", (int) curve);
    o->setProperty ("curveDepth", curveDepth);
    o->setProperty ("pitchLow", pitchLow);
    o->setProperty ("pitchHigh", pitchHigh);
    o->setProperty ("motion", (int) motion);
    juce::Array<juce::var> poolList, eng, gram;
    for (const auto& e : pool)
    {
        auto* x = new juce::DynamicObject();
        x->setProperty ("pitch", e.pitch);
        x->setProperty ("weight", e.weight);
        x->setProperty ("probability", e.probability);
        x->setProperty ("enabled", e.enabled);
        poolList.add (juce::var (x));
    }
    for (bool b : engines) eng.add (b);
    for (bool b : grammars) gram.add (b);
    o->setProperty ("pool", poolList);
    o->setProperty ("engines", eng);
    o->setProperty ("grammars", gram);
    o->setProperty ("euclidSteps", euclidSteps);
    o->setProperty ("euclidPulses", euclidPulses);
    o->setProperty ("euclidRotation", euclidRotation);
    o->setProperty ("ratchetMax", ratchetMax);
    o->setProperty ("polyRatio", polyRatio);
    o->setProperty ("grooveAware", grooveAware);
    o->setProperty ("grooveStrength", grooveStrength);
    return juce::var (o);
}

Settings Settings::fromVar (const juce::var& v)
{
    Settings s;
    if (! v.isObject()) return s;
    const auto num = [&] (const juce::var& o, const char* k, double d) {
        const auto x = o.getProperty (k, d);
        const double r = x.isDouble() || x.isInt() || x.isInt64() || x.isBool() ? (double) x : d;
        return std::isfinite (r) ? r : d;
    };
    const auto f01 = [&] (const char* k, float d) { return (float) std::clamp (num (v, k, d), 0.0, 1.0); };
    s.density = f01 ("density", s.density);
    s.complexity = f01 ("complexity", s.complexity);
    s.behaviour = f01 ("behaviour", s.behaviour);
    s.lengthChoice = (LengthChoice) std::clamp ((int) num (v, "length", 2), 0, 5);
    s.customLength = std::clamp (num (v, "customLength", 4.0), 0.25, 64.0);
    s.beatsPerBar = std::clamp (num (v, "beatsPerBar", 4.0), 1.0, 16.0);
    s.resolution = (Resolution) std::clamp ((int) num (v, "resolution", 1), 0, 7);
    s.customSubdivision = std::clamp ((int) num (v, "customSubdivision", 6), 1, 32);
    s.swing = f01 ("swing", 0.0f);
    s.humanise = f01 ("humanise", 0.0f);
    s.velocityBase = std::clamp ((int) num (v, "velocityBase", 96), 1, 127);
    s.velocityVariation = f01 ("velocityVariation", s.velocityVariation);
    s.accent = f01 ("accent", s.accent);
    s.ghost = f01 ("ghost", s.ghost);
    s.curve = (VelocityCurve) std::clamp ((int) num (v, "curve", 0), 0, 5);
    s.curveDepth = f01 ("curveDepth", s.curveDepth);
    s.pitchLow = std::clamp ((int) num (v, "pitchLow", 0), 0, 127);
    s.pitchHigh = std::clamp ((int) num (v, "pitchHigh", 127), 0, 127);
    s.motion = (PitchMotion) std::clamp ((int) num (v, "motion", 0), 0, 6);
    if (const auto* list = v.getProperty ("pool", {}).getArray())
        for (const auto& x : *list)
        {
            PitchEntry e;
            e.pitch = std::clamp ((int) num (x, "pitch", 36), 0, 127);
            e.weight = (float) std::clamp (num (x, "weight", 1.0), 0.0, 1.0);
            e.probability = (float) std::clamp (num (x, "probability", 1.0), 0.0, 1.0);
            e.enabled = (bool) x.getProperty ("enabled", true);
            if (s.pool.size() < 32) s.pool.push_back (e);
        }
    if (const auto* list = v.getProperty ("engines", {}).getArray())
        for (int i = 0; i < kNumEngines && i < list->size(); ++i) s.engines[(size_t) i] = (bool) (*list)[i];
    if (const auto* list = v.getProperty ("grammars", {}).getArray())
        for (int i = 0; i < kNumGrammars && i < list->size(); ++i) s.grammars[(size_t) i] = (bool) (*list)[i];
    s.euclidSteps = std::clamp ((int) num (v, "euclidSteps", 0), 0, 64);
    s.euclidPulses = std::clamp ((int) num (v, "euclidPulses", 0), 0, 64);
    s.euclidRotation = std::clamp ((int) num (v, "euclidRotation", 0), 0, 63);
    s.ratchetMax = std::clamp ((int) num (v, "ratchetMax", 7), 2, 13);
    s.polyRatio = std::clamp ((int) num (v, "polyRatio", 0), 0, (int) kPolyRatios.size() - 1);
    s.grooveAware = (bool) v.getProperty ("grooveAware", false);
    s.grooveStrength = f01 ("grooveStrength", s.grooveStrength);
    return s;
}

const char* stringOf (Resolution r) { return kResolutionNames[(size_t) std::clamp ((int) r, 0, 7)]; }

juce::String noteName (int midi)
{
    static const char* const n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    midi = std::clamp (midi, 0, 127);
    return juce::String (n[midi % 12]) + juce::String (midi / 12 - 2);
}
} // namespace fm
