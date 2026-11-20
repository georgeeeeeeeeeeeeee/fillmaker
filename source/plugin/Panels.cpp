#include "plugin/Panels.h"

#include <map>

namespace fm
{
// ---------------------------------------------------------------------------------------------- generation
GenerationPanel::GenerationPanel (FillMakerProcessor& p)
    : proc (p),
      density (p, "density", "Density", "How many hits: low leaves space, high makes rolls and clusters", ui::accent, true),
      complexity (p, "complexity", "Complexity", "Syncopation, odd subdivisions, irregular groupings, displacement", ui::accent, true),
      swing (p, "swing", "Swing", "Off-beat 16ths (8ths on an 8th grid) pushed late"),
      humanise (p, "humanise", "Humanise", "Small random timing displacement"),
      customLength (p, "customLength", "Beats", "Length when Length is Custom"),
      customSub (p, "customSub", "Per beat", "Steps per beat when Resolution is Custom"),
      velBase (p, "velBase", "Velocity", "The base velocity"),
      velVar (p, "velVar", "Variation", "Random velocity variation"),
      accent (p, "accent", "Accents", "How many and how strong"),
      ghost (p, "ghost", "Ghosts", "Quiet notes on weak positions"),
      curveDepth (p, "curveDepth", "Depth", "How strongly the curve shapes velocity"),
      length (p, "length", "Length", "How long the fill is"),
      resolution (p, "resolution", "Resolution", "The grid it's built on (engines may switch subdivision along the way)"),
      curve (p, "curve", "Curve", "Velocity across the fill")
{
    styleAction (generate, ui::accent, ui::bg);
    generate.getProperties().set ("fontSize", 22.0);
    generate.setTooltip ("A new fill (G). With the seed locked, the same one again");
    styleAction (regenerate, ui::raised, ui::text);
    regenerate.setTooltip ("The same seed with the current settings (R): turn a knob, regenerate, compare");
    styleAction (mutate, ui::raised, ui::accent);
    mutate.setTooltip ("Change this fill, keeping Preservation's share of it (M). With notes selected or a ruler region, only there");
    for (auto* c : std::initializer_list<juce::Component*> { &generate, &regenerate, &mutate, &density, &complexity, &swing, &humanise, &customLength, &customSub,
                                                              &velBase, &velVar, &accent, &ghost, &curveDepth, &length, &resolution, &curve })
        addAndMakeVisible (*c);
    behaviour.setSliderStyle (juce::Slider::LinearHorizontal);
    behaviour.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    behaviour.setColour (juce::Slider::trackColourId, ui::accent);
    behaviour.setTooltip ("The fill's personality: familiar structures and beat-aware accents, through to displacement, odd groupings, extremes and silences");
    addAndMakeVisible (behaviour);
    behaviourAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.state(), "behaviour", behaviour);
    behaviour.setDoubleClickReturnValue (true, 25.0);
    behaviourValue.setFont (ui::mono (12.0f, true));
    behaviourValue.setJustificationType (juce::Justification::centred);
    behaviourValue.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (behaviourValue);
    refresh();
}

void GenerationPanel::refresh()
{
    const bool customLen = juce::roundToInt (proc.state().getRawParameterValue ("length")->load()) == (int) LengthChoice::custom;
    const bool customRes = juce::roundToInt (proc.state().getRawParameterValue ("resolution")->load()) == (int) Resolution::custom;
    customLength.setAlpha (customLen ? 1.0f : 0.35f);
    customSub.setAlpha (customRes ? 1.0f : 0.35f);
    const float b = proc.state().getRawParameterValue ("behaviour")->load();
    const juce::String t = b < 0.5f ? "CONVENTIONAL" : b > 99.5f ? "UNHINGED" : juce::String (juce::roundToInt (b)) + "%";
    if (behaviourValue.getText() != t) behaviourValue.setText (t, juce::dontSendNotification);
    behaviourValue.setColour (juce::Label::textColourId, ui::accent.interpolatedWith (ui::warn, b / 100.0f));
}

