#pragma once
// Standard MIDI files: export (timing, lengths, pitch, velocity, tempo, time signature) and import.

#include "engine/Pattern.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace fm
{
inline constexpr int kTicksPerBeat = 960;

// Notes with a probability under 1 are rolled (with probSeed, so an export is repeatable); `repeats`
// writes the pattern that many times in a row. A note running into the next hit on the same pitch is
// shortened, so no note-on arrives while that pitch is still sounding.
juce::MidiFile toMidiFile (const Pattern&, double bpm, int channel, int repeats = 1, uint32_t probSeed = 1);
bool writeMidiFile (const juce::File&, const Pattern&, double bpm, int channel, int repeats = 1, juce::String* error = nullptr);

// All notes of a MIDI file as a pattern (length rounded up to whole bars).
Pattern patternFromMidiFile (const juce::MidiFile&, double* bpm = nullptr);
} // namespace fm
