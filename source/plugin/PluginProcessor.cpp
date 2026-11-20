#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"

namespace fm
{
namespace
{
constexpr int kStateVersion = 1;
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; }
juce::StringArray names (const auto& arr) { juce::StringArray a; for (auto* c : arr) a.add (c); return a; }
} // namespace

// ---------------------------------------------------------------------------------------------- history
void PatternHistory::push (Entry e)
{
    if (cursor_ + 1 < (int) list_.size()) list_.erase (list_.begin() + cursor_ + 1, list_.end());
    list_.push_back (std::move (e));
    if ((int) list_.size() > kMax) list_.erase (list_.begin());
    cursor_ = (int) list_.size() - 1;
}
const PatternHistory::Entry* PatternHistory::undo() noexcept { if (! canUndo()) return nullptr; --cursor_; return current(); }
const PatternHistory::Entry* PatternHistory::redo() noexcept { if (! canRedo()) return nullptr; ++cursor_; return current(); }
const PatternHistory::Entry* PatternHistory::jump (int i) noexcept { if (i < 0 || i >= (int) list_.size()) return nullptr; cursor_ = i; return current(); }

juce::var PatternHistory::toVar() const
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> a;
    // The last 32 are saved with the set.
    const int from = std::max (0, (int) list_.size() - 32);
    for (int i = from; i < (int) list_.size(); ++i)
    {
        const auto& e = list_[(size_t) i];
        auto* x = new juce::DynamicObject();
        x->setProperty ("pattern", e.pattern.toVar());
        x->setProperty ("label", e.label);
        x->setProperty ("structure", e.structure);
        x->setProperty ("seed", (juce::int64) e.seed);
        x->setProperty ("generated", e.generated);
        a.add (juce::var (x));
    }
    o->setProperty ("entries", a);
    o->setProperty ("cursor", std::max (0, cursor_ - from));
    return juce::var (o);
}

void PatternHistory::fromVar (const juce::var& v)
{
    clear();
    if (const auto* a = v.getProperty ("entries", {}).getArray())
        for (const auto& x : *a)
            list_.push_back ({ Pattern::fromVar (x.getProperty ("pattern", {})), x.getProperty ("label", "").toString(),
                               x.getProperty ("structure", "").toString(), (uint32_t) (juce::int64) x.getProperty ("seed", 0),
                               (bool) x.getProperty ("generated", false) });
    cursor_ = list_.empty() ? -1 : std::clamp ((int) v.getProperty ("cursor", (int) list_.size() - 1), 0, (int) list_.size() - 1);
}

// ---------------------------------------------------------------------------------------------- parameters
juce::String FillMakerProcessor::eid (int e) { return "eng" + juce::String (e); }
juce::String FillMakerProcessor::gid (int g) { return "gram" + juce::String (g); }

