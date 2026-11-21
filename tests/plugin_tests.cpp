// The plug-in: generate / mutate / history, playing in time with a host, audition, MIDI thru and
// groove capture, state, and the editor.
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <map>

using namespace fm;

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;

struct Head final : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

void set (FillMakerProcessor& p, const char* id, float v) { auto* q = p.state().getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }

std::unique_ptr<FillMakerProcessor> make()
{
    auto p = std::make_unique<FillMakerProcessor>();
    p->setPlayConfigDetails (0, 2, kSr, kBlock);
    p->prepareToPlay (kSr, kBlock);
    return p;
}

struct Out { std::vector<std::pair<long long, juce::MidiMessage>> midi; float peak = 0.0f; };

// Runs blocks; the head (if any) moves with them. `in` events are fed at their absolute sample.
Out run (FillMakerProcessor& p, Head* head, double seconds, const std::vector<std::pair<long long, juce::MidiMessage>>& in = {})
{
    Out o;
    p.setPlayHead (head);
    juce::AudioBuffer<float> audio (2, kBlock);
    const long long total = (long long) (seconds * kSr);
    for (long long at = 0; at < total; at += kBlock)
    {
        juce::MidiBuffer mb;
        for (const auto& [s, m] : in) if (s >= at && s < at + kBlock) mb.addEvent (m, (int) (s - at));
        p.processBlock (audio, mb);
        for (const auto m : mb) o.midi.push_back ({ at + m.samplePosition, m.getMessage() });
        o.peak = std::max (o.peak, audio.getMagnitude (0, 0, kBlock));
        if (head != nullptr && head->playing) head->ppq += kBlock * head->bpm / 60.0 / kSr;
    }
    p.setPlayHead (nullptr);
    return o;
}

int ons (const Out& o) { int n = 0; for (const auto& [s, m] : o.midi) n += m.isNoteOn(); return n; }
} // namespace

TEST_CASE ("plug-in: generate, regenerate, seed lock, mutate, undo and redo")
{
    auto p = make();
    REQUIRE (! p->pattern().events.empty());   // a fill is there from the start
    const auto first = p->pattern();
    p->generate();
    const auto second = p->pattern();
    CHECK (! (first == second));
    // Regenerate: the same seed, so the same fill until a setting moves.
    p->regenerate();
    CHECK (p->pattern().events == second.events);
    set (*p, "density", 95.0f);
    p->regenerate();
    CHECK (p->pattern().events.size() > second.events.size());
    // Seed lock: Generate repeats.
    set (*p, "seedLock", 1.0f);
    p->generate();
    const auto locked = p->pattern();
    p->generate();
    CHECK (p->pattern().events == locked.events);
    set (*p, "seedLock", 0.0f);
    p->mutate();
    CHECK (p->history().current()->label == "Mutate");
    const int n = p->history().size();
    p->undo();
    CHECK (p->pattern().events == locked.events);
    p->redo();
    CHECK (p->history().cursor() == n - 1);
    p->jumpToHistory (0);
    CHECK (p->pattern().events == first.events);
}

TEST_CASE ("plug-in: in Live (Loop), the fill plays on its own channel, in time, each pass the same")
{
    auto p = make();
    set (*p, "playMode", 1.0f);   // Loop
    set (*p, "channel", 10.0f);
    set (*p, "preview", 0.0f);
    set (*p, "midiThru", 0.0f);
    set (*p, "length", (float) LengthChoice::oneBar);
    p->regenerate();
    auto certain = p->pattern();
    for (auto& e : certain.events) e.probability = 1.0f;   // deterministic passes
    p->commitEdit (certain, "prob 1");
    const int perPass = (int) p->pattern().events.size();
    Head h;
    const auto o = run (*p, &h, 6.0);   // 3 bars at 120 (the last block runs a little past)
    int inThree = 0;
    for (const auto& [s, m] : o.midi) inThree += m.isNoteOn() && s < (long long) (kSr * 6.0);
    CHECK (inThree == perPass * 3);
    for (const auto& [s, m] : o.midi) CHECK (m.getChannel() == 10);
    // The first note of each pass lands on the bar line plus its offset, the same each time.
    std::vector<long long> firsts;
    for (const auto& [s, m] : o.midi) if (m.isNoteOn()) { firsts.push_back (s); break; }
    const long long bar = (long long) (kSr * 2.0);
    long long second = -1;
    for (const auto& [s, m] : o.midi) if (m.isNoteOn() && s >= bar) { second = s; break; }
    CHECK (second - firsts[0] == bar);
}

