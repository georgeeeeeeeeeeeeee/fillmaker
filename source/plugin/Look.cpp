#include "plugin/Look.h"

namespace fm
{
juce::Font ui::mono (float size, bool bold)
{
    return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain);
}

juce::Font ui::label (float size)
{
    return juce::FontOptions (size, juce::Font::bold).withKerningFactor (0.12f);
}

Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::PopupMenu::backgroundColourId, ui::raised);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::line);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::text);
    setColour (juce::PopupMenu::headerTextColourId, ui::dim);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::backgroundColourId, ui::raised);
    setColour (juce::ComboBox::outlineColourId, ui::line);
    setColour (juce::ComboBox::arrowColourId, ui::dim);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::TextEditor::backgroundColourId, ui::raised);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::outlineColourId, ui::line);
    setColour (juce::TextEditor::focusedOutlineColourId, ui::accent);
    setColour (juce::TooltipWindow::backgroundColourId, ui::raised);
    setColour (juce::TooltipWindow::textColourId, ui::text);
    setColour (juce::TooltipWindow::outlineColourId, ui::line);
    setColour (juce::TextButton::buttonColourId, ui::raised);
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::AlertWindow::backgroundColourId, ui::panel);
    setColour (juce::AlertWindow::textColourId, ui::text);
    setColour (juce::ScrollBar::thumbColourId, ui::line);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float rad = std::min (r.getWidth(), r.getHeight()) * 0.5f - 2.0f;
    if (rad < 5.0f) return;
    const float cx = r.getCentreX(), cy = r.getCentreY(), th = std::max (2.5f, rad * 0.13f);
    const auto c = s.findColour (juce::Slider::rotarySliderFillColourId);
    const juce::PathStrokeType stroke (th, juce::PathStrokeType::curved, juce::PathStrokeType::butt);
    juce::Path track;
    track.addCentredArc (cx, cy, rad, rad, 0.0f, start, end, true);
    g.setColour (ui::line);
    g.strokePath (track, stroke);
    const float to = start + pos * (end - start);
    if (pos > 0.002f)
    {
        juce::Path v;
        v.addCentredArc (cx, cy, rad, rad, 0.0f, start, to, true);
        g.setColour (c);
        g.strokePath (v, stroke);
    }
    // A short tick at the value, inside the arc.
    const auto at = [&] (float rr) { return juce::Point<float> (cx + rr * std::sin (to), cy - rr * std::cos (to)); };
    g.setColour (ui::text);
    g.drawLine ({ at (rad * 0.45f), at (rad * 0.82f) }, 2.0f);
}

void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s); return; }
    const auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (ui::line);
    g.fillRoundedRectangle (r, 3.0f);
    const auto c = s.findColour (juce::Slider::trackColourId);
    g.setColour (c);
    g.fillRoundedRectangle (r.withRight (pos), 3.0f);
    g.setColour (ui::text);
    g.fillRoundedRectangle (juce::Rectangle<float> (pos - 2.0f, (float) y + h * 0.5f - 9.0f, 4.0f, 18.0f), 2.0f);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (down) fill = fill.brighter (0.15f);
    else if (over) fill = fill.brighter (0.07f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? fill.brighter (0.3f) : ui::line);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    g.setColour (b.findColour (on ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    const float size = (float) b.getProperties().getWithDefault ("fontSize", 11.0);
    g.setFont (ui::label (size));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 2);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ui::accent : ui::line);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (ui::dim);
    g.fillPath (arrow);
}

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
    l.setFont (getComboBoxFont (box));
}

juce::Label* Look::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (ui::mono (11.5f));
    l->setColour (juce::Label::textColourId, ui::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

// ---------------------------------------------------------------------------------------------- widgets
Knob::Knob (FillMakerProcessor& p, const juce::String& id, const juce::String& name, const juce::String& tip, juce::Colour c, bool b) : big (b)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, big ? 18 : 15);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.4f));
    slider.setTooltip (tip + "  (double-click: default)");
    caption.setText (name.toUpperCase(), juce::dontSendNotification);
    caption.setFont (ui::label (big ? 11.5f : 10.0f));
    caption.setColour (juce::Label::textColourId, ui::dim);
    caption.setJustificationType (juce::Justification::centred);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (caption);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.state(), id, slider);
    lvrs::Spotlight::tag (*this, id);
    if (auto* q = p.state().getParameter (id)) slider.setDoubleClickReturnValue (true, q->convertFrom0to1 (q->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (big ? 15 : 13));
    slider.setBounds (r);
}

Menu::Menu (FillMakerProcessor& p, const juce::String& id, const juce::String& name, const juce::String& tip)
{
    if (auto* q = dynamic_cast<juce::AudioParameterChoice*> (p.state().getParameter (id))) box.addItemList (q->choices, 1);
    box.setTooltip (tip);
    caption.setText (name.toUpperCase(), juce::dontSendNotification);
    caption.setFont (ui::label (10.0f));
    caption.setColour (juce::Label::textColourId, ui::dim);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (box);
    addAndMakeVisible (caption);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.state(), id, box);
    lvrs::Spotlight::tag (*this, id);
}

void Menu::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (13));
    box.setBounds (r.removeFromTop (24));
}

Chip::Chip (FillMakerProcessor& p, const juce::String& id, const juce::String& text, const juce::String& tip, juce::Colour on) : juce::TextButton (text)
{
    setClickingTogglesState (true);
    setTooltip (tip);
    setColour (juce::TextButton::buttonColourId, ui::raised);
    setColour (juce::TextButton::buttonOnColourId, on.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOffId, ui::dim);
    setColour (juce::TextButton::textColourOnId, ui::bg);
    getProperties().set ("fontSize", 10.0);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.state(), id, *this);
    lvrs::Spotlight::tag (*this, id);
}

void heading (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t)
{
    g.setFont (ui::label (10.5f));
    g.setColour (ui::faint);
    g.drawText (t.toUpperCase(), r, juce::Justification::centredLeft);
    const int w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t.toUpperCase()) + 8;
    g.setColour (ui::line);
    g.drawHorizontalLine (r.getCentreY(), (float) (r.getX() + w), (float) r.getRight());
}

void styleAction (juce::TextButton& b, juce::Colour fill, juce::Colour textColour)
{
    b.setColour (juce::TextButton::buttonColourId, fill);
    b.setColour (juce::TextButton::buttonOnColourId, fill.brighter (0.2f));
    b.setColour (juce::TextButton::textColourOffId, textColour);
    b.setColour (juce::TextButton::textColourOnId, textColour);
}
} // namespace fm