juce::AudioProcessorValueTreeState::ParameterLayout FillMakerProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    using F = juce::AudioParameterFloat;
    using I = juce::AudioParameterInt;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;
    using A = juce::AudioParameterFloatAttributes;
    const auto id = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };
    const auto percent = [&] (const char* i, const char* n, float def) { return std::make_unique<F> (id (i), n, juce::NormalisableRange<float> (0.0f, 100.0f), def, A().withLabel ("%").withStringFromValueFunction (pct)); };
    l.add (percent ("density", "Density", 50.0f));
    l.add (percent ("complexity", "Complexity", 40.0f));
    l.add (std::make_unique<F> (id ("behaviour"), "Behaviour", juce::NormalisableRange<float> (0.0f, 100.0f), 25.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return v < 0.5f ? juce::String ("Conventional") : v > 99.5f ? juce::String ("Unhinged") : juce::String (juce::roundToInt (v)) + "%"; })));
    l.add (std::make_unique<C> (id ("length"), "Length", names (kLengthNames), 2));
    l.add (std::make_unique<F> (id ("customLength"), "Custom Length", juce::NormalisableRange<float> (0.25f, 32.0f, 0.25f), 6.0f,
                                A().withLabel ("beats").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " beats"; })));
    l.add (std::make_unique<C> (id ("resolution"), "Resolution", names (kResolutionNames), 1));
    l.add (std::make_unique<I> (id ("customSub"), "Custom Subdivision", 1, 32, 6, juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return juce::String (v) + "/beat"; })));
    l.add (percent ("swing", "Swing", 0.0f));
    l.add (percent ("humanise", "Humanise", 0.0f));
    l.add (std::make_unique<I> (id ("velBase"), "Velocity", 1, 127, 96));
    l.add (percent ("velVar", "Velocity Variation", 25.0f));
    l.add (percent ("accent", "Accents", 50.0f));
    l.add (percent ("ghost", "Ghost Notes", 25.0f));
    l.add (std::make_unique<C> (id ("curve"), "Velocity Curve", names (kCurveNames), 0));
    l.add (percent ("curveDepth", "Curve Depth", 50.0f));
    const auto noteText = juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v) + " (" + juce::String (v) + ")"; });
    l.add (std::make_unique<I> (id ("pitchLow"), "Lowest Pitch", 0, 127, 0, noteText));
    l.add (std::make_unique<I> (id ("pitchHigh"), "Highest Pitch", 0, 127, 127, noteText));
    l.add (std::make_unique<C> (id ("motion"), "Pitch Motion", names (kMotionNames), 0));
    const bool engDefault[] = { true, true, true, false, false };
    for (int e = 0; e < kNumEngines; ++e) l.add (std::make_unique<B> (id (eid (e)), juce::String (kEngineNames[(size_t) e]) + " Engine", engDefault[e]));
    for (int g = 0; g < kNumGrammars; ++g) l.add (std::make_unique<B> (id (gid (g)), juce::String (kGrammarNames[(size_t) g]), false));
    const auto autoInt = juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? juce::String ("Auto") : juce::String (v); });
    l.add (std::make_unique<I> (id ("euclidSteps"), "Euclid Steps", 0, 32, 0, autoInt));
    l.add (std::make_unique<I> (id ("euclidPulses"), "Euclid Pulses", 0, 32, 0, autoInt));
    l.add (std::make_unique<I> (id ("euclidRotation"), "Euclid Rotation", 0, 31, 0));
    juce::StringArray counts;
    for (int c : kRatchetCounts) counts.add (juce::String (c));
    l.add (std::make_unique<C> (id ("ratchetMax"), "Ratchet Max", counts, 4));
    l.add (std::make_unique<C> (id ("polyRatio"), "Polyrhythm", names (kPolyNames), 0));
    l.add (percent ("preservation", "Preservation", 60.0f));
    l.add (percent ("mTiming", "Mutate Timing", 60.0f));
    l.add (percent ("mVelocity", "Mutate Velocity", 60.0f));
    l.add (percent ("mPitch", "Mutate Pitch", 40.0f));
    l.add (percent ("mDensity", "Mutate Density", 50.0f));
    l.add (percent ("mStructure", "Mutate Structure", 40.0f));
    l.add (percent ("mSubdivision", "Mutate Subdivision", 30.0f));
    l.add (percent ("mProbability", "Mutate Probability", 20.0f));
    l.add (std::make_unique<B> (id ("grooveAware"), "Groove Aware", false));
    l.add (percent ("grooveStrength", "Groove Strength", 60.0f));
    l.add (std::make_unique<I> (id ("channel"), "MIDI Channel", 1, 16, 1));
    l.add (std::make_unique<C> (id ("playMode"), "Play in Live", names (kPlayModeNames), 0));
    l.add (std::make_unique<C> (id ("phrase"), "Phrase", names (kPhraseNames), 2));
    l.add (std::make_unique<B> (id ("midiThru"), "MIDI Thru", true));
    l.add (std::make_unique<C> (id ("preview"), "Preview Sound", names (kPreviewNames), 1));
    l.add (std::make_unique<B> (id ("auditionMidi"), "Audition to MIDI Out", false));
    l.add (std::make_unique<B> (id ("auditionLoop"), "Audition Loop", true));
    l.add (std::make_unique<B> (id ("seedLock"), "Seed Lock", false));
    return l;
}

// ---------------------------------------------------------------------------------------------- life
FillMakerProcessor::FillMakerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FillMakerState", layout())
{
    pool_ = defaultPool();
    seed_ = 1;
    regenerate();
    startTimerHz (30);
}

FillMakerProcessor::~FillMakerProcessor() { stopTimer(); }

