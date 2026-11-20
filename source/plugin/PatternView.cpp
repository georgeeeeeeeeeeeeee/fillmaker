#include "plugin/PatternView.h"

namespace fm
{
namespace
{
constexpr int kKeys = 54, kRuler = 18, kLane = 54;
}

PatternView::PatternView (FillMakerProcessor& p) : proc_ (p)
{
    setWantsKeyboardFocus (true);
    working_ = proc_.pattern();
    shown_ = proc_.patternVersion();
    autoRange();
}

// ---------------------------------------------------------------------------------------------- geometry
double PatternView::beatAt (float x) const noexcept { return scroll_ + (x - roll_.getX()) / pxPerBeat_; }
float PatternView::xAt (double beat) const noexcept { return (float) (roll_.getX() + (beat - scroll_) * pxPerBeat_); }

int PatternView::pitchAt (float y) const noexcept
{
    const float rowH = (float) roll_.getHeight() / (high_ - low_ + 1);
    return std::clamp (high_ - (int) std::floor ((y - roll_.getY()) / rowH), 0, 127);
}

float PatternView::yAt (int pitch) const noexcept
{
    const float rowH = (float) roll_.getHeight() / (high_ - low_ + 1);
    return roll_.getY() + (high_ - pitch) * rowH;
}

double PatternView::step() const noexcept { return 1.0 / proc_.settings().stepsPerBeat(); }

double PatternView::snap (double beat, bool fine) const noexcept
{
    if (fine) return std::round (beat * 960.0) / 960.0;
    const double s = step();
    return std::round (beat / s) * s;
}

juce::Rectangle<float> PatternView::noteRect (const Event& e) const noexcept
{
    const float rowH = (float) roll_.getHeight() / (high_ - low_ + 1);
    const float x0 = xAt (e.time()), x1 = std::max (x0 + 3.0f, xAt (e.end()));
    return { x0, yAt (e.pitch) + 1.0f, x1 - x0, std::max (2.0f, rowH - 2.0f) };
}

Event* PatternView::hit (juce::Point<float> pt) noexcept
{
    // Last drawn on top: search backwards.
    for (auto it = working_.events.rbegin(); it != working_.events.rend(); ++it)
        if (noteRect (*it).expanded (1.0f, 0.0f).contains (pt)) return &*it;
    return nullptr;
}

void PatternView::autoRange()
{
    if (manualRange_) return;
    int lo = 127, hi = 0;
    for (const auto& e : proc_.pool()) if (e.enabled) { lo = std::min (lo, e.pitch); hi = std::max (hi, e.pitch); }
    for (const auto& e : working_.events) { lo = std::min (lo, e.pitch); hi = std::max (hi, e.pitch); }
    if (lo > hi) { lo = 36; hi = 51; }
    lo = std::max (0, lo - 1);
    hi = std::min (127, hi + 1);
    while (hi - lo < 7) { if (lo > 0) --lo; if (hi - lo < 7 && hi < 127) ++hi; }
    low_ = lo;
    high_ = hi;
}

void PatternView::fit()
{
    if (roll_.getWidth() <= 0) return;
    pxPerBeat_ = roll_.getWidth() / std::max (0.25, working_.length);
    scroll_ = 0.0;
    repaint();
}

void PatternView::zoomBy (double f)
{
    const double centre = scroll_ + roll_.getWidth() * 0.5 / pxPerBeat_;
    const double minPx = roll_.getWidth() / std::max (0.25, working_.length);
    pxPerBeat_ = std::clamp (pxPerBeat_ * f, minPx, 4000.0);
    scroll_ = std::clamp (centre - roll_.getWidth() * 0.5 / pxPerBeat_, 0.0, std::max (0.0, working_.length - roll_.getWidth() / pxPerBeat_));
    repaint();
}

void PatternView::resized()
{
    auto r = getLocalBounds();
    keys_ = r.removeFromLeft (kKeys);
    ruler_ = r.removeFromTop (kRuler);
    lane_ = r.removeFromBottom (kLane);
    roll_ = r;
    keys_ = keys_.withTrimmedTop (kRuler).withTrimmedBottom (kLane);
    fit();
}

void PatternView::refresh()
{
    if (drag_ == Drag::none && proc_.patternVersion() != shown_)
    {
        const bool lengthChanged = std::abs (working_.length - proc_.pattern().length) > 1.0e-9;
        working_ = proc_.pattern();
        shown_ = proc_.patternVersion();
        for (auto it = selected_.begin(); it != selected_.end();)
            it = working_.find (*it) == nullptr ? selected_.erase (it) : std::next (it);
        if (regionB_ > working_.length) { regionA_ = regionB_ = 0.0; }
        autoRange();
        if (lengthChanged) fit();
        repaint();
    }
    const double ph = proc_.playheadBeat();
    if (std::abs (ph - playhead_) > 1.0e-4) { playhead_ = ph; repaint(); }
}

// ---------------------------------------------------------------------------------------------- paint
void PatternView::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    const float rowH = (float) roll_.getHeight() / (high_ - low_ + 1);
    std::set<int> poolPitches;
    for (const auto& e : proc_.pool()) if (e.enabled) poolPitches.insert (e.pitch);

