// fillmaker-cli: the generator from the command line.
//   fillmaker-cli generate [preset="Ratchet Storm"] [seed=42] [density=0.7] [complexity=0.5] [behaviour=0.8]
//                 [length=0..5] [beats=6] [resolution=0..7] [engines=prob,euclid,ratchet,perm,poly]
//                 [grammars=build,burst,...] [mutate=0.4] [bpm=120] [out=fill.mid] [repeats=1]
//   fillmaker-cli presets                      list the factory presets
//   fillmaker-cli snapshot ui.png [tab=0..3] [preset=...]   a picture of the editor
#include "engine/Generator.h"
#include "engine/MidiExport.h"
#include "engine/Mutation.h"
#include "engine/Presets.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <iostream>
#include <set>

using namespace fm;

namespace
{
std::string roll (const Pattern& p)
{
    std::set<int> pitches;
    for (const auto& e : p.events) pitches.insert (e.pitch);
    const int cols = std::max (16, (int) std::lround (p.length * 16));
    std::string out;
    for (auto it = pitches.rbegin(); it != pitches.rend(); ++it)
    {
        std::string row ((size_t) cols, '.');
        for (int c = 0; c < cols; c += 16) row[(size_t) c] = '|';
        for (const auto& e : p.events)
            if (e.pitch == *it)
                row[(size_t) std::clamp ((int) (e.time() / p.length * cols), 0, cols - 1)] = e.velocity > 100 ? '#' : e.velocity > 60 ? 'o' : '-';
        out += juce::String (*it).paddedLeft (' ', 4).toStdString() + " " + row + "\n";
    }
    return out;
}

const Preset* findPreset (const juce::String& name)
{
    for (const auto& p : factoryPresets()) if (p.name.equalsIgnoreCase (name)) return &p;
    return nullptr;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringPairArray kv;
    juce::String cmd = argc > 1 ? juce::String (argv[1]) : juce::String ("generate"), file;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a.containsChar ('=')) kv.set (a.upToFirstOccurrenceOf ("=", false, false), a.fromFirstOccurrenceOf ("=", false, false));
        else file = a;
    }
    if (cmd == "presets")
    {
        for (const auto& p : factoryPresets()) std::cout << p.category << " / " << p.name << "\n";
        return 0;
    }
    if (cmd == "snapshot")
    {
        FillMakerProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        if (kv.containsKey ("preset")) if (auto* p = findPreset (kv["preset"])) proc.applyPreset (*p);
        if (kv.containsKey ("seed")) { proc.setSeed ((uint32_t) kv["seed"].getLargeIntValue()); proc.regenerate(); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        if (auto* fe = dynamic_cast<FillMakerEditor*> (ed.get())) fe->variationPanel().showTab (kv.getValue ("tab", "0").getIntValue());
        for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, (float) kv.getValue ("scale", "1").getDoubleValue());
        juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (file.isEmpty() ? "ui.png" : file);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "wrote " << f.getFullPathName() << "\n";
        return 0;
    }

    Settings s;
    s.pool = defaultPool();
    for (auto& e : s.pool) e.enabled = true;
    if (kv.containsKey ("preset"))
    {
        if (auto* p = findPreset (kv["preset"])) s = applyPreset (*p, s);
        else { std::cerr << "no preset " << kv["preset"] << "\n"; return 1; }
    }
    const auto f01 = [&] (const char* k, float& v) { if (kv.containsKey (k)) v = (float) std::clamp (kv[k].getDoubleValue(), 0.0, 1.0); };
    f01 ("density", s.density);
    f01 ("complexity", s.complexity);
    f01 ("behaviour", s.behaviour);
    f01 ("swing", s.swing);
    if (kv.containsKey ("length")) s.lengthChoice = (LengthChoice) std::clamp (kv["length"].getIntValue(), 0, 5);
    if (kv.containsKey ("beats")) { s.lengthChoice = LengthChoice::custom; s.customLength = kv["beats"].getDoubleValue(); }
    if (kv.containsKey ("resolution")) s.resolution = (Resolution) std::clamp (kv["resolution"].getIntValue(), 0, 7);
    if (kv.containsKey ("engines"))
    {
        s.engines = {};
        const juce::StringArray keys { "prob", "euclid", "ratchet", "perm", "poly" };
        for (const auto& t : juce::StringArray::fromTokens (kv["engines"], ",", "")) if (keys.indexOf (t) >= 0) s.engines[(size_t) keys.indexOf (t)] = true;
    }
    if (kv.containsKey ("grammars"))
    {
        s.grammars = {};
        const juce::StringArray keys { "build", "release", "burst", "call", "accel", "decel", "interrupt", "escalate", "displace", "fragment" };
        for (const auto& t : juce::StringArray::fromTokens (kv["grammars"], ",", "")) if (keys.indexOf (t) >= 0) s.grammars[(size_t) keys.indexOf (t)] = true;
    }
    const auto seed = (uint32_t) kv.getValue ("seed", "1").getLargeIntValue();
    GenerationReport rep;
    auto p = generate (s, seed, nullptr, &rep);
    if (kv.containsKey ("mutate"))
    {
        MutationSettings m;
        m.preservation = (float) std::clamp (kv["mutate"].getDoubleValue(), 0.0, 1.0);
        juce::String what;
        p = mutate (p, s, m, seed * 7 + 1, &what);
        std::cout << "mutated: " << what << "\n";
    }
    std::cout << p.events.size() << " notes, " << p.length << " beats, seed " << seed << "\n" << rep.summary() << "\n" << roll (p);
    if (kv.containsKey ("out"))
    {
        const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (kv["out"]);
        juce::String err;
        if (! writeMidiFile (out, p, kv.getValue ("bpm", "120").getDoubleValue(), 10, kv.getValue ("repeats", "1").getIntValue(), &err)) { std::cerr << err << "\n"; return 1; }
        std::cout << "wrote " << out.getFullPathName() << "\n";
    }
    return 0;
}