bool FillMakerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void FillMakerProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    hostPlayer_.prepare (sr_);
    auditionPlayer_.prepare (sr_);
    synth_.prepare (sr_);
    out_.ensureSize (8192);
    auditionOut_.ensureSize (8192);
    scratch_.ensureSize (8192);
    juce::ignoreUnused (maxBlock);
    publish();
}

int FillMakerProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String FillMakerProcessor::getProgramName (int i) { return i >= 0 && i < getNumPrograms() ? factoryPresets()[(size_t) i].name : juce::String(); }
void FillMakerProcessor::setCurrentProgram (int i)
{
    if (i < 0 || i >= getNumPrograms()) return;
    program_ = i;
    applyPreset (factoryPresets()[(size_t) i]);
}

// ---------------------------------------------------------------------------------------------- settings
Settings FillMakerProcessor::settings() const
{
    Settings s;
    s.density = p ("density") * 0.01f;
    s.complexity = p ("complexity") * 0.01f;
    s.behaviour = p ("behaviour") * 0.01f;
    s.lengthChoice = (LengthChoice) juce::roundToInt (p ("length"));
    s.customLength = p ("customLength");
    s.beatsPerBar = beatsPerBar_.load();
    s.resolution = (Resolution) juce::roundToInt (p ("resolution"));
    s.customSubdivision = juce::roundToInt (p ("customSub"));
    s.swing = p ("swing") * 0.01f;
    s.humanise = p ("humanise") * 0.01f;
    s.velocityBase = juce::roundToInt (p ("velBase"));
    s.velocityVariation = p ("velVar") * 0.01f;
    s.accent = p ("accent") * 0.01f;
    s.ghost = p ("ghost") * 0.01f;
    s.curve = (VelocityCurve) juce::roundToInt (p ("curve"));
    s.curveDepth = p ("curveDepth") * 0.01f;
    s.pool = pool_;
    s.pitchLow = juce::roundToInt (p ("pitchLow"));
    s.pitchHigh = juce::roundToInt (p ("pitchHigh"));
    s.motion = (PitchMotion) juce::roundToInt (p ("motion"));
    for (int e = 0; e < kNumEngines; ++e) s.engines[(size_t) e] = apvts.getRawParameterValue (eid (e))->load() > 0.5f;
    for (int g = 0; g < kNumGrammars; ++g) s.grammars[(size_t) g] = apvts.getRawParameterValue (gid (g))->load() > 0.5f;
    s.euclidSteps = juce::roundToInt (p ("euclidSteps"));
    s.euclidPulses = juce::roundToInt (p ("euclidPulses"));
    s.euclidRotation = juce::roundToInt (p ("euclidRotation"));
    s.ratchetMax = kRatchetCounts[(size_t) juce::jlimit (0, 7, juce::roundToInt (p ("ratchetMax")))];
    s.polyRatio = juce::roundToInt (p ("polyRatio"));
    s.grooveAware = p ("grooveAware") > 0.5f;
    s.grooveStrength = p ("grooveStrength") * 0.01f;
    return s;
}

MutationSettings FillMakerProcessor::mutationSettings() const
{
    MutationSettings m;
    m.preservation = p ("preservation") * 0.01f;
    m.timing = p ("mTiming") * 0.01f;
    m.velocity = p ("mVelocity") * 0.01f;
    m.pitch = p ("mPitch") * 0.01f;
    m.density = p ("mDensity") * 0.01f;
    m.structure = p ("mStructure") * 0.01f;
    m.subdivision = p ("mSubdivision") * 0.01f;
    m.probability = p ("mProbability") * 0.01f;
    return m;
}

void FillMakerProcessor::setPool (std::vector<PitchEntry> pool)
{
    if (pool.size() > 32) pool.resize (32);
    pool_ = std::move (pool);
}

// ---------------------------------------------------------------------------------------------- generating
void FillMakerProcessor::setPattern (const Pattern& pt, const juce::String& label, bool push, bool generated)
{
    pattern_ = pt;
    pattern_.sanitise();
    lastAction_ = label;
    if (push) history_.push ({ pattern_, label, structure_, seed_, generated });
    ++version_;
    publish();
}