void GenerationPanel::paint (juce::Graphics& g)
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    heading (g, shapeHead, "Generation");
    heading (g, velHead, "Velocity");
    // Behaviour: its name above, its two ends below.
    const auto b = behaviourArea;
    g.setFont (ui::label (10.0f));
    g.setColour (ui::dim);
    g.drawText ("BEHAVIOUR", b.withHeight (12), juce::Justification::centredLeft);
    g.setFont (ui::label (9.0f));
    g.setColour (ui::faint);
    g.drawText ("CONVENTIONAL", b.withTop (b.getBottom() - 12), juce::Justification::centredLeft);
    g.drawText ("UNHINGED", b.withTop (b.getBottom() - 12), juce::Justification::centredRight);
}

void GenerationPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    generate.setBounds (r.removeFromTop (72));
    r.removeFromTop (6);
    auto row = r.removeFromTop (30);
    regenerate.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (0, 0).withTrimmedRight (3));
    mutate.setBounds (row.withTrimmedLeft (3));
    r.removeFromTop (8);
    shapeHead = r.removeFromTop (16);
    row = r.removeFromTop (100);
    density.setBounds (row.removeFromLeft (row.getWidth() / 2));
    complexity.setBounds (row);
    r.removeFromTop (4);
    behaviourArea = r.removeFromTop (50);
    const auto b = behaviourArea.withTrimmedTop (13).withHeight (22);
    behaviour.setBounds (b);
    behaviourValue.setBounds (behaviourArea.withTop (behaviourArea.getBottom() - 14).withSizeKeepingCentre (120, 14));
    r.removeFromTop (10);
    row = r.removeFromTop (40);
    length.setBounds (row.removeFromLeft (row.getWidth() / 2).withTrimmedRight (4));
    resolution.setBounds (row.withTrimmedLeft (4));
    r.removeFromTop (6);
    row = r.removeFromTop (70);
    const int w4 = row.getWidth() / 4;
    customLength.setBounds (row.removeFromLeft (w4));
    customSub.setBounds (row.removeFromLeft (w4));
    swing.setBounds (row.removeFromLeft (w4));
    humanise.setBounds (row);
    r.removeFromTop (6);
    velHead = r.removeFromTop (16);
    row = r.removeFromTop (70);
    velBase.setBounds (row.removeFromLeft (w4));
    velVar.setBounds (row.removeFromLeft (w4));
    accent.setBounds (row.removeFromLeft (w4));
    ghost.setBounds (row);
    r.removeFromTop (4);
    row = r.removeFromTop (70);
    curve.setBounds (row.removeFromLeft (w4 * 2).withTrimmedRight (8).withSizeKeepingCentre (w4 * 2 - 8, 40));
    curveDepth.setBounds (row.removeFromLeft (w4));
}

// ---------------------------------------------------------------------------------------------- pool
PoolView::PoolView (FillMakerProcessor& p) : proc (p) {}

PoolView::Col PoolView::colAt (int x) const noexcept
{
    const int w = getWidth();
    if (x < 24) return Col::enable;
    if (x < 96) return Col::pitch;
    if (x < 96 + (w - 120) / 2) return Col::weight;
    if (x < w - 24) return Col::probability;
    return Col::remove;
}

juce::Rectangle<int> PoolView::cell (int row, Col c) const noexcept
{
    const int y = 18 + (row - scroll) * rowHeight(), w = getWidth(), h = rowHeight();
    const int wW = (w - 120) / 2;
    switch (c)
    {
        case Col::enable: return { 0, y, 24, h };
        case Col::pitch: return { 24, y, 72, h };
        case Col::weight: return { 96, y, wW, h };
        case Col::probability: return { 96 + wW, y, w - 24 - 96 - wW, h };
        case Col::remove: return { w - 24, y, 24, h };
        case Col::none: default: return {};
    }
}

