#include "engine/Presets.h"

#include <functional>

namespace fm
{
namespace
{
using G = std::array<bool, kNumGrammars>;
using E = std::array<bool, kNumEngines>;

G grammars (std::initializer_list<GrammarId> on)
{
    G g {};
    for (auto i : on) g[(size_t) i] = true;
    return g;
}
E engines (std::initializer_list<EngineId> on)
{
    E e {};
    for (auto i : on) e[(size_t) i] = true;
    return e;
}

Preset make (const char* name, const char* cat, const std::function<void (Settings&)>& f, float preservation = 0.6f)
{
    Preset p;
    p.name = name;
    p.category = cat;
    f (p.settings);
    p.mutation.preservation = preservation;
    p.keepsPool = true;
    return p;
}
} // namespace

std::vector<PitchEntry> defaultPool()
{
    // Drum Rack's first 16 pads start at C1 (36). Weighted so the lower pads (often kick, snare,
    // toms) come up more, but nothing about them is assumed beyond that.
    std::vector<PitchEntry> p;
    const float w[] = { 0.9f, 0.4f, 1.0f, 0.3f, 0.6f, 0.7f, 0.5f, 0.7f, 0.3f, 0.7f, 0.3f, 0.6f, 0.2f, 0.4f, 0.2f, 0.3f };
    for (int i = 0; i < 16; ++i) p.push_back ({ 36 + i, w[i], 1.0f, i < 8 });
    return p;
}

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list = [] {
        std::vector<Preset> v;
        v.push_back (make ("Init", "Basics", [] (Settings&) {}));
        v.push_back (make ("Snare Roll Up", "Classic", [] (Settings& s) {
            s.density = 0.7f; s.complexity = 0.15f; s.behaviour = 0.05f; s.resolution = Resolution::sixteenth;
            s.engines = engines ({ kProbability, kRatchet }); s.grammars = grammars ({ kBuild });
            s.curve = VelocityCurve::crescendo; s.curveDepth = 0.8f; s.motion = PitchMotion::repeated; s.ghost = 0.4f; }));
        v.push_back (make ("Tom Cascade", "Classic", [] (Settings& s) {
            s.density = 0.65f; s.complexity = 0.25f; s.behaviour = 0.1f; s.resolution = Resolution::sixteenth;
            s.engines = engines ({ kProbability }); s.motion = PitchMotion::descending; s.accent = 0.6f; s.ghost = 0.1f; }));
        v.push_back (make ("Triplet Turnaround", "Classic", [] (Settings& s) {
            s.density = 0.55f; s.complexity = 0.3f; s.behaviour = 0.1f; s.resolution = Resolution::triplet;
            s.lengthChoice = LengthChoice::halfBar; s.engines = engines ({ kProbability, kEuclidean });
            s.motion = PitchMotion::alternating; s.grammars = grammars ({ kInterruption }); }));
        v.push_back (make ("Ghost Shuffle", "Groove", [] (Settings& s) {
            s.density = 0.45f; s.complexity = 0.35f; s.behaviour = 0.15f; s.swing = 0.55f; s.ghost = 0.75f; s.accent = 0.7f;
            s.engines = engines ({ kProbability, kPermutation }); s.humanise = 0.25f; }));
        v.push_back (make ("Euclid Spiral", "Geometric", [] (Settings& s) {
            s.density = 0.5f; s.complexity = 0.6f; s.behaviour = 0.3f; s.lengthChoice = LengthChoice::twoBars;
            s.engines = engines ({ kEuclidean }); s.motion = PitchMotion::ascending; s.grammars = grammars ({ kEscalation }); }));
        v.push_back (make ("Polymeter Spill", "Geometric", [] (Settings& s) {
            s.density = 0.5f; s.complexity = 0.7f; s.behaviour = 0.35f; s.lengthChoice = LengthChoice::twoBars;
            s.engines = engines ({ kPolyrhythm, kProbability }); s.polyRatio = 2; }));
        v.push_back (make ("Ratchet Storm", "Machine", [] (Settings& s) {
            s.density = 0.75f; s.complexity = 0.7f; s.behaviour = 0.55f; s.resolution = Resolution::thirtySecond;
            s.engines = engines ({ kRatchet }); s.ratchetMax = 13; s.grammars = grammars ({ kBurst, kEscalation }); }));
        v.push_back (make ("Quintuplet Turn", "Odd", [] (Settings& s) {
            s.density = 0.55f; s.complexity = 0.6f; s.behaviour = 0.3f; s.resolution = Resolution::quintuplet;
            s.engines = engines ({ kProbability, kRatchet }); s.motion = PitchMotion::descending; }));
        v.push_back (make ("Septuplet Drift", "Odd", [] (Settings& s) {
            s.density = 0.5f; s.complexity = 0.7f; s.behaviour = 0.45f; s.resolution = Resolution::septuplet;
            s.engines = engines ({ kProbability, kEuclidean }); s.grammars = grammars ({ kDisplacement }); }));
        v.push_back (make ("Call and Response", "Structure", [] (Settings& s) {
            s.density = 0.55f; s.complexity = 0.45f; s.behaviour = 0.25f; s.lengthChoice = LengthChoice::oneBar;
            s.engines = engines ({ kProbability, kPermutation }); s.grammars = grammars ({ kCallResponse }); }));
        v.push_back (make ("Broken Build", "Structure", [] (Settings& s) {
            s.density = 0.6f; s.complexity = 0.55f; s.behaviour = 0.45f; s.lengthChoice = LengthChoice::twoBars;
            s.engines = engines ({ kProbability, kRatchet, kPermutation }); s.grammars = grammars ({ kBuild, kDisplacement, kBurst }); }));
        v.push_back (make ("Accelerando", "Structure", [] (Settings& s) {
            s.density = 0.7f; s.complexity = 0.4f; s.behaviour = 0.2f; s.engines = engines ({ kProbability, kRatchet });
            s.grammars = grammars ({ kAcceleration, kEscalation }); s.curve = VelocityCurve::crescendo; }));
        v.push_back (make ("Falling Apart", "Structure", [] (Settings& s) {
            s.density = 0.65f; s.complexity = 0.6f; s.behaviour = 0.55f; s.engines = engines ({ kProbability, kPermutation, kRatchet });
            s.grammars = grammars ({ kRelease, kDeceleration, kFragmentation }); s.curve = VelocityCurve::decrescendo; }));
        v.push_back (make ("Stop-Start", "Structure", [] (Settings& s) {
            s.density = 0.7f; s.complexity = 0.5f; s.behaviour = 0.4f; s.engines = engines ({ kProbability, kRatchet, kEuclidean });
            s.grammars = grammars ({ kInterruption, kFragmentation }); }));
        v.push_back (make ("Glitch Confetti", "Unhinged", [] (Settings& s) {
            s.density = 0.7f; s.complexity = 0.85f; s.behaviour = 0.85f; s.resolution = Resolution::thirtySecond;
            s.engines = engines ({ kProbability, kRatchet, kPolyrhythm, kPermutation }); s.ratchetMax = 13;
            s.grammars = grammars ({ kBurst, kInterruption, kDisplacement }); s.motion = PitchMotion::random; s.velocityVariation = 0.6f; }, 0.4f));
        v.push_back (make ("Eleven Against Sixteen", "Unhinged", [] (Settings& s) {
            s.density = 0.55f; s.complexity = 0.9f; s.behaviour = 0.75f; s.lengthChoice = LengthChoice::twoBars;
            s.engines = engines ({ kPolyrhythm, kEuclidean }); s.polyRatio = 4; s.euclidSteps = 11; }, 0.4f));
        v.push_back (make ("Total Chaos", "Unhinged", [] (Settings& s) {
            s.density = 0.8f; s.complexity = 1.0f; s.behaviour = 1.0f; s.engines = engines ({ kProbability, kEuclidean, kRatchet, kPermutation, kPolyrhythm });
            s.ratchetMax = 13; s.grammars = grammars ({ kBurst, kInterruption, kDisplacement, kFragmentation, kEscalation });
            s.motion = PitchMotion::random; s.velocityVariation = 0.8f; s.humanise = 0.4f; }, 0.2f));
        v.push_back (make ("Sparse Thoughts", "Space", [] (Settings& s) {
            s.density = 0.15f; s.complexity = 0.5f; s.behaviour = 0.4f; s.lengthChoice = LengthChoice::twoBars;
            s.engines = engines ({ kProbability, kEuclidean }); s.grammars = grammars ({ kInterruption }); s.ghost = 0.5f; }));
        return v;
    }();
    return list;
}