void FillMakerProcessor::publish() { exchange_.publish (pattern_, version_); }

void FillMakerProcessor::generate()
{
    if (p ("seedLock") < 0.5f)
        seed_ = (uint32_t) seeds_.nextInt64() | 1u;
    generateAs ("Generate");
}

void FillMakerProcessor::regenerate() { generateAs ("Regenerate"); }

void FillMakerProcessor::generateAs (const juce::String& label)
{
    const auto s = settings();
    GenerationReport rep;
    const auto pt = fm::generate (s, seed_, s.grooveAware && groove_.valid() ? &groove_ : nullptr, &rep);
    structure_ = rep.summary();
    setPattern (pt, label, true, true);
}

void FillMakerProcessor::mutate (double a, double b, const std::vector<uint32_t>& ids)
{
    auto m = mutationSettings();
    m.regionStart = a;
    m.regionEnd = b;
    m.onlyIds = ids;
    juce::String what;
    const auto out = fm::mutate (pattern_, settings(), m, (uint32_t) seeds_.nextInt(), &what);
    structure_ = "Mutated: " + what;
    setPattern (out, ids.empty() && b <= a ? "Mutate" : "Mutate selection", true, false);
}

void FillMakerProcessor::commitEdit (const Pattern& pt, const juce::String& label) { setPattern (pt, label, true, false); }

void FillMakerProcessor::previewEdit (const Pattern& pt)
{
    pattern_ = pt;
    ++version_;
    publish();
}

void FillMakerProcessor::undo()
{
    if (auto* e = history_.undo()) { pattern_ = e->pattern; structure_ = e->structure; seed_ = e->seed ? e->seed : seed_; lastAction_ = "Undo"; ++version_; publish(); }
}
void FillMakerProcessor::redo()
{
    if (auto* e = history_.redo()) { pattern_ = e->pattern; structure_ = e->structure; seed_ = e->seed ? e->seed : seed_; lastAction_ = "Redo"; ++version_; publish(); }
}
void FillMakerProcessor::jumpToHistory (int i)
{
    if (auto* e = history_.jump (i)) { pattern_ = e->pattern; structure_ = e->structure; seed_ = e->seed ? e->seed : seed_; lastAction_ = e->label; ++version_; publish(); }
}

void FillMakerProcessor::audition() { auditionStart_.store (true); }
void FillMakerProcessor::stopAudition() { auditionStop_.store (true); }

// ---------------------------------------------------------------------------------------------- groove
juce::String FillMakerProcessor::grooveStatus() const
{
    if (! groove_.valid()) return followInput ? "Listening on MIDI in: play a clip on this track (or import a MIDI file)" : "No groove: import a MIDI file, or Follow MIDI in";
    return (grooveSourceName_.isNotEmpty() ? grooveSourceName_ + ": " : juce::String()) + groove_.describe();
}

bool FillMakerProcessor::importGroove (const juce::File& f, juce::String* error)
{
    auto pt = importMidiFile (f, error);
    if (pt.events.empty()) return false;
    groove_ = analyseGroove (pt);
    grooveSourceName_ = f.getFileNameWithoutExtension();
    followInput = false;
    return true;
}

void FillMakerProcessor::captureGroove()
{
    const double bpb = beatsPerBar_.load();
    if (heard_.empty()) return;
    const double latest = heard_.back().ppq;
    const double barEnd = std::floor (latest / bpb) * bpb;
    const double from = barEnd - 2.0 * bpb;
    Pattern pt;
    pt.beatsPerBar = bpb;
    pt.length = 2.0 * bpb;
    for (const auto& h : heard_)
        if (h.ppq >= from - 1.0e-6 && h.ppq < barEnd - 1.0e-6)
        {
            Event e;
            e.start = h.ppq - from;
            e.pitch = h.pitch;
            e.velocity = h.velocity;
            e.length = 0.1;
            pt.add (e);
        }
    pt.sanitise();
    if (pt.events.size() < 2) return;
    groove_ = analyseGroove (pt);
    grooveSourceName_ = "MIDI in";
    lastGrooveBar_ = barEnd;
}

void FillMakerProcessor::clearGroove()
{
    groove_ = GrooveProfile {};
    grooveSourceName_.clear();
    heard_.clear();
}

