#pragma once
// Presets. Factory presets set the generation (density, behaviour, engines, grammars ...) and leave
// the pitch pool alone, since that belongs to your kit. User presets save everything, the pool too,
// and optionally the pattern on screen. Files are JSON (.fillpreset) in the user preset folder;
// patterns on their own are .fillpattern files.

#include "engine/Mutation.h"

namespace fm
{
struct Preset
{
    juce::String name, category;
    Settings settings;
    MutationSettings mutation;
    bool hasPattern = false;
    Pattern pattern;
    bool keepsPool = true;   // factory: true (the pool and pitch range are left as they are)

    juce::var toVar() const;
    static Preset fromVar (const juce::var&);
};

const std::vector<Preset>& factoryPresets();
// Applies a preset to existing settings (a factory preset keeps the pool and range).
Settings applyPreset (const Preset&, const Settings& current);

juce::File userPresetFolder();   // ~/Library/Application Support/LeverStudios/Fill Maker/Presets (or the OS's equivalent)
bool saveUserPreset (const juce::File&, const Preset&, juce::String* error = nullptr);
bool loadUserPreset (const juce::File&, Preset&, juce::String* error = nullptr);
juce::Array<juce::File> listUserPresets (const juce::File& folder);

bool savePatternFile (const juce::File&, const Pattern&, juce::String* error = nullptr);
bool loadPatternFile (const juce::File&, Pattern&, juce::String* error = nullptr);

std::vector<PitchEntry> defaultPool();   // 36..51 (a 16-pad drum rack's first pads), mixed weights
} // namespace fm