TEST_CASE ("plug-in: stopping Live ends every note; nothing plays while stopped; Off plays nothing")
{
    auto p = make();
    set (*p, "playMode", 1.0f);
    set (*p, "preview", 0.0f);
    // A long note, so one is sounding when the transport stops.
    Pattern pt;
    pt.length = 4.0;
    Event e;
    e.start = 0.0;
    e.length = 3.5;
    e.pitch = 40;
    pt.add (e);
    p->commitEdit (pt, "long");
    Head h;
    auto o = run (*p, &h, 0.5);
    CHECK (ons (o) == 1);
    h.playing = false;
    o = run (*p, &h, 0.5);
    int offs = 0;
    for (const auto& [s, m] : o.midi) offs += m.isNoteOff();
    CHECK (offs == 1);
    CHECK (ons (o) == 0);
    h.playing = true;
    set (*p, "playMode", 0.0f);
    CHECK (ons (run (*p, &h, 2.0)) == 0);
}

TEST_CASE ("plug-in: phrase end mode plays the fill only in the last bar of each phrase")
{
    auto p = make();
    set (*p, "playMode", 2.0f);
    set (*p, "phrase", 2.0f);   // every 4 bars
    set (*p, "preview", 0.0f);
    set (*p, "midiThru", 0.0f);
    Head h;
    const auto o = run (*p, &h, 16.0);   // 8 bars
    for (const auto& [s, m] : o.midi)
        if (m.isNoteOn()) { const double beat = s / 24000.0; CHECK (std::fmod (beat, 16.0) >= 12.0 - 1.0e-6); }
    CHECK (ons (o) > 0);
}

TEST_CASE ("plug-in: audition plays through the preview voice without Live, and to MIDI only when asked")
{
    auto p = make();
    set (*p, "preview", 1.0f);
    set (*p, "auditionMidi", 0.0f);
    set (*p, "auditionLoop", 0.0f);
    p->audition();
    auto o = run (*p, nullptr, 2.5);
    CHECK (o.peak > 0.01f);
    CHECK (ons (o) == 0);
    CHECK (! p->auditioning());   // one pass, then it stops by itself
    set (*p, "auditionMidi", 1.0f);
    set (*p, "auditionLoop", 1.0f);
    p->audition();
    o = run (*p, nullptr, 1.0);
    CHECK (ons (o) > 0);
    CHECK (p->auditioning());
    p->stopAudition();
    o = run (*p, nullptr, 0.1);
    CHECK (! p->auditioning());
}

TEST_CASE ("plug-in: MIDI thru passes the groove on; groove-aware mode hears it and captures whole bars")
{
    auto p = make();
    set (*p, "midiThru", 1.0f);
    set (*p, "grooveAware", 1.0f);
    set (*p, "preview", 0.0f);
    std::vector<std::pair<long long, juce::MidiMessage>> in;
    // 3 bars at 120 BPM: kick on beats, hats on 8ths (12000 samples).
    for (int i = 0; i < 24; ++i)
    {
        in.push_back ({ i * 12000LL, juce::MidiMessage::noteOn (1, 42, (juce::uint8) 70) });
        in.push_back ({ i * 12000LL + 2000, juce::MidiMessage::noteOff (1, 42) });
        if (i % 2 == 0) in.push_back ({ i * 12000LL, juce::MidiMessage::noteOn (1, 36, (juce::uint8) 110) });
    }
    Head h;
    const auto o = run (*p, &h, 6.1, in);
    int thruOns = 0;
    for (const auto& [s, m] : o.midi) thruOns += m.isNoteOn() && m.getChannel() == 1;
    CHECK (thruOns == 36);
    for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    REQUIRE (p->groove().valid());
    CHECK (p->groove().events == 24);   // two whole bars: 16 hats + 8 kicks
    CHECK (p->groove().pitches[0].first == 42);
    p->regenerate();
    CHECK (! p->pattern().events.empty());
}