    // Rows and the key column.
    for (int pt = low_; pt <= high_; ++pt)
    {
        const float y = yAt (pt);
        const bool inPool = poolPitches.count (pt) > 0;
        g.setColour (inPool ? ui::panel : ui::bg);
        g.fillRect ((float) roll_.getX(), y, (float) roll_.getWidth(), rowH);
        g.setColour (ui::line.withAlpha (0.35f));
        g.drawHorizontalLine ((int) y, (float) roll_.getX(), (float) roll_.getRight());
        if (rowH >= 9.0f)
        {
            g.setColour (inPool ? ui::text : ui::faint);
            g.setFont (ui::mono (std::min (11.0f, rowH * 0.75f)));
            g.drawText (noteName (pt) + " " + juce::String (pt), juce::Rectangle<float> ((float) keys_.getX() + 3, y, (float) keys_.getWidth() - 6, rowH),
                        juce::Justification::centredRight);
        }
    }

    // Grid: the current subdivision (faint), beats, bars (with numbers in the ruler).
    g.saveState();
    g.reduceClipRegion (roll_.getUnion (lane_).getUnion (ruler_));
    const double L = working_.length, bar = working_.beatsPerBar;
    const double st = step();
    const double first = std::floor (scroll_ / st) * st, last = scroll_ + roll_.getWidth() / pxPerBeat_;
    if (st * pxPerBeat_ >= 5.0)
        for (double b = first; b <= std::min (L, last) + 1.0e-9; b += st)
        {
            g.setColour (ui::line.withAlpha (0.45f));
            g.drawVerticalLine ((int) xAt (b), (float) roll_.getY(), (float) lane_.getBottom());
        }
    for (double b = std::ceil (scroll_); b <= std::min (L, last) + 1.0e-9; b += 1.0)
    {
        const bool isBar = std::abs (std::fmod (b, bar)) < 1.0e-6;
        g.setColour (isBar ? ui::dim.withAlpha (0.6f) : ui::line);
        g.drawVerticalLine ((int) xAt (b), (float) ruler_.getY() + (isBar ? 0 : 9), (float) lane_.getBottom());
        if (isBar)
        {
            g.setColour (ui::dim);
            g.setFont (ui::mono (10.0f));
            g.drawText (juce::String ((int) std::lround (b / bar) + 1), (int) xAt (b) + 3, ruler_.getY(), 30, ruler_.getHeight(), juce::Justification::centredLeft);
        }
    }
    // Past the end of the pattern: dimmed.
    if (xAt (L) < roll_.getRight())
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (xAt (L), (float) ruler_.getY(), roll_.getRight() - xAt (L), (float) (lane_.getBottom() - ruler_.getY())));
    }
    // The Mutate region.
    if (hasRegion())
    {
        const auto r = juce::Rectangle<float> (xAt (regionA_), (float) ruler_.getY(), xAt (regionB_) - xAt (regionA_), (float) (lane_.getBottom() - ruler_.getY()));
        g.setColour (ui::accent.withAlpha (0.07f));
        g.fillRect (r);
        g.setColour (ui::accent.withAlpha (0.8f));
        g.fillRect (r.withHeight ((float) ruler_.getHeight()).reduced (0, 5));
    }
    g.setColour (ui::line);
    g.drawHorizontalLine (ruler_.getBottom(), (float) roll_.getX(), (float) roll_.getRight());
    g.drawHorizontalLine (lane_.getY(), (float) roll_.getX(), (float) roll_.getRight());

    // Notes.
    for (const auto& e : working_.events)
    {
        if (e.pitch < low_ || e.pitch > high_) continue;
        const auto r = noteRect (e);
        const bool sel = selected_.count (e.id) > 0;
        const float v = e.velocity / 127.0f;
        auto c = ui::cool.withAlpha (0.25f + 0.75f * v);
        if (e.probability < 1.0f) c = c.withMultipliedAlpha (0.55f + 0.45f * e.probability);
        g.setColour (c);
        g.fillRoundedRectangle (r, 2.0f);
        if (e.probability < 1.0f)
        {
            // Hatched: it plays only sometimes.
            g.saveState();
            g.reduceClipRegion (r.toNearestInt());
            g.setColour (ui::bg.withAlpha (0.55f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 5.0f) g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.2f);
            g.restoreState();
            if (r.getWidth() > 24.0f && r.getHeight() > 9.0f)
            {
                g.setColour (ui::text);
                g.setFont (ui::mono (9.0f));
                g.drawText (juce::String (juce::roundToInt (e.probability * 100)) + "%", r.reduced (2, 0), juce::Justification::centredLeft);
            }
        }
        if (e.accent) { g.setColour (ui::accent); g.fillRect (r.withHeight (2.5f)); }
        if (sel) { g.setColour (ui::selection); g.drawRoundedRectangle (r.expanded (0.5f), 2.0f, 1.5f); }
        // Displaced off its grid position: a tick where it belongs.
        if (std::abs (e.offset) > 1.0e-4)
        {
            g.setColour (ui::cool.withAlpha (0.6f));
            const float gx = xAt (e.start);
            g.drawLine (gx, r.getY() + 1.0f, gx, r.getBottom() - 1.0f, 1.0f);
        }
        // Velocity stem.
        const float lx = xAt (e.time());
        const float top = lane_.getBottom() - 4.0f - v * (lane_.getHeight() - 10.0f);
        g.setColour ((sel ? ui::selection : ui::cool).withAlpha (0.4f + 0.6f * v));
        g.fillRect (juce::Rectangle<float> (lx, top, 2.0f, lane_.getBottom() - 4.0f - top));
        g.fillEllipse (lx - 2.0f, top - 2.0f, 6.0f, 6.0f);
    }
    if (drag_ == Drag::box)
    {
        g.setColour (ui::selection.withAlpha (0.08f));
        g.fillRect (box_);
        g.setColour (ui::selection.withAlpha (0.6f));
        g.drawRect (box_, 1.0f);
    }
    // The playhead.
    if (playhead_ >= 0.0)
    {
        g.setColour (ui::accent);
        g.drawLine (xAt (playhead_), (float) ruler_.getY(), xAt (playhead_), (float) lane_.getBottom(), 1.5f);
    }
    g.restoreState();
    g.setColour (ui::faint);
    g.setFont (ui::label (9.5f));
    g.drawText ("VEL", keys_.getX(), lane_.getY(), keys_.getWidth() - 6, lane_.getHeight(), juce::Justification::centredRight);
    if (working_.events.empty())
    {
        g.setColour (ui::dim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Press GENERATE, or double-click to draw notes", roll_, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------------------------- mouse
void PatternView::mouseMove (const juce::MouseEvent& e)
{
    auto* n = roll_.contains (e.getPosition()) ? hit (e.position) : nullptr;
    if (n != nullptr && e.position.x > noteRect (*n).getRight() - 5.0f) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (n != nullptr) setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else if (lane_.contains (e.getPosition())) setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else setMouseCursor (juce::MouseCursor::NormalCursor);
}

void PatternView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    down_ = e.position;
    before_ = working_;
    dirty_ = false;
    if (ruler_.contains (e.getPosition()))
    {
        drag_ = Drag::region;
        regionA_ = regionB_ = std::clamp (snap (beatAt (e.position.x), false), 0.0, working_.length);
        repaint();
        if (onSelection) onSelection();
        return;
    }
    if (lane_.contains (e.getPosition()))
    {
        drag_ = Drag::velocity;
        mouseDrag (e);
        return;
    }
    if (! roll_.contains (e.getPosition())) return;
    auto* n = hit (e.position);
    if (e.mods.isPopupMenu())
    {
        if (n != nullptr) showNoteMenu (n->id);
        return;
    }
    if (n == nullptr)
    {
        if (! e.mods.isShiftDown()) selected_.clear();
        drag_ = Drag::box;
        box_ = {};
        if (onSelection) onSelection();
        repaint();
        return;
    }
    if (e.mods.isShiftDown()) { if (! selected_.erase (n->id)) selected_.insert (n->id); }
    else if (selected_.count (n->id) == 0) { selected_.clear(); selected_.insert (n->id); }
    drag_ = e.position.x > noteRect (*n).getRight() - 5.0f ? Drag::resize : Drag::move;
    if (onSelection) onSelection();
    repaint();
}

void PatternView::mouseDrag (const juce::MouseEvent& e)
{
    const bool fine = e.mods.isAltDown();
    switch (drag_)
    {
        case Drag::region:
            regionB_ = std::clamp (snap (beatAt (e.position.x), false), 0.0, working_.length);
            if (regionB_ < regionA_) std::swap (regionA_, regionB_);
            repaint();
            if (onSelection) onSelection();
            break;
        case Drag::box:
        {
            box_ = juce::Rectangle<float> (down_, e.position);
            if (! e.mods.isShiftDown()) selected_.clear();
            for (const auto& n : working_.events) if (box_.intersects (noteRect (n))) selected_.insert (n.id);
            if (onSelection) onSelection();
            repaint();
            break;
        }
        case Drag::move:
        {
            const double db = snap (beatAt (e.position.x), fine) - snap (beatAt (down_.x), fine);
            const int dp = pitchAt (e.position.y) - pitchAt (down_.y);
            working_ = before_;
            for (auto& n : working_.events)
            {
                if (selected_.count (n.id) == 0) continue;
                n.start = std::clamp (n.start + db, 0.0, working_.length - Pattern::kMinLength);
                n.pitch = std::clamp (n.pitch + dp, 0, 127);
                if (n.pitch > high_) high_ = n.pitch;
                if (n.pitch < low_) low_ = n.pitch;
            }
            changed();
            break;
        }
        case Drag::resize:
        {
            const double db = snap (beatAt (e.position.x), fine) - snap (beatAt (down_.x), fine);
            working_ = before_;
            for (auto& n : working_.events)
                if (selected_.count (n.id) > 0)
                    n.length = std::clamp (n.length + db, Pattern::kMinLength, std::max (Pattern::kMinLength, working_.length - n.time()));
            changed();
            break;
        }
        case Drag::velocity:
        {
            // Paint: the note nearest the pointer (within a few pixels) takes the height's velocity.
            const int v = std::clamp ((int) std::lround ((lane_.getBottom() - 4.0f - e.position.y) / (lane_.getHeight() - 10.0f) * 127.0f), 1, 127);
            Event* best = nullptr;
            float bestD = 7.0f;
            for (auto& n : working_.events)
            {
                const float d = std::abs (xAt (n.time()) - e.position.x);
                if (d < bestD || (best != nullptr && d <= bestD + 0.01f && selected_.count (n.id) > 0)) { bestD = d; best = &n; }
            }
            if (best != nullptr)
            {
                // Several notes at the same moment (a chord of hits): all of them, if selected.
                const double t = best->time();
                for (auto& n : working_.events)
                    if (&n == best || (std::abs (n.time() - t) < 1.0e-6 && selected_.count (n.id) > 0)) n.velocity = v;
                changed();
            }
            break;
        }
        case Drag::none: break;
    }
}

void PatternView::mouseUp (const juce::MouseEvent&)
{
    const auto d = drag_;
    drag_ = Drag::none;
    if (dirty_)
    {
        commit (d == Drag::move ? "Move notes" : d == Drag::resize ? "Change length" : d == Drag::velocity ? "Velocity" : "Edit");
        if (d == Drag::move) autoRange();
    }
    if (d == Drag::region && ! hasRegion()) { regionA_ = regionB_ = 0.0; if (onSelection) onSelection(); }
    repaint();
}

void PatternView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! roll_.contains (e.getPosition())) return;
    before_ = working_;
    if (auto* n = hit (e.position))
    {
        const auto id = n->id;
        working_.remove (id);
        selected_.erase (id);
        commit ("Delete note");
        return;
    }
    Event n;
    n.start = std::clamp (std::floor (beatAt (e.position.x) / step()) * step(), 0.0, working_.length - step());
    if (e.mods.isAltDown()) n.start = snap (beatAt (e.position.x), true);
    n.pitch = pitchAt (e.position.y);
    n.length = std::max (Pattern::kMinLength, step() * 0.9);
    n.velocity = juce::roundToInt (proc_.state().getRawParameterValue ("velBase")->load());
    auto& added = working_.add (n);
    selected_ = { added.id };
    commit ("Add note");
}