void PoolView::paint (juce::Graphics& g)
{
    const auto pool = proc.pool();
    g.setFont (ui::label (9.5f));
    g.setColour (ui::faint);
    g.drawText ("PITCH", cell (scroll, Col::pitch).withY (0).withHeight (16), juce::Justification::centredLeft);
    g.drawText ("WEIGHT", cell (scroll, Col::weight).withY (0).withHeight (16), juce::Justification::centredLeft);
    g.drawText ("PROBABILITY", cell (scroll, Col::probability).withY (0).withHeight (16), juce::Justification::centredLeft);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedTop (18));
    for (int i = 0; i < (int) pool.size(); ++i)
    {
        const auto& e = pool[(size_t) i];
        const auto row = cell (i, Col::enable).getUnion (cell (i, Col::remove));
        if (row.getBottom() < 18 || row.getY() > getHeight()) continue;
        g.setColour (i % 2 ? ui::panel : ui::raised.withAlpha (0.5f));
        g.fillRect (row);
        const auto en = cell (i, Col::enable).reduced (6).toFloat();
        g.setColour (e.enabled ? ui::cool : ui::line);
        g.fillRoundedRectangle (en, 2.0f);
        g.setColour (e.enabled ? ui::text : ui::faint);
        g.setFont (ui::mono (11.5f));
        g.drawText (noteName (e.pitch) + "  " + juce::String (e.pitch), cell (i, Col::pitch).reduced (4, 0), juce::Justification::centredLeft);
        const auto bar = [&] (juce::Rectangle<int> r, float v, juce::Colour c) {
            auto b = r.reduced (4, 7).toFloat();
            g.setColour (ui::line);
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour (c.withAlpha (e.enabled ? 1.0f : 0.35f));
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * v), 2.0f);
        };
        bar (cell (i, Col::weight), e.weight, ui::accent);
        bar (cell (i, Col::probability), e.probability, ui::cool);
        g.setColour (ui::faint);
        g.drawText ("x", cell (i, Col::remove), juce::Justification::centred);
    }
    g.restoreState();
    if (pool.empty())
    {
        g.setColour (ui::dim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("No pool: any pitch in the range", getLocalBounds().withTrimmedTop (18), juce::Justification::centred);
    }
}

void PoolView::resized() {}

void PoolView::apply (std::vector<PitchEntry> pool)
{
    proc.setPool (std::move (pool));
    repaint();
    if (auto* parent = getParentComponent()) parent->repaint();
}

void PoolView::mouseDown (const juce::MouseEvent& e)
{
    auto pool = proc.pool();
    const int row = scroll + (e.y - 18) / rowHeight();
    if (e.y < 18 || row < 0 || row >= (int) pool.size()) return;
    dragging = colAt (e.x);
    dragRow = row;
    dragStartY = (float) e.y;
    dragStartPitch = pool[(size_t) row].pitch;
    if (dragging == Col::enable) { pool[(size_t) row].enabled = ! pool[(size_t) row].enabled; apply (pool); dragging = Col::none; }
    else if (dragging == Col::remove) { pool.erase (pool.begin() + row); apply (pool); dragging = Col::none; }
    else mouseDrag (e);
}

void PoolView::mouseDrag (const juce::MouseEvent& e)
{
    auto pool = proc.pool();
    if (dragRow < 0 || dragRow >= (int) pool.size()) return;
    auto& x = pool[(size_t) dragRow];
    if (dragging == Col::pitch)
    {
        x.pitch = std::clamp (dragStartPitch + (int) std::lround ((dragStartY - e.y) / 6.0f), 0, 127);
        apply (pool);
    }
    else if (dragging == Col::weight || dragging == Col::probability)
    {
        const auto c = cell (dragRow, dragging).reduced (4, 0);
        const float v = std::clamp ((float) (e.x - c.getX()) / std::max (1, c.getWidth()), 0.0f, 1.0f);
        (dragging == Col::weight ? x.weight : x.probability) = std::round (v * 100.0f) / 100.0f;
        apply (pool);
    }
}

void PoolView::mouseUp (const juce::MouseEvent&) { dragging = Col::none; dragRow = -1; }