TEST_CASE ("plug-in: the session comes back: pattern, pool, seed, history and settings")
{
    auto a = make();
    set (*a, "complexity", 77.0f);
    set (*a, FillMakerProcessor::gid (kBurst).toRawUTF8(), 1.0f);
    auto pool = a->pool();
    pool[0].weight = 0.123f;
    a->setPool (pool);
    a->generate();
    a->mutate();
    juce::MemoryBlock mb;
    a->getStateInformation (mb);
    auto b = make();
    b->setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (b->pattern().events.size() == a->pattern().events.size());
    CHECK (b->pattern().toJson() == Pattern::fromJson (a->pattern().toJson()).toJson());
    CHECK (b->seed() == a->seed());
    CHECK (std::abs (b->pool()[0].weight - 0.123f) < 1e-6f);
    CHECK (b->history().size() == a->history().size());
    CHECK (std::abs (b->state().getRawParameterValue ("complexity")->load() - 77.0f) < 0.01f);
    CHECK (b->settings().grammars[kBurst]);
    // Garbage state doesn't break anything.
    const char junk[] = "nonsense";
    b->setStateInformation (junk, sizeof (junk));
    CHECK (! b->pattern().events.empty());
}

TEST_CASE ("plug-in: factory programs apply and keep the pitch pool")
{
    auto p = make();
    auto pool = p->pool();
    pool.resize (4);
    p->setPool (pool);
    for (int i = 0; i < p->getNumPrograms(); ++i)
    {
        p->setCurrentProgram (i);
        CHECK (p->pool().size() == 4);
        CHECK (p->presetName() == p->getProgramName (i));
        for (const auto& e : p->pattern().events) CHECK ((e.pitch >= 36 && e.pitch <= 39));
    }
}

TEST_CASE ("plug-in: user presets and pattern files round trip")
{
    auto p = make();
    set (*p, "behaviour", 88.0f);
    p->generate();
    const auto tmp = juce::File::createTempFile (".fillpattern");
    REQUIRE (p->savePattern (tmp));
    const auto saved = p->pattern();
    p->generate();
    REQUIRE (p->loadPattern (tmp));
    CHECK (p->pattern().events.size() == saved.events.size());
    tmp.deleteFile();
    const auto mid = juce::File::createTempFile (".mid");
    REQUIRE (p->exportMidi (mid, 2));
    CHECK (mid.getSize() > 30);
    mid.deleteFile();
    CHECK (p->writeDragFile().existsAsFile());
}

TEST_CASE ("plug-in: a mapped control lights up and its tab opens; the window's own moves don't")
{
    auto p = make();
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    auto& fe = dynamic_cast<FillMakerEditor&> (*ed);
    auto& sp = fe.getSpotlight();
    fe.variationPanel().showTab (0);
    const auto hostSet = [&p] (const char* id, float v) {   // as Live's Key Map / MIDI Map does it
        auto* q = p->state().getParameter (id);
        REQUIRE (q != nullptr);
        q->setValue (v);
        q->sendValueChangedMessageToListeners (v);
    };
    sp.poll();
    CHECK (sp.litCount() == 0);
    hostSet ("euclidRotation", 0.5f);   // on the More tab
    sp.poll();
    CHECK (sp.newest() == "euclidRotation");
    CHECK (fe.variationPanel().currentTab() == 3);   // the More tab came up
    auto* q = p->state().getParameter ("behaviour");
    q->beginChangeGesture();
    q->setValueNotifyingHost (0.9f);
    q->endChangeGesture();
    sp.poll();
    CHECK (sp.newest() == "euclidRotation");
}

TEST_CASE ("plug-in: the editor opens, draws every tab, and survives edits")
{
    auto p = make();
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    REQUIRE (ed != nullptr);
    auto* fe = dynamic_cast<FillMakerEditor*> (ed.get());
    REQUIRE (fe != nullptr);
    for (int t = 0; t < 4; ++t)
    {
        fe->variationPanel().showTab (t);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        CHECK (img.isValid());
    }
    // An edit through the processor shows up in the roll's next refresh.
    auto pt = p->pattern();
    pt.events.clear();
    p->commitEdit (pt, "Clear");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    CHECK (p->pattern().events.empty());
    p->undo();
    CHECK (! p->pattern().events.empty());
}