Settings applyPreset (const Preset& p, const Settings& current)
{
    Settings s = p.settings;
    if (p.keepsPool)
    {
        s.pool = current.pool;
        s.pitchLow = current.pitchLow;
        s.pitchHigh = current.pitchHigh;
        s.beatsPerBar = current.beatsPerBar;
    }
    return s;
}

juce::var Preset::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "fillmaker-preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    o->setProperty ("category", category);
    o->setProperty ("settings", settings.toVar());
    o->setProperty ("mutation", fm::toVar (mutation));
    if (hasPattern) o->setProperty ("pattern", pattern.toVar());
    return juce::var (o);
}

Preset Preset::fromVar (const juce::var& v)
{
    Preset p;
    p.name = v.getProperty ("name", "Preset").toString();
    p.category = v.getProperty ("category", "User").toString();
    p.settings = Settings::fromVar (v.getProperty ("settings", {}));
    p.mutation = mutationFromVar (v.getProperty ("mutation", {}));
    const auto pat = v.getProperty ("pattern", {});
    p.hasPattern = pat.isObject();
    if (p.hasPattern) p.pattern = Pattern::fromVar (pat);
    p.keepsPool = false;
    return p;
}

juce::File userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Application Support")
#endif
        .getChildFile ("LeverStudios").getChildFile ("Fill Maker").getChildFile ("Presets");
}