void PatternView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown()) { zoomBy (w.deltaY > 0 ? 1.15 : 1.0 / 1.15); return; }
    if (e.mods.isShiftDown())
    {
        const int d = w.deltaY > 0 ? 1 : -1;
        if (low_ + d >= 0 && high_ + d <= 127) { low_ += d; high_ += d; manualRange_ = true; repaint(); }
        return;
    }
    const double d = (std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : -w.deltaY) * 200.0 / pxPerBeat_;
    scroll_ = std::clamp (scroll_ + d, 0.0, std::max (0.0, working_.length - roll_.getWidth() / pxPerBeat_));
    repaint();
}

bool PatternView::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if ((k.isKeyCode (juce::KeyPress::deleteKey) || k.isKeyCode (juce::KeyPress::backspaceKey)) && ! selected_.empty())
    {
        before_ = working_;
        for (auto id : selected_) working_.remove (id);
        selected_.clear();
        commit ("Delete notes");
        return true;
    }
    if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        for (const auto& n : working_.events) selected_.insert (n.id);
        if (onSelection) onSelection();
        repaint();
        return true;
    }
    if (k.isKeyCode (juce::KeyPress::escapeKey))
    {
        selected_.clear();
        regionA_ = regionB_ = 0.0;
        if (onSelection) onSelection();
        repaint();
        return true;
    }
    const bool up = k.isKeyCode (juce::KeyPress::upKey), downK = k.isKeyCode (juce::KeyPress::downKey);
    const bool left = k.isKeyCode (juce::KeyPress::leftKey), right = k.isKeyCode (juce::KeyPress::rightKey);
    if ((up || downK || left || right) && ! selected_.empty())
    {
        before_ = working_;
        const int dp = up ? (mods.isShiftDown() ? 12 : 1) : downK ? (mods.isShiftDown() ? -12 : -1) : 0;
        const double db = left || right ? (right ? 1.0 : -1.0) * (mods.isShiftDown() ? 1.0 : mods.isAltDown() ? 1.0 / 960.0 : step()) : 0.0;
        for (auto& n : working_.events)
            if (selected_.count (n.id))
            {
                n.pitch = std::clamp (n.pitch + dp, 0, 127);
                n.start = std::clamp (n.start + db, 0.0, working_.length - Pattern::kMinLength);
            }
        commit (dp != 0 ? "Change pitch" : "Nudge");
        manualRange_ = false;
        autoRange();
        return true;
    }
    return false;
}

