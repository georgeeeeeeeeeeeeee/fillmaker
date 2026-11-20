#include "engine/MidiExport.h"

#include "engine/Random.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace fm
{
juce::MidiFile toMidiFile (const Pattern& p, double bpm, int channel, int repeats, uint32_t probSeed)
{
    channel = std::clamp (channel, 1, 16);
    repeats = std::clamp (repeats, 1, 64);
    bpm = std::clamp (std::isfinite (bpm) ? bpm : 120.0, 20.0, 999.0);
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Fill Maker"), 0.0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);
    const int num = std::clamp ((int) std::lround (p.beatsPerBar), 1, 32);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (num, 4), 0.0);
    Rng rng (probSeed, 0xe7);
    const auto tick = [] (double beats) { return std::round (beats * kTicksPerBeat); };

    struct N { double on, off; int pitch, vel; };
    std::vector<N> notes;
    for (int r = 0; r < repeats; ++r)
        for (const auto& e : p.events)
        {
            if (e.probability < 1.0f && ! rng.chance (e.probability)) continue;
            const double on = tick (r * p.length + e.time());
            notes.push_back ({ on, std::max (on + 1.0, tick (r * p.length + e.end())), e.pitch, e.velocity });
        }
    std::sort (notes.begin(), notes.end(), [] (const N& a, const N& b) { return a.on < b.on; });
    // Same pitch overlapping the next hit on it: end it a tick before.
    std::map<int, size_t> last;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        if (auto it = last.find (notes[i].pitch); it != last.end() && notes[it->second].off > notes[i].on - 1.0)
            notes[it->second].off = std::max (notes[it->second].on + 1.0, notes[i].on - 1.0);
        last[notes[i].pitch] = i;
    }
    for (const auto& n : notes)
    {
        seq.addEvent (juce::MidiMessage::noteOn (channel, n.pitch, (juce::uint8) std::clamp (n.vel, 1, 127)), n.on);
        seq.addEvent (juce::MidiMessage::noteOff (channel, n.pitch), n.off);
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), tick (repeats * p.length));
    seq.updateMatchedPairs();
    juce::MidiFile f;
    f.setTicksPerQuarterNote (kTicksPerBeat);
    f.addTrack (seq);
    return f;
}

bool writeMidiFile (const juce::File& file, const Pattern& p, double bpm, int channel, int repeats, juce::String* error)
{
    const auto mf = toMidiFile (p, bpm, channel, repeats);
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (! os.openedOk() || ! mf.writeTo (os, 1))
    {
        if (error != nullptr) *error = "Couldn't write " + file.getFullPathName();
        return false;
    }
    return true;
}

Pattern patternFromMidiFile (const juce::MidiFile& src, double* bpm)
{
    juce::MidiFile mf = src;
    const int tpq = mf.getTimeFormat() > 0 ? mf.getTimeFormat() : 960;
    Pattern p;
    p.beatsPerBar = 4.0;
    double maxEnd = 0.0;
    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*mf.getTrack (t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto* ev = seq.getEventPointer (i);
            const auto& m = ev->message;
            if (m.isTempoMetaEvent() && bpm != nullptr && m.getTempoSecondsPerQuarterNote() > 0.0) *bpm = 60.0 / m.getTempoSecondsPerQuarterNote();
            if (m.isTimeSignatureMetaEvent())
            {
                int n = 4, d = 4;
                m.getTimeSignatureInfo (n, d);
                p.beatsPerBar = n * 4.0 / std::max (1, d);
            }
            if (! m.isNoteOn()) continue;
            Event e;
            e.start = m.getTimeStamp() / tpq;
            const double off = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() / tpq : e.start + 0.125;
            e.length = std::max (Pattern::kMinLength, off - e.start);
            e.pitch = m.getNoteNumber();
            e.velocity = m.getVelocity();
            p.events.push_back (e);
            maxEnd = std::max (maxEnd, e.start + 1.0e-6);
        }
    }
    // Starting at the first bar that has notes, rounded up to whole bars.
    if (! p.events.empty())
    {
        double first = 1.0e9;
        for (const auto& e : p.events) first = std::min (first, e.start);
        const double shift = std::floor (first / p.beatsPerBar) * p.beatsPerBar;
        for (auto& e : p.events) e.start -= shift;
        maxEnd -= shift;
    }
    p.length = std::clamp (std::ceil (std::max (maxEnd, 0.25) / p.beatsPerBar) * p.beatsPerBar, p.beatsPerBar, 64.0);
    p.sanitise();
    return p;
}
} // namespace fm
