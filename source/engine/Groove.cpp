#include "engine/Groove.h"

#include "engine/MidiExport.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace fm
{
GrooveProfile analyseGroove (const Pattern& src)
{
    GrooveProfile g;
    g.beatsPerBar = src.beatsPerBar > 0.0 ? src.beatsPerBar : 4.0;
    g.source = src;
    const int slots = std::max (1, (int) std::lround (g.beatsPerBar * GrooveProfile::kSlotsPerBeat));
    g.onset.assign ((size_t) slots, 0.0);
    g.velocity.assign ((size_t) slots, 0.0);
    g.events = (int) src.events.size();
    g.bars = std::max (1.0, src.length / g.beatsPerBar);
    if (src.events.empty()) return g;

    std::vector<int> count ((size_t) slots, 0);
    std::map<int, int> pitches;
    int ternary = 0, fine = 0;
    double vsum = 0.0;
    // Per bar: which slots are hit (for repetition).
    const int nBars = std::max (1, (int) std::ceil (src.length / g.beatsPerBar - 1.0e-9));
    std::vector<std::vector<bool>> barHits ((size_t) nBars, std::vector<bool> ((size_t) slots, false));
    for (const auto& e : src.events)
    {
        const double pos = std::fmod (e.time(), g.beatsPerBar);
        const int slot = std::clamp ((int) std::lround (pos * GrooveProfile::kSlotsPerBeat), 0, slots) % slots;
        count[(size_t) slot]++;
        g.velocity[(size_t) slot] += e.velocity;
        pitches[e.pitch]++;
        vsum += e.velocity;
        const double inBeat = std::fmod (pos, 1.0) * 12.0;   // 12ths of a beat
        const int k = (int) std::lround (inBeat);
        const bool onGrid = std::abs (inBeat - k) < 0.35;
        if (onGrid && k % 3 != 0 && k % 2 == 0) ++ternary;   // a triplet position (8th or 16th triplet) that isn't a 16th
        if (! onGrid) ++fine;
        const int b = std::clamp ((int) std::floor (e.time() / g.beatsPerBar), 0, nBars - 1);
        barHits[(size_t) b][(size_t) slot] = true;
    }
    int top = 1;
    for (int c : count) top = std::max (top, c);
    for (int i = 0; i < slots; ++i)
    {
        g.onset[(size_t) i] = (double) count[(size_t) i] / top;
        if (count[(size_t) i] > 0) g.velocity[(size_t) i] /= count[(size_t) i];
    }
    g.density = (double) g.events / std::max (0.25, src.length);
    g.ternary = (double) ternary / g.events;
    g.fine = (double) fine / g.events;
    g.meanVelocity = vsum / g.events;
    for (const auto& [p, n] : pitches) g.pitches.push_back ({ p, (double) n / g.events });
    std::sort (g.pitches.begin(), g.pitches.end(), [] (const auto& a, const auto& b) { return a.second > b.second; });
    // Repetition: how many of each bar's hits the next bar shares.
    if (nBars > 1)
    {
        double sim = 0.0;
        for (int b = 1; b < nBars; ++b)
        {
            int both = 0, either = 0;
            for (int i = 0; i < slots; ++i)
            {
                both += barHits[(size_t) b][(size_t) i] && barHits[(size_t) b - 1][(size_t) i];
                either += barHits[(size_t) b][(size_t) i] || barHits[(size_t) b - 1][(size_t) i];
            }
            sim += either > 0 ? (double) both / either : 1.0;
        }
        g.repetition = sim / (nBars - 1);
    }
    else
        g.repetition = 1.0;
    return g;
}

double GrooveProfile::onsetAt (double beat) const noexcept
{
    if (onset.empty()) return 0.0;
    double pos = std::fmod (beat, beatsPerBar);
    if (pos < 0.0) pos += beatsPerBar;
    return onset[(size_t) (std::clamp ((int) std::lround (pos * kSlotsPerBeat), 0, slots()) % slots())];
}

double GrooveProfile::emptiness (double beat) const noexcept
{
    // 1 in the groove's silences (no hit within a 16th either side), fading near its hits.
    if (onset.empty()) return 1.0;
    double pos = std::fmod (beat, beatsPerBar);
    if (pos < 0.0) pos += beatsPerBar;
    const int c = (int) std::lround (pos * kSlotsPerBeat);
    double near = 0.0;
    for (int d = -3; d <= 3; ++d)
        near = std::max (near, onset[(size_t) (((c + d) % slots() + slots()) % slots())] * (1.0 - std::abs (d) / 4.0));
    return 1.0 - near;
}

juce::String GrooveProfile::describe() const
{
    if (! valid()) return "No groove yet";
    juce::String s;
    s << events << " hits over " << juce::String (bars, bars < 2 ? 1 : 0) << (bars == 1.0 ? " bar" : " bars")
      << ", " << juce::String (density, 1) << "/beat";
    if (ternary > 0.3) s << ", shuffled";
    if (repetition > 0.8) s << ", repeating";
    if (! pitches.empty())
    {
        juce::StringArray top;
        for (size_t i = 0; i < std::min<size_t> (3, pitches.size()); ++i) top.add (juce::String (pitches[i].first));
        s << ", pitches " << top.joinIntoString (" ");
    }
    return s;
}

Pattern importMidiFile (const juce::File& f, juce::String* error, double* bpm)
{
    juce::FileInputStream in (f);
    juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in))
    {
        if (error != nullptr) *error = "Can't read " + f.getFileName() + " as a MIDI file";
        return {};
    }
    auto p = patternFromMidiFile (mf, bpm);
    if (p.events.empty() && error != nullptr) *error = f.getFileName() + " has no notes";
    return p;
}
} // namespace fm