void PoolView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    auto pool = proc.pool();
    const int row = scroll + (e.y - 18) / rowHeight();
    if (colAt (e.x) == Col::pitch && row >= 0 && row < (int) pool.size())
    {
        pool[(size_t) row].pitch = std::clamp (pool[(size_t) row].pitch + (w.deltaY > 0 ? 1 : -1), 0, 127);
        apply (pool);
        return;
    }
    const int visible = std::max (1, (getHeight() - 18) / rowHeight());
    scroll = std::clamp (scroll + (w.deltaY > 0 ? -1 : 1), 0, std::max (0, (int) pool.size() - visible));
    repaint();
}

// ---------------------------------------------------------------------------------------------- history
HistoryStrip::HistoryStrip (FillMakerProcessor& p) : proc (p) {}

int HistoryStrip::first() const noexcept
{
    const int fit = std::max (1, getWidth() / cellW);
    return std::max (0, proc.history().size() - fit);
}

int HistoryStrip::indexAt (int x) const noexcept
{
    const int i = first() + x / cellW;
    return i < proc.history().size() ? i : -1;
}

void HistoryStrip::paint (juce::Graphics& g)
{
    const auto& h = proc.history();
    g.setColour (ui::raised);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    for (int i = first(), k = 0; i < h.size(); ++i, ++k)
    {
        const auto r = juce::Rectangle<float> ((float) (k * cellW), 0.0f, (float) cellW, (float) getHeight()).reduced (3.0f, 6.0f);
        const auto& e = h.at (i);
        const bool cur = i == h.cursor();
        if (e.generated)
        {
            g.setColour (cur ? ui::accent : ui::accent.withAlpha (0.35f));
            g.fillRoundedRectangle (r, 2.0f);
        }
        else
        {
            g.setColour (cur ? ui::cool : ui::cool.withAlpha (0.35f));
            g.fillEllipse (r.withSizeKeepingCentre (5.0f, 5.0f));
        }
        if (cur) { g.setColour (ui::text); g.drawRoundedRectangle (r.expanded (1.5f), 2.0f, 1.0f); }
    }
}

void HistoryStrip::mouseDown (const juce::MouseEvent& e)
{
    if (const int i = indexAt (e.x); i >= 0) proc.jumpToHistory (i);
    repaint();
}

void HistoryStrip::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.x);
    setTooltip (i >= 0 ? juce::String (i + 1) + ". " + proc.history().at (i).label + (proc.history().at (i).seed ? "  (seed " + juce::String::toHexString ((juce::int64) proc.history().at (i).seed).toUpperCase() + ")" : juce::String()) : juce::String());
}

void HistoryStrip::refresh()
{
    if (proc.history().size() != shownSize || proc.history().cursor() != shownCursor)
    {
        shownSize = proc.history().size();
        shownCursor = proc.history().cursor();
        repaint();
    }
}