bool saveUserPreset (const juce::File& f, const Preset& p, juce::String* error)
{
    f.getParentDirectory().createDirectory();
    if (! f.replaceWithText (juce::JSON::toString (p.toVar(), false)))
    {
        if (error != nullptr) *error = "Couldn't save " + f.getFullPathName();
        return false;
    }
    return true;
}

bool loadUserPreset (const juce::File& f, Preset& p, juce::String* error)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v.getProperty ("format", "").toString() != "fillmaker-preset")
    {
        if (error != nullptr) *error = f.getFileName() + " isn't a Fill Maker preset";
        return false;
    }
    p = Preset::fromVar (v);
    if (p.name.isEmpty()) p.name = f.getFileNameWithoutExtension();
    return true;
}

juce::Array<juce::File> listUserPresets (const juce::File& folder)
{
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.fillpreset");
    files.sort();
    return files;
}

bool savePatternFile (const juce::File& f, const Pattern& p, juce::String* error)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "fillmaker-pattern");
    o->setProperty ("pattern", p.toVar());
    f.getParentDirectory().createDirectory();
    if (! f.replaceWithText (juce::JSON::toString (juce::var (o), false)))
    {
        if (error != nullptr) *error = "Couldn't save " + f.getFullPathName();
        return false;
    }
    return true;
}

bool loadPatternFile (const juce::File& f, Pattern& p, juce::String* error)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v.getProperty ("format", "").toString() != "fillmaker-pattern")
    {
        if (error != nullptr) *error = f.getFileName() + " isn't a Fill Maker pattern";
        return false;
    }
    p = Pattern::fromVar (v.getProperty ("pattern", {}));
    return true;
}
} // namespace fm
