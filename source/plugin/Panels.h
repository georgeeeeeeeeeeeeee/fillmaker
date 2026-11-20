#pragma once
// The editor's panels: GENERATION (left), VARIATION with its tabs (right), the pitch pool editor,
// groove and output settings, and the history strip.

#include "plugin/Look.h"

namespace fm
{
class GenerationPanel final : public juce::Component
{
public:
    explicit GenerationPanel (FillMakerProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    juce::TextButton generate { "Generate" }, regenerate { "Regenerate" }, mutate { "Mutate" };

private:
    FillMakerProcessor& proc;
    Knob density, complexity, swing, humanise, customLength, customSub, velBase, velVar, accent, ghost, curveDepth;
    Menu length, resolution, curve;
    juce::Slider behaviour;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> behaviourAtt;
    juce::Label behaviourValue;
    juce::Rectangle<int> shapeHead, behaviourArea, velHead;
};

// The pitch pool: which pitches a fill may use, how often, and how reliably.
class PoolView final : public juce::Component
{
public:
    explicit PoolView (FillMakerProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int rowHeight() const noexcept { return 22; }

private:
    enum class Col { none, enable, pitch, weight, probability, remove };
    Col colAt (int x) const noexcept;
    juce::Rectangle<int> cell (int row, Col) const noexcept;
    void apply (std::vector<PitchEntry>);
    FillMakerProcessor& proc;
    Col dragging = Col::none;
    int dragRow = -1, dragStartPitch = 0;
    float dragStartY = 0.0f;
    int scroll = 0;
};

class HistoryStrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HistoryStrip (FillMakerProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void refresh();

private:
    int indexAt (int x) const noexcept;
    int first() const noexcept;
    FillMakerProcessor& proc;
    int shownSize = -1, shownCursor = -1, cellW = 14;
};

class VariationPanel final : public juce::Component
{
public:
    explicit VariationPanel (FillMakerProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void showTab (int);
    juce::TextButton undo { "Undo" }, redo { "Redo" };

private:
    FillMakerProcessor& proc;
    int tab = 0;
    std::array<juce::TextButton, 4> tabs;
    // Variation
    Knob preservation, mTiming, mVelocity, mPitch, mDensity, mStructure, mSubdivision, mProbability;
    juce::OwnedArray<Chip> engineChips, grammarChips;
    // Pitch
    PoolView pool;
    Knob pitchLow, pitchHigh;
    Menu motion;
    juce::TextButton addPitch { "+ Pitch" }, resetPool { "Reset 36-51" }, fromPattern { "From pattern" };
    // Groove
    Chip grooveAware;
    Knob grooveStrength;
    juce::TextButton follow { "Follow MIDI in" }, capture { "Capture now" }, importMidi { "Import MIDI file" }, clearGroove { "Clear" };
    juce::Label grooveText;
    juce::Rectangle<int> grooveMap;
    // More
    Knob euclidSteps, euclidPulses, euclidRotation, channel;
    Menu ratchetMax, poly, preview;
    Chip thru, auditionMidi;
    // Always shown
    juce::Label seedLabel;
    Chip seedLock;
    HistoryStrip history;
    juce::Label historyText;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> heads;
    juce::Rectangle<int> content, bottom;

    std::vector<juce::Component*> tabComponents (int) ;
};
} // namespace fm