// ---------------------------------------------------------------------------------------------- variation
VariationPanel::VariationPanel (FillMakerProcessor& p)
    : proc (p),
      preservation (p, "preservation", "Preservation", "Mutate keeps this much of the fill: 100% very subtle, 0% close to a new one", ui::accent, true),
      mTiming (p, "mTiming", "Timing", "Mutate: nudges and displacements"),
      mVelocity (p, "mVelocity", "Velocity", "Mutate: dynamics and accents"),
      mPitch (p, "mPitch", "Pitch", "Mutate: notes moved to other pitches"),
      mDensity (p, "mDensity", "Density", "Mutate: notes removed and added"),
      mStructure (p, "mStructure", "Structure", "Mutate: beats rearranged, motifs permuted"),
      mSubdivision (p, "mSubdivision", "Subdiv", "Mutate: a beat moved to another grid, a note turned into a ratchet"),
      mProbability (p, "mProbability", "Chance", "Mutate: some notes made to play only sometimes"),
      pool (p),
      pitchLow (p, "pitchLow", "Lowest", "Nothing below this pitch"),
      pitchHigh (p, "pitchHigh", "Highest", "Nothing above this pitch"),
      motion (p, "motion", "Motion", "How pitches move: Auto picks per phrase, by Behaviour"),
      grooveAware (p, "grooveAware", "Groove-aware", "Fills answer the groove: its pitches, its spaces, its feel, even its motifs", ui::accent),
      grooveStrength (p, "grooveStrength", "Strength", "How closely the fill relates to the groove", ui::accent, true),
      euclidSteps (p, "euclidSteps", "Steps", "Euclidean steps (Auto: from the grid, or odd counts when complex)"),
      euclidPulses (p, "euclidPulses", "Pulses", "Euclidean pulses (Auto: from Density)"),
      euclidRotation (p, "euclidRotation", "Rotate", "Euclidean rotation"),
      channel (p, "channel", "Channel", "The MIDI channel Fill Maker plays on"),
      ratchetMax (p, "ratchetMax", "Ratchet max", "The largest ratchet allowed (2 to 13 hits)"),
      poly (p, "polyRatio", "Polyrhythm", "Competing cycles (Auto chooses by complexity and behaviour)"),
      preview (p, "preview", "Preview sound", "The built-in voice: off, for Audition only, or also for what plays in Live"),
      thru (p, "midiThru", "MIDI thru", "Pass the track's MIDI (your groove) on to the drum track as well"),
      auditionMidi (p, "auditionMidi", "Audition to MIDI out", "Audition also plays your real kit (the drum track), not just the preview voice"),
      seedLock (p, "seedLock", "Lock", "Generate keeps this seed (repeatable: the same settings give the same fill)", ui::accent),
      history (p)
{
    static const char* const names[] = { "Variation", "Pitch", "Groove", "More" };
    for (int i = 0; i < 4; ++i)
    {
        auto& t = tabs[(size_t) i];
        t.setButtonText (names[i]);
        t.setClickingTogglesState (true);
        t.setRadioGroupId (77);
        t.setColour (juce::TextButton::buttonColourId, ui::bg);
        t.setColour (juce::TextButton::buttonOnColourId, ui::raised);
        t.setColour (juce::TextButton::textColourOffId, ui::faint);
        t.setColour (juce::TextButton::textColourOnId, ui::text);
        t.onClick = [this, i] { if (tabs[(size_t) i].getToggleState()) showTab (i); };
        addAndMakeVisible (t);
    }
    for (int e = 0; e < kNumEngines; ++e)
    {
        static const char* const tips[] = { "Weighted chances per step: metric strength, density, what came before",
                                            "Evenly spread pulses, on steps that needn't match the bar",
                                            "Repeats inside an interval: 2 to 13 hits, irregular groupings, ramps",
                                            "A motif rearranged: rotated, reversed, intervals shuffled, fragmented",
                                            "Competing cycles: 3:4, 5:4, 7:8, 11:16 ..., across bar lines" };
        engineChips.add (new Chip (p, FillMakerProcessor::eid (e), kEngineNames[(size_t) e], tips[e]));
        addChildComponent (engineChips.getLast());
    }
    for (int g = 0; g < kNumGrammars; ++g)
    {
        static const char* const tips[] = { "Density rises through the fill", "Starts dense, thins out", "Concentrated clusters", "A motif, then an answer to it",
                                            "Intervals shorten", "Intervals lengthen", "Unexpected gaps", "Rhythmic and dynamic intensity rise",
                                            "Events pulled off their expected positions, more as it goes", "Motifs broken into pieces" };
        grammarChips.add (new Chip (p, FillMakerProcessor::gid (g), kGrammarNames[(size_t) g], tips[g], ui::accent));
        addChildComponent (grammarChips.getLast());
    }
    for (auto* c : std::initializer_list<juce::Component*> { &preservation, &mTiming, &mVelocity, &mPitch, &mDensity, &mStructure, &mSubdivision, &mProbability,
                                                              &pool, &pitchLow, &pitchHigh, &motion, &addPitch, &resetPool, &fromPattern,
                                                              &grooveAware, &grooveStrength, &follow, &capture, &importMidi, &clearGroove, &grooveText,
                                                              &euclidSteps, &euclidPulses, &euclidRotation, &channel, &ratchetMax, &poly, &preview, &thru, &auditionMidi })
        addChildComponent (*c);
    addPitch.onClick = [this] {
        auto pl = proc.pool();
        int next = pl.empty() ? 36 : pl.back().pitch + 1;
        pl.push_back ({ std::clamp (next, 0, 127), 0.5f, 1.0f, true });
        proc.setPool (pl);
        pool.repaint();
    };
    resetPool.onClick = [this] { proc.setPool (defaultPool()); pool.repaint(); };
    resetPool.setTooltip ("The first 16 pads of a Drum Rack (36-51), the first 8 on");
    fromPattern.onClick = [this] {
        std::vector<PitchEntry> pl;
        std::map<int, int> count;
        for (const auto& e : proc.pattern().events) count[e.pitch]++;
        int top = 1;
        for (auto [pt, n] : count) top = std::max (top, n);
        for (auto [pt, n] : count) pl.push_back ({ pt, std::max (0.1f, (float) n / top), 1.0f, true });
        if (! pl.empty()) { proc.setPool (pl); pool.repaint(); }
    };
    fromPattern.setTooltip ("The pitches the fill on screen uses, weighted by how often");
    follow.setClickingTogglesState (true);
    follow.setColour (juce::TextButton::buttonOnColourId, ui::cool.withAlpha (0.85f));
    follow.setColour (juce::TextButton::textColourOnId, ui::bg);
    follow.setTooltip ("Keep the groove updated from the MIDI this track plays (its last two whole bars)");
    follow.onClick = [this] { proc.followInput = follow.getToggleState(); };
    capture.onClick = [this] { proc.captureGroove(); };
    capture.setTooltip ("Take the last two whole bars heard on MIDI in as the groove, now");
    importMidi.setTooltip ("A drum pattern from a MIDI file as the groove (for when MIDI in isn't available)");
    importMidi.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Import a groove (MIDI file)", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.mid;*.midi");
        juce::Component::SafePointer<VariationPanel> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc) {
            if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
            juce::String err;
            if (! safe->proc.importGroove (fc.getResult(), &err)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import", err);
            safe->refresh();
        });
    };
    clearGroove.onClick = [this] { proc.clearGroove(); refresh(); };
    grooveText.setFont (juce::FontOptions (12.0f));
    grooveText.setColour (juce::Label::textColourId, ui::dim);
    grooveText.setJustificationType (juce::Justification::topLeft);
    seedLabel.setFont (ui::mono (13.0f, true));
    seedLabel.setColour (juce::Label::textColourId, ui::text);
    seedLabel.setColour (juce::Label::backgroundColourId, ui::raised);
    seedLabel.setEditable (false, true);
    seedLabel.setTooltip ("The seed: double-click to type one (hex) and hear that fill again");
    seedLabel.onTextChange = [this] {
        const auto v = (uint32_t) seedLabel.getText().retainCharacters ("0123456789abcdefABCDEF").getHexValue64();
        if (v != 0) { proc.setSeed (v); proc.regenerate(); }
    };
    addAndMakeVisible (seedLabel);
    addAndMakeVisible (seedLock);
    addAndMakeVisible (history);
    historyText.setFont (ui::mono (10.5f));
    historyText.setColour (juce::Label::textColourId, ui::dim);
    addAndMakeVisible (historyText);
    for (auto* b : { &undo, &redo }) { styleAction (*b, ui::raised, ui::text); addAndMakeVisible (*b); }
    undo.setTooltip ("Undo (Cmd+Z)");
    redo.setTooltip ("Redo (Cmd+Shift+Z)");
    tabs[0].setToggleState (true, juce::dontSendNotification);
    showTab (0);
}