void FillMakerProcessor::timerCallback()
{
    // Notes heard on MIDI in (with their song position), for groove-aware mode.
    const auto scope = heardFifo_.read (heardFifo_.getNumReady());
    for (int i = 0; i < scope.blockSize1; ++i) heard_.push_back (heardData_[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) heard_.push_back (heardData_[(size_t) (scope.startIndex2 + i)]);
    if (! heard_.empty())
    {
        // Keep the last 16 bars; a jump back in the song starts the memory again.
        const double bpb = beatsPerBar_.load();
        for (size_t i = 1; i < heard_.size(); ++i)
            if (heard_[i].ppq < heard_[i - 1].ppq - bpb) { heard_.erase (heard_.begin(), heard_.begin() + (long) i); break; }
        const double keepFrom = heard_.back().ppq - 16.0 * bpb;
        heard_.erase (heard_.begin(), std::find_if (heard_.begin(), heard_.end(), [keepFrom] (const Heard& h) { return h.ppq >= keepFrom; }));
        if (followInput)
        {
            const double bar = std::floor (heard_.back().ppq / bpb) * bpb;
            if (bar > lastGrooveBar_ + 1.0e-6) captureGroove();
        }
    }
}

// ---------------------------------------------------------------------------------------------- files
bool FillMakerProcessor::exportMidi (const juce::File& f, int repeats, juce::String* error) const
{
    return writeMidiFile (f, pattern_, bpm_.load(), juce::roundToInt (p ("channel")), repeats, error);
}

juce::File FillMakerProcessor::writeDragFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Fill Maker");
    dir.createDirectory();
    const auto f = dir.getChildFile ("Fill " + juce::String::toHexString ((juce::int64) seed_).toUpperCase() + "-" + juce::String (version_) + ".mid");
    writeMidiFile (f, pattern_, bpm_.load(), juce::roundToInt (p ("channel")), 1);
    return f;
}

