#pragma once
// The editor. Three areas: GENERATION (left), PATTERN (centre), VARIATION (right); presets along the
// top. Keys: G generate, R regenerate, M mutate, Space audition, Cmd+Z / Cmd+Shift+Z undo / redo.

#include "plugin/Panels.h"
#include "plugin/PatternView.h"

namespace fm
{
// Drag this into Live's arrangement or a clip slot: the fill as a MIDI clip.
class DragHandle final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragHandle (FillMakerProcessor& p) : proc (p) { setTooltip ("Drag into Live: drops the fill as a MIDI clip"); setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }

private:
    FillMakerProcessor& proc;
    bool dragging = false;
};

class FillMakerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FillMakerEditor (FillMakerProcessor&);
    ~FillMakerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    PatternView& patternView() noexcept { return roll; }
    VariationPanel& variationPanel() noexcept { return variation; }
    lvrs::Spotlight& getSpotlight() { return *spotlight; }

private:
    void timerCallback() override;
    void doMutate();
    void buildPresetMenu();
    void savePresetDialog();
    void fileMenu();

    Look look;
    FillMakerProcessor& proc;
    GenerationPanel generation;
    VariationPanel variation;
    PatternView roll;
    juce::ComboBox presets;
    juce::TextButton savePreset { "Save preset" }, prevPreset { "<" }, nextPreset { ">" };
    juce::TextButton audition { "Audition" }, zoomIn { "+" }, zoomOut { "-" }, zoomFit { "Fit" }, exportMidi { "Export MIDI" }, fileButton { "Pattern" };
    Chip loop;
    Menu playMode, phrase;
    DragHandle drag;
    juce::Label structure, status;
    juce::Rectangle<int> top, centre;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tips { this, 700 };
    std::vector<juce::File> userPresetFiles;
    juce::String shownPreset;
    std::unique_ptr<lvrs::Spotlight> spotlight;   // shows what Key Map / MIDI Map / automation is moving
};
} // namespace fm