std::vector<juce::Component*> VariationPanel::tabComponents (int t)
{
    switch (t)
    {
        case 0:
        {
            std::vector<juce::Component*> v { &preservation, &mTiming, &mVelocity, &mPitch, &mDensity, &mStructure, &mSubdivision, &mProbability };
            for (auto* c : engineChips) v.push_back (c);
            for (auto* c : grammarChips) v.push_back (c);
            return v;
        }
        case 1: return { &pool, &pitchLow, &pitchHigh, &motion, &addPitch, &resetPool, &fromPattern };
        case 2: return { &grooveAware, &grooveStrength, &follow, &capture, &importMidi, &clearGroove, &grooveText };
        default: return { &euclidSteps, &euclidPulses, &euclidRotation, &channel, &ratchetMax, &poly, &preview, &thru, &auditionMidi };
    }
}

void VariationPanel::showTab (int t)
{
    tab = t;
    for (int i = 0; i < 4; ++i)
    {
        tabs[(size_t) i].setToggleState (i == t, juce::dontSendNotification);
        for (auto* c : tabComponents (i)) c->setVisible (i == t);
    }
    resized();
    repaint();
}

void VariationPanel::refresh()
{
    history.refresh();
    const auto seedText = juce::String::toHexString ((juce::int64) proc.seed()).toUpperCase().paddedLeft ('0', 8);
    if (! seedLabel.isBeingEdited() && seedLabel.getText() != seedText) seedLabel.setText (seedText, juce::dontSendNotification);
    undo.setEnabled (proc.history().canUndo());
    redo.setEnabled (proc.history().canRedo());
    const auto* cur = proc.history().current();
    const juce::String ht = cur != nullptr ? juce::String (proc.history().cursor() + 1) + "/" + juce::String (proc.history().size()) + "  " + cur->label : juce::String();
    if (historyText.getText() != ht) historyText.setText (ht, juce::dontSendNotification);
    follow.setToggleState (proc.followInput, juce::dontSendNotification);
    const auto gs = proc.grooveStatus();
    if (grooveText.getText() != gs) { grooveText.setText (gs, juce::dontSendNotification); repaint (grooveMap); }
}

