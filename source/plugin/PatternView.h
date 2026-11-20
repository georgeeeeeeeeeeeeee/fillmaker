#pragma once
// The pattern editor: a piano roll for drum fills. Time across (bars, beats and the current grid,
// whatever subdivision it is), pitch down the side (only the range in use, so it stays readable),
// velocity as brightness and as stems in the lane below. A note's original grid position shows as a
// tick when swing or displacement has moved it; a dashed note plays only sometimes.
//
//   click             select (Shift adds)            drag empty      box-select
//   drag a note       move (snapped; Alt: fine)      drag its end    change length
//   double-click      add a note / delete one        right-click     accent, probability, delete
//   velocity lane     paint velocities               ruler drag      a region for Mutate
//   wheel             scroll   Cmd+wheel zoom   Shift+wheel pitch range
//   Delete            delete selected   arrows: nudge (Shift: octave/beat)   Cmd+A: all   Esc: clear

#include "plugin/Look.h"

#include <set>

namespace fm
{
class PatternView final : public juce::Component
{
public:
    explicit PatternView (FillMakerProcessor&);
    void refresh();   // ~30 Hz: picks up new patterns, moves the playhead
    void zoomBy (double factor);
    void fit();
    std::vector<uint32_t> selection() const { return { selected_.begin(), selected_.end() }; }
    bool hasRegion() const noexcept { return regionB_ > regionA_ + 1.0e-9; }
    double regionStart() const noexcept { return regionA_; }
    double regionEnd() const noexcept { return regionB_; }
    std::function<void()> onSelection;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Drag { none, move, resize, velocity, box, region };
    double beatAt (float x) const noexcept;
    float xAt (double beat) const noexcept;
    int pitchAt (float y) const noexcept;
    float yAt (int pitch) const noexcept;
    double step() const noexcept;
    double snap (double beat, bool fine) const noexcept;
    Event* hit (juce::Point<float>) noexcept;
    juce::Rectangle<float> noteRect (const Event&) const noexcept;
    void autoRange();
    void commit (const juce::String& label);
    void showNoteMenu (uint32_t id);
    void changed() { proc_.previewEdit (working_); dirty_ = true; repaint(); }

    FillMakerProcessor& proc_;
    Pattern working_, before_;
    uint32_t shown_ = 0xffffffff;
    std::set<uint32_t> selected_;
    juce::Rectangle<int> keys_, ruler_, roll_, lane_;
    double pxPerBeat_ = 120.0, scroll_ = 0.0;
    int low_ = 36, high_ = 51;
    bool manualRange_ = false, dirty_ = false;
    double regionA_ = 0.0, regionB_ = 0.0;
    Drag drag_ = Drag::none;
    juce::Point<float> down_;
    juce::Rectangle<float> box_;
    double playhead_ = -1.0;
};
} // namespace fm