void FillMakerProcessor::applyPreset (const Preset& pr)
{
    const auto s = fm::applyPreset (pr, settings());
    const auto set = [this] (const juce::String& id, float v) { if (auto* q = apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    set ("density", s.density * 100.0f);
    set ("complexity", s.complexity * 100.0f);
    set ("behaviour", s.behaviour * 100.0f);
    set ("length", (float) s.lengthChoice);
    set ("customLength", (float) s.customLength);
    set ("resolution", (float) s.resolution);
    set ("customSub", (float) s.customSubdivision);
    set ("swing", s.swing * 100.0f);
    set ("humanise", s.humanise * 100.0f);
    set ("velBase", (float) s.velocityBase);
    set ("velVar", s.velocityVariation * 100.0f);
    set ("accent", s.accent * 100.0f);
    set ("ghost", s.ghost * 100.0f);
    set ("curve", (float) s.curve);
    set ("curveDepth", s.curveDepth * 100.0f);
    set ("motion", (float) s.motion);
    for (int e = 0; e < kNumEngines; ++e) set (eid (e), s.engines[(size_t) e] ? 1.0f : 0.0f);
    for (int g = 0; g < kNumGrammars; ++g) set (gid (g), s.grammars[(size_t) g] ? 1.0f : 0.0f);
    set ("euclidSteps", (float) s.euclidSteps);
    set ("euclidPulses", (float) s.euclidPulses);
    set ("euclidRotation", (float) s.euclidRotation);
    int ri = 0;
    for (int i = 0; i < 8; ++i) if (kRatchetCounts[(size_t) i] <= s.ratchetMax) ri = i;
    set ("ratchetMax", (float) ri);
    set ("polyRatio", (float) s.polyRatio);
    set ("grooveAware", s.grooveAware ? 1.0f : 0.0f);
    set ("grooveStrength", s.grooveStrength * 100.0f);
    set ("preservation", pr.mutation.preservation * 100.0f);
    if (! pr.keepsPool)
    {
        pool_ = s.pool;
        set ("pitchLow", (float) s.pitchLow);
        set ("pitchHigh", (float) s.pitchHigh);
    }
    presetName_ = pr.name;
    if (pr.hasPattern) { structure_ = "Preset pattern: " + pr.name; setPattern (pr.pattern, "Preset " + pr.name, true, false); }
    else
        generateAs ("Preset " + pr.name);
}

bool FillMakerProcessor::saveUserPreset (const juce::String& name, bool withPattern, juce::String* error)
{
    Preset pr;
    pr.name = name.trim().isEmpty() ? juce::String ("User preset") : name.trim();
    pr.category = "User";
    pr.settings = settings();
    pr.mutation = mutationSettings();
    pr.hasPattern = withPattern;
    pr.pattern = pattern_;
    pr.keepsPool = false;
    const auto f = userPresetFolder().getChildFile (juce::File::createLegalFileName (pr.name) + ".fillpreset");
    if (! fm::saveUserPreset (f, pr, error)) return false;
    presetName_ = pr.name;
    return true;
}

bool FillMakerProcessor::loadUserPreset (const juce::File& f, juce::String* error)
{
    Preset pr;
    if (! fm::loadUserPreset (f, pr, error)) return false;
    applyPreset (pr);
    return true;
}

bool FillMakerProcessor::savePattern (const juce::File& f, juce::String* error) const { return savePatternFile (f, pattern_, error); }

bool FillMakerProcessor::loadPattern (const juce::File& f, juce::String* error)
{
    Pattern pt;
    if (! loadPatternFile (f, pt, error)) return false;
    structure_ = "Loaded " + f.getFileNameWithoutExtension();
    setPattern (pt, "Load pattern", true, false);
    return true;
}

// ---------------------------------------------------------------------------------------------- audio
void FillMakerProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = audio.getNumSamples();
    audio.clear();
    double bpm = bpm_.load(), ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b && *b > 1.0) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q; else playing = false;
            if (auto ts = pos->getTimeSignature()) beatsPerBar_.store (std::clamp (ts->numerator * 4.0 / std::max (1, ts->denominator), 0.5, 32.0));
        }
    bpm_.store (bpm);
    const int channel = juce::roundToInt (p ("channel"));
    const int mode = juce::roundToInt (p ("playMode"));
    const int preview = juce::roundToInt (p ("preview"));
    const bool thru = p ("midiThru") > 0.5f;

    // MIDI in: note it (for the groove), and pass it on (thru).
    out_.clear();
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn() && playing)
        {
            const auto w = heardFifo_.write (1);
            const Heard h { ppq + m.samplePosition * bpm / 60.0 / sr_, (uint8_t) msg.getNoteNumber(), (uint8_t) msg.getVelocity() };
            if (w.blockSize1 > 0) heardData_[(size_t) w.startIndex1] = h;
            else if (w.blockSize2 > 0) heardData_[(size_t) w.startIndex2] = h;
        }
        if (thru) out_.addEvent (msg, m.samplePosition);
    }

    const auto& snap = exchange_.read();
    // A change of channel or play mode ends what's sounding first (on the channel it started on).
    if (channel != lastChannel_ || mode != lastMode_) { hostPlayer_.stop (out_, 0); lastChannel_ = channel; lastMode_ = mode; }
    hostPlayer_.setChannel (channel);
    auditionPlayer_.setChannel (channel);

    // With Live: loop the pattern, or play it at the end of every phrase.
    auditionOut_.clear();
    if (mode > 0 && playing)
    {
        Player::Placement place;
        if (mode == 2) place.phraseBeats = kPhraseBars[(size_t) juce::jlimit (0, 4, juce::roundToInt (p ("phrase")))] * beatsPerBar_.load();
        hostPlayer_.render (snap, ppq, bpm, n, out_, place, playRng_);
        if (preview == 2)
            for (const auto m : out_) if (m.getMessage().isNoteOnOrOff()) auditionOut_.addEvent (m.getMessage(), m.samplePosition);
    }
    else if (hostPlayer_.running() || hostPlayer_.anyActive())
        hostPlayer_.stop (out_, 0);   // transport stopped (or Off): everything it started ends now

    // Audition: its own clock at Live's tempo, whether Live is playing or not.
    const bool toMidi = p ("auditionMidi") > 0.5f;
    if (auditionStop_.exchange (false))
    {
        scratch_.clear();
        auditionPlayer_.stop (scratch_, 0);
        if (toMidi) for (const auto m : scratch_) out_.addEvent (m.getMessage(), 0);
        synth_.allOff();
        auditioning_.store (false);
    }
    if (auditionStart_.exchange (false))
    {
        scratch_.clear();
        auditionPlayer_.stop (scratch_, 0);
        if (toMidi) for (const auto m : scratch_) out_.addEvent (m.getMessage(), 0);
        auditionBeat_ = 0.0;
        auditioning_.store (true);
    }
    double head = mode > 0 && playing ? hostPlayer_.patternPosition() : -1.0;
    if (auditioning_.load())
    {
        Player::Placement place;
        place.once = p ("auditionLoop") < 0.5f;
        scratch_.clear();
        auditionPlayer_.render (snap, auditionBeat_, bpm, n, scratch_, place, playRng_);
        auditionBeat_ += n * bpm / 60.0 / sr_;
        head = auditionPlayer_.patternPosition();
        if (auditionPlayer_.finished() && ! auditionPlayer_.anyActive())
        {
            auditionPlayer_.stop (scratch_, n - 1);
            auditioning_.store (false);
        }
        for (const auto m : scratch_)
        {
            auditionOut_.addEvent (m.getMessage(), m.samplePosition);
            if (toMidi) out_.addEvent (m.getMessage(), m.samplePosition);
        }
    }
    playhead_.store (head);
    if (preview > 0) synth_.render (audio, 0, n, auditionOut_);
    midi.swapWith (out_);
}