void PatternView::commit (const juce::String& label)
{
    working_.sanitise();
    proc_.commitEdit (working_, label);
    shown_ = proc_.patternVersion();
    dirty_ = false;
    if (onSelection) onSelection();
    repaint();
}

void PatternView::showNoteMenu (uint32_t id)
{
    auto* n = working_.find (id);
    if (n == nullptr) return;
    if (selected_.count (id) == 0) selected_ = { id };
    juce::PopupMenu m;
    m.addSectionHeader (noteName (n->pitch) + " (" + juce::String (n->pitch) + "), velocity " + juce::String (n->velocity));
    m.addItem (1, "Accent", true, n->accent);
    juce::PopupMenu prob;
    for (int pc : { 100, 75, 50, 25 }) prob.addItem (10 + pc, juce::String (pc) + "%", true, juce::roundToInt (n->probability * 100) == pc);
    m.addSubMenu ("Probability", prob);
    juce::PopupMenu vel;
    for (int v : { 127, 110, 90, 64, 40, 20 }) vel.addItem (200 + v, juce::String (v), true, n->velocity == v);
    m.addSubMenu ("Velocity", vel);
    m.addItem (2, "Mutate selected notes");
    m.addSeparator();
    m.addItem (3, selected_.size() > 1 ? "Delete selected" : "Delete");
    juce::Component::SafePointer<PatternView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ juce::Desktop::getMousePosition(), juce::Desktop::getMousePosition() }), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& v = *safe;
        v.before_ = v.working_;
        auto forSel = [&v] (auto fn) { for (auto& e : v.working_.events) if (v.selected_.count (e.id)) fn (e); };
        if (r == 1) { bool on = false; forSel ([&] (Event& e) { on = on || ! e.accent; }); forSel ([&] (Event& e) { e.accent = on; }); v.commit ("Accent"); }
        else if (r >= 10 && r < 200) { forSel ([r] (Event& e) { e.probability = (r - 10) / 100.0f; }); v.commit ("Probability"); }
        else if (r >= 200) { forSel ([r] (Event& e) { e.velocity = r - 200; }); v.commit ("Velocity"); }
        else if (r == 2) v.proc_.mutate (0.0, -1.0, v.selection());
        else if (r == 3) { for (auto id : std::vector<uint32_t> (v.selected_.begin(), v.selected_.end())) v.working_.remove (id); v.selected_.clear(); v.commit ("Delete notes"); }
    });
}
} // namespace fm