void VariationPanel::paint (juce::Graphics& g)
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    for (const auto& [r, t] : heads) heading (g, r, t);
    if (tab == 2 && ! grooveMap.isEmpty())
    {
        // The groove's hits through one bar.
        const auto& gr = proc.groove();
        g.setColour (ui::raised);
        g.fillRoundedRectangle (grooveMap.toFloat(), 3.0f);
        if (gr.valid())
        {
            const int n = gr.slots();
            for (int i = 0; i < n; ++i)
            {
                const float x = grooveMap.getX() + 4.0f + (grooveMap.getWidth() - 8.0f) * i / n;
                const float h = (float) gr.onset[(size_t) i] * (grooveMap.getHeight() - 8.0f);
                if (i % GrooveProfile::kSlotsPerBeat == 0) { g.setColour (ui::line); g.drawVerticalLine ((int) x, (float) grooveMap.getY() + 2, (float) grooveMap.getBottom() - 2); }
                if (h > 0.5f) { g.setColour (ui::accent); g.fillRect (x, grooveMap.getBottom() - 4.0f - h, 3.0f, h); }
            }
        }
    }
    g.setColour (ui::faint);
    g.setFont (ui::label (10.0f));
    g.drawText ("SEED", bottom.withHeight (24), juce::Justification::centredLeft);
}