// ---------------------------------------------------------------------------------------------- state
void FillMakerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("stateVersion", kStateVersion, nullptr);
    st.removeChild (st.getChildWithName ("FillMaker"), nullptr);
    juce::ValueTree fmTree ("FillMaker");
    fmTree.setProperty ("pattern", pattern_.toJson(), nullptr);
    fmTree.setProperty ("history", juce::JSON::toString (history_.toVar(), true), nullptr);
    Settings s;
    s.pool = pool_;
    fmTree.setProperty ("pool", juce::JSON::toString (s.toVar().getProperty ("pool", {}), true), nullptr);
    fmTree.setProperty ("seed", (juce::int64) seed_, nullptr);
    fmTree.setProperty ("structure", structure_, nullptr);
    fmTree.setProperty ("preset", presetName_, nullptr);
    fmTree.setProperty ("followInput", followInput, nullptr);
    if (groove_.valid())
    {
        fmTree.setProperty ("groove", groove_.source.toJson(), nullptr);
        fmTree.setProperty ("grooveName", grooveSourceName_, nullptr);
    }
    st.appendChild (fmTree, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void FillMakerProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    const auto fmTree = st.getChildWithName ("FillMaker");
    st.removeChild (fmTree, nullptr);
    apvts.replaceState (st);
    // Every parameter set from the state explicitly: replaceState skips a switch whose stored value
    // rounds to its current one (a host may have left it at 0.49, which reads as off).
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (const auto child = st.getChildWithProperty ("id", rp->paramID); child.isValid() && child.hasProperty ("value"))
                rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
    if (! fmTree.isValid()) return;
    auto poolVar = juce::JSON::parse (fmTree.getProperty ("pool", "[]").toString());
    auto* o = new juce::DynamicObject();
    o->setProperty ("pool", poolVar);
    pool_ = Settings::fromVar (juce::var (o)).pool;
    seed_ = (uint32_t) (juce::int64) fmTree.getProperty ("seed", 1);
    structure_ = fmTree.getProperty ("structure", "").toString();
    presetName_ = fmTree.getProperty ("preset", "Init").toString();
    followInput = (bool) fmTree.getProperty ("followInput", true);
    history_.fromVar (juce::JSON::parse (fmTree.getProperty ("history", "{}").toString()));
    if (fmTree.hasProperty ("groove"))
    {
        groove_ = analyseGroove (Pattern::fromJson (fmTree.getProperty ("groove").toString()));
        grooveSourceName_ = fmTree.getProperty ("grooveName", "").toString();
    }
    pattern_ = Pattern::fromJson (fmTree.getProperty ("pattern", "{}").toString());
    lastAction_ = "Session restored";
    ++version_;
    publish();
}

juce::AudioProcessorEditor* FillMakerProcessor::createEditor() { return new FillMakerEditor (*this); }
} // namespace fm

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new fm::FillMakerProcessor(); }
