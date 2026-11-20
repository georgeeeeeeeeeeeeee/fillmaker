#pragma once
// The look: an instrument panel rather than a plug-in dialog. Near-black, one warm accent (generate),
// one cool one (notes, playhead), monospaced numbers, small caps labels.

#include "plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace fm
{
namespace ui
{
const juce::Colour bg (0xff0c0d10), panel (0xff14161b), raised (0xff1c1f26), line (0xff2a2e37), text (0xffe9ebef),
    dim (0xff8f96a3), faint (0xff5d6470), accent (0xffffb547), cool (0xff5fe1d9), warn (0xffff6b5e), selection (0xffffffff);
juce::Font mono (float size, bool bold = false);
juce::Font label (float size = 10.5f);
}

class Look final : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::mono (12.0f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

// A knob with its name above and its value below (double-click resets).
class Knob final : public juce::Component
{
public:
    Knob (FillMakerProcessor&, const juce::String& paramId, const juce::String& name, const juce::String& tip, juce::Colour = ui::cool, bool big = false);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    bool big;
};

// A labelled menu tied to a choice (or int) parameter.
class Menu final : public juce::Component
{
public:
    Menu (FillMakerProcessor&, const juce::String& paramId, const juce::String& name, const juce::String& tip);
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// A toggle chip tied to a bool parameter.
class Chip final : public juce::TextButton
{
public:
    Chip (FillMakerProcessor&, const juce::String& paramId, const juce::String& text, const juce::String& tip, juce::Colour on = ui::cool);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

void heading (juce::Graphics&, juce::Rectangle<int>, const juce::String&);
void styleAction (juce::TextButton&, juce::Colour fill, juce::Colour textColour);
} // namespace fm