void VariationPanel::resized()
{
    heads.clear();
    auto r = getLocalBounds().reduced (10);
    auto tabRow = r.removeFromTop (26);
    const int tw = tabRow.getWidth() / 4;
    for (int i = 0; i < 4; ++i) tabs[(size_t) i].setBounds (i == 3 ? tabRow : tabRow.removeFromLeft (tw));
    bottom = r.removeFromBottom (104);
    content = r.withTrimmedTop (8);
    {
        auto b = bottom.withTrimmedTop (8);
        auto row = b.removeFromTop (24);
        row.removeFromLeft (40);
        seedLock.setBounds (row.removeFromRight (56));
        row.removeFromRight (6);
        seedLabel.setBounds (row);
        b.removeFromTop (8);
        historyText.setBounds (b.removeFromTop (16));
        history.setBounds (b.removeFromTop (22));
        b.removeFromTop (6);
        auto ur = b.removeFromTop (24);
        undo.setBounds (ur.removeFromLeft (ur.getWidth() / 2).withTrimmedRight (3));
        redo.setBounds (ur.withTrimmedLeft (3));
    }
    auto c = content;
    const auto head = [&] (const char* t) { heads.push_back ({ c.removeFromTop (16), t }); };
    const int w4 = c.getWidth() / 4;
    if (tab == 0)
    {
        auto row = c.removeFromTop (96);
        preservation.setBounds (row.removeFromLeft (110));
        head ("Mutation");
        row = c.removeFromTop (62);
        for (auto* k : { &mTiming, &mVelocity, &mPitch, &mDensity }) k->setBounds (row.removeFromLeft (w4));
        row = c.removeFromTop (62);
        for (auto* k : { &mStructure, &mSubdivision, &mProbability }) k->setBounds (row.removeFromLeft (w4));
        c.removeFromTop (4);
        head ("Engines");
        row = c.removeFromTop (24);
        const int w3 = c.getWidth() / 3;
        for (int i = 0; i < 3; ++i) engineChips[i]->setBounds (row.removeFromLeft (w3).reduced (2, 1));
        row = c.removeFromTop (24);
        for (int i = 3; i < 5; ++i) engineChips[i]->setBounds (row.removeFromLeft (w3).reduced (2, 1));
        c.removeFromTop (4);
        head ("Grammars");
        const int w2 = c.getWidth() / 2;
        for (int i = 0; i < kNumGrammars; i += 2)
        {
            row = c.removeFromTop (23);
            grammarChips[i]->setBounds (row.removeFromLeft (w2).reduced (2, 1));
            grammarChips[i + 1]->setBounds (row.reduced (2, 1));
        }
    }
    else if (tab == 1)
    {
        auto row = c.removeFromTop (70);
        pitchLow.setBounds (row.removeFromLeft (w4));
        pitchHigh.setBounds (row.removeFromLeft (w4));
        motion.setBounds (row.withSizeKeepingCentre (row.getWidth() - 8, 40));
        head ("Pitch pool");
        auto buttons = c.removeFromBottom (26);
        const int bw = buttons.getWidth() / 3;
        addPitch.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
        resetPool.setBounds (buttons.removeFromLeft (bw).reduced (2, 0));
        fromPattern.setBounds (buttons.reduced (2, 0));
        c.removeFromBottom (6);
        pool.setBounds (c);
    }
    else if (tab == 2)
    {
        auto row = c.removeFromTop (28);
        grooveAware.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 2));
        follow.setBounds (row.reduced (2, 2));
        row = c.removeFromTop (100);
        grooveStrength.setBounds (row.removeFromLeft (110));
        auto right = row.reduced (6, 10);
        capture.setBounds (right.removeFromTop (24));
        right.removeFromTop (6);
        importMidi.setBounds (right.removeFromTop (24));
        right.removeFromTop (6);
        clearGroove.setBounds (right.removeFromTop (24));
        head ("Groove heard");
        grooveMap = c.removeFromTop (46);
        c.removeFromTop (6);
        grooveText.setBounds (c.removeFromTop (80));
    }
    else
    {
        head ("Euclidean");
        auto row = c.removeFromTop (66);
        euclidSteps.setBounds (row.removeFromLeft (w4));
        euclidPulses.setBounds (row.removeFromLeft (w4));
        euclidRotation.setBounds (row.removeFromLeft (w4));
        head ("Ratchet and polyrhythm");
        row = c.removeFromTop (44);
        ratchetMax.setBounds (row.removeFromLeft (row.getWidth() / 2).withTrimmedRight (4));
        poly.setBounds (row.withTrimmedLeft (4));
        c.removeFromTop (8);
        head ("MIDI out and preview");
        row = c.removeFromTop (66);
        channel.setBounds (row.removeFromLeft (w4));
        preview.setBounds (row.withSizeKeepingCentre (row.getWidth() - 8, 40));
        row = c.removeFromTop (26);
        thru.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 1));
        auditionMidi.setBounds (row.reduced (2, 1));
    }
}
} // namespace fm
