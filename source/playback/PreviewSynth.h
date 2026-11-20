#pragma once
// The audition voice: a small percussion synth so a fill can be heard without routing anything. It is
// separate from the MIDI output and can be switched off; the generated MIDI never depends on it.
//
// Pitch picks the sound along one continuous range, without assuming any drum map: low pitches are
// pitched thumps with a downward sweep, the middle tuned toms, the top noise (snare, then hats).
// Velocity sets level and brightness; a note-off releases the voice, so note lengths are heard.

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace fm
{
class PreviewSynth
{
public:
    void prepare (double sampleRate) noexcept;
    void render (juce::AudioBuffer<float>&, int startSample, int numSamples, const juce::MidiBuffer& events) noexcept;
    void allOff() noexcept;   // immediate silence

private:
    struct Voice
    {
        bool active = false, released = false;
        int pitch = 0;
        uint32_t age = 0;
        double phase = 0.0, freq = 100.0, sweep = 0.0, sweepK = 0.0;
        double env = 0.0, decayK = 0.0, releaseK = 0.0, level = 0.0, noiseMix = 0.0, toneLevel = 1.0;
        double hp = 0.0, hpX = 0.0, lp = 0.0, hpK = 0.0, lpK = 0.0;
    };
    void start (int pitch, int velocity) noexcept;
    void release (int pitch) noexcept;
    float tick (Voice&) noexcept;
    std::array<Voice, 24> voices_ {};
    double sr_ = 48000.0;
    uint32_t noise_ = 0x12345678u, ages_ = 0;
};
} // namespace fm
