#include "plugin/PluginEditor.h"

namespace fm
{
// ---------------------------------------------------------------------------------------------- drag
void DragHandle::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (dragging ? ui::accent.withAlpha (0.25f) : ui::raised);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ui::accent.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    // Grip dots, then the words.
    for (int i = 0; i < 3; ++i) for (int k = 0; k < 2; ++k) g.fillEllipse (8.0f + k * 5.0f, r.getCentreY() - 6.0f + i * 5.0f, 2.5f, 2.5f);
    g.setColour (ui::text);
    g.setFont (ui::label (10.5f));
    g.drawText ("DRAG MIDI", getLocalBounds().withTrimmedLeft (18), juce::Justification::centred);
}

void DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4) return;
    dragging = true;
    repaint();
    const auto f = proc.writeDragFile();
    if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; repaint(); });
}

// ---------------------------------------------------------------------------------------------- editor
FillMakerEditor::FillMakerEditor (FillMakerProcessor& p)
    : AudioProcessorEditor (p), proc (p), generation (p), variation (p), roll (p),
      loop (p, "auditionLoop", "Loop", "Audition loops until you stop it"),
      playMode (p, "playMode", "In Live", "With Live playing: Off, loop the fill, or play it at the end of every phrase"),
      phrase (p, "phrase", "Phrase", "How long a phrase is: the fill plays in its last bars"),
      drag (p)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);
    for (auto* c : std::initializer_list<juce::Component*> { &generation, &variation, &roll, &presets, &savePreset, &prevPreset, &nextPreset, &audition,
                                                              &zoomIn, &zoomOut, &zoomFit, &exportMidi, &fileButton, &loop, &playMode, &phrase, &drag, &structure, &status })
        addAndMakeVisible (*c);
    generation.generate.onClick = [this] { proc.generate(); };
    generation.regenerate.onClick = [this] { proc.regenerate(); };
    generation.mutate.onClick = [this] { doMutate(); };
    variation.undo.onClick = [this] { proc.undo(); };
    variation.redo.onClick = [this] { proc.redo(); };
    styleAction (audition, ui::cool, ui::bg);
    audition.getProperties().set ("fontSize", 12.0);
    audition.setTooltip ("Hear the fill now, without starting Live (Space)");
    audition.onClick = [this] { proc.auditioning() ? proc.stopAudition() : proc.audition(); };
    zoomIn.onClick = [this] { roll.zoomBy (1.4); };
    zoomOut.onClick = [this] { roll.zoomBy (1.0 / 1.4); };
    zoomFit.onClick = [this] { roll.fit(); };
    for (auto* b : { &zoomIn, &zoomOut, &zoomFit, &savePreset, &prevPreset, &nextPreset, &exportMidi, &fileButton }) styleAction (*b, ui::raised, ui::text);
    exportMidi.setTooltip ("Save the fill as a standard MIDI file (timing, lengths, pitch, velocity, tempo)");
    exportMidi.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Export MIDI", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fill " + juce::String::toHexString ((juce::int64) proc.seed()).toUpperCase() + ".mid"), "*.mid");
        juce::Component::SafePointer<FillMakerEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [safe] (const juce::FileChooser& fc) {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            juce::String err;
            auto f = fc.getResult().withFileExtension ("mid");
            if (! safe->proc.exportMidi (f, 1, &err)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export MIDI", err);
            else safe->status.setText ("Exported " + f.getFileName(), juce::dontSendNotification);
        });
    };
    fileButton.setTooltip ("Save or load a pattern, or export several repeats");
    fileButton.onClick = [this] { fileMenu(); };
    savePreset.setTooltip ("Save these settings (pitch pool too, and optionally the pattern) as a user preset");
    savePreset.onClick = [this] { savePresetDialog(); };
    prevPreset.onClick = [this] { const int n = presets.getNumItems(); if (n > 0) presets.setSelectedItemIndex ((presets.getSelectedItemIndex() - 1 + n) % n); };
    nextPreset.onClick = [this] { const int n = presets.getNumItems(); if (n > 0) presets.setSelectedItemIndex ((presets.getSelectedItemIndex() + 1) % n); };
    presets.setTooltip ("Factory and user presets. Factory presets keep your pitch pool");
    presets.onChange = [this] {
        const int id = presets.getSelectedId();
        if (id >= 1 && id <= (int) factoryPresets().size()) proc.applyPreset (factoryPresets()[(size_t) id - 1]);
        else if (id > 1000 && id - 1001 < (int) userPresetFiles.size())
        {
            juce::String err;
            if (! proc.loadUserPreset (userPresetFiles[(size_t) (id - 1001)], &err)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", err);
        }
        shownPreset = proc.presetName();
    };
    buildPresetMenu();
    structure.setFont (ui::mono (11.0f));
    structure.setColour (juce::Label::textColourId, ui::dim);
    structure.setMinimumHorizontalScale (0.7f);
    status.setFont (ui::mono (11.0f));
    status.setColour (juce::Label::textColourId, ui::faint);
    status.setJustificationType (juce::Justification::centredRight);
    spotlight = std::make_unique<lvrs::Spotlight> (p, *this);
    spotlight->onReveal = [this] (const juce::String&, juce::Component& c) {
        if (const int t = variation.tabOf (c); t >= 0) variation.showTab (t);
    };
    addAndMakeVisible (*spotlight);
    setSize (1240, 760);
    startTimerHz (30);
}

FillMakerEditor::~FillMakerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FillMakerEditor::buildPresetMenu()
{
    presets.clear (juce::dontSendNotification);
    auto* root = presets.getRootMenu();
    juce::String cat;
    const auto& f = factoryPresets();
    for (size_t i = 0; i < f.size(); ++i)
    {
        if (f[i].category != cat) { cat = f[i].category; root->addSectionHeader (cat.toUpperCase()); }
        root->addItem ((int) i + 1, f[i].name);
    }
    userPresetFiles.clear();
    for (const auto& file : listUserPresets (userPresetFolder())) userPresetFiles.push_back (file);
    if (! userPresetFiles.empty())
    {
        root->addSectionHeader ("USER");
        for (size_t i = 0; i < userPresetFiles.size(); ++i) root->addItem (1001 + (int) i, userPresetFiles[i].getFileNameWithoutExtension());
    }
    for (int i = 0; i < presets.getNumItems(); ++i)
        if (presets.getItemText (i) == proc.presetName()) { presets.setSelectedItemIndex (i, juce::dontSendNotification); break; }
    shownPreset = proc.presetName();
}

void FillMakerEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "A name for these settings:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presetName() == "Init" ? juce::String ("My fill") : proc.presetName());
    w->addComboBox ("pattern", { "Settings only", "Settings and this pattern" });
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<FillMakerEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r) {
        if (safe != nullptr && r == 1)
        {
            juce::String err;
            const auto name = w->getTextEditorContents ("name");
            const bool withPattern = w->getComboBoxComponent ("pattern")->getSelectedItemIndex() == 1;
            if (! safe->proc.saveUserPreset (name, withPattern, &err)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", err);
            safe->buildPresetMenu();
        }
    }), true);
}

void FillMakerEditor::fileMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save pattern...");
    m.addItem (2, "Load pattern...");
    m.addSeparator();
    m.addItem (3, "Export MIDI, 2 repeats...");
    m.addItem (4, "Export MIDI, 4 repeats...");
    m.addSeparator();
    m.addItem (5, "Show user presets folder");
    juce::Component::SafePointer<FillMakerEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileButton), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& ed = *safe;
        const auto folder = userPresetFolder().getParentDirectory().getChildFile ("Patterns");
        if (r == 5) { userPresetFolder().createDirectory(); userPresetFolder().revealToUser(); return; }
        const bool save = r != 2;
        const juce::String wild = r >= 3 ? "*.mid" : "*.fillpattern";
        folder.createDirectory();
        ed.chooser = std::make_unique<juce::FileChooser> (r == 1 ? "Save pattern" : r == 2 ? "Load pattern" : "Export MIDI",
                                                          r >= 3 ? juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fill.mid") : folder.getChildFile ("Fill.fillpattern"), wild);
        ed.chooser->launchAsync ((save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode)
                                     | juce::FileBrowserComponent::canSelectFiles,
                                 [safe, r] (const juce::FileChooser& fc) {
                                     if (safe == nullptr || fc.getResult() == juce::File()) return;
                                     juce::String err;
                                     bool ok = true;
                                     if (r == 1) ok = safe->proc.savePattern (fc.getResult().withFileExtension ("fillpattern"), &err);
                                     else if (r == 2) ok = safe->proc.loadPattern (fc.getResult(), &err);
                                     else ok = safe->proc.exportMidi (fc.getResult().withFileExtension ("mid"), r == 3 ? 2 : 4, &err);
                                     if (! ok) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Pattern", err);
                                 });
    });
}

void FillMakerEditor::doMutate()
{
    if (! roll.selection().empty()) proc.mutate (0.0, -1.0, roll.selection());
    else if (roll.hasRegion()) proc.mutate (roll.regionStart(), roll.regionEnd());
    else proc.mutate();
}

bool FillMakerEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.redo(); return true; }
    if (k.getModifiers().isAnyModifierKeyDown()) return false;
    switch (juce::CharacterFunctions::toLowerCase (k.getTextCharacter()))
    {
        case 'g': proc.generate(); return true;
        case 'r': proc.regenerate(); return true;
        case 'm': doMutate(); return true;
        case ' ': proc.auditioning() ? proc.stopAudition() : proc.audition(); return true;
        default: return false;
    }
}

void FillMakerEditor::timerCallback()
{
    roll.refresh();
    generation.refresh();
    variation.refresh();
    const bool on = proc.auditioning();
    audition.setButtonText (on ? "Stop" : "Audition");
    styleAction (audition, on ? ui::warn : ui::cool, ui::bg);
    phrase.setVisible (juce::roundToInt (proc.state().getRawParameterValue ("playMode")->load()) == 2);
    if (structure.getText() != proc.structure()) { structure.setText (proc.structure(), juce::dontSendNotification); structure.setTooltip (proc.structure()); }
    const auto& pt = proc.pattern();
    const double bars = pt.length / pt.beatsPerBar;
    const juce::String st = juce::String ((int) pt.events.size()) + " notes   " + juce::String (bars, bars == std::floor (bars) ? 0 : 2) + (bars == 1.0 ? " bar" : " bars")
                          + "   " + juce::String (proc.hostBpm(), 1) + " BPM";
    if (status.getText() != st && ! status.getText().startsWith ("Exported")) status.setText (st, juce::dontSendNotification);
    if (proc.presetName() != shownPreset) buildPresetMenu();
    repaint (top);
}

void FillMakerEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    // The wordmark.
    g.setColour (ui::accent);
    g.fillEllipse (16.0f, (float) top.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour (ui::text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold).withKerningFactor (0.2f));
    g.drawText ("FILL MAKER", 34, top.getY(), 200, top.getHeight(), juce::Justification::centredLeft);
    g.setColour (ui::faint);
    g.setFont (ui::label (9.5f));
    g.drawText ("GENERATIVE DRUM FILLS", 188, top.getY() + 2, 200, top.getHeight(), juce::Justification::centredLeft);
    g.drawText ("LEVERSTUDIOS", getWidth() - 140, top.getY(), 124, top.getHeight(), juce::Justification::centredRight);
    // The pattern panel.
    g.setColour (ui::panel);
    g.fillRoundedRectangle (centre.toFloat(), 6.0f);
    heading (g, centre.reduced (10, 0).withHeight (20).translated (0, 6), "Pattern");
}

void FillMakerEditor::resized()
{
    if (spotlight != nullptr) spotlight->setBounds (getLocalBounds());
    auto r = getLocalBounds().reduced (10, 6);
    top = r.removeFromTop (38);
    {
        auto t = top.withTrimmedLeft (330).withTrimmedRight (150);
        auto right = t.removeFromRight (std::min (t.getWidth(), 460));
        savePreset.setBounds (right.removeFromRight (100).reduced (0, 6));
        right.removeFromRight (6);
        nextPreset.setBounds (right.removeFromRight (26).reduced (0, 6));
        prevPreset.setBounds (right.removeFromRight (26).reduced (0, 6));
        right.removeFromRight (4);
        presets.setBounds (right.reduced (0, 6));
    }
    r.removeFromTop (4);
    generation.setBounds (r.removeFromLeft (300));
    r.removeFromLeft (8);
    variation.setBounds (r.removeFromRight (320));
    r.removeFromRight (8);
    centre = r;
    auto c = r.reduced (10);
    auto head = c.removeFromTop (22).withTrimmedLeft (70);
    zoomFit.setBounds (head.removeFromRight (40));
    zoomIn.setBounds (head.removeFromRight (26).withTrimmedLeft (2));
    zoomOut.setBounds (head.removeFromRight (26).withTrimmedLeft (2));
    head.removeFromRight (8);
    structure.setBounds (head);
    c.removeFromTop (6);
    auto foot = c.removeFromBottom (78);
    roll.setBounds (c);
    // Row 1: hearing it. Row 2: taking it away.
    foot.removeFromTop (6);
    auto row = foot.removeFromTop (38);
    audition.setBounds (row.removeFromLeft (124).withTrimmedTop (4));
    row.removeFromLeft (6);
    loop.setBounds (row.removeFromLeft (58).withTrimmedTop (9).withTrimmedBottom (3));
    row.removeFromLeft (16);
    playMode.setBounds (row.removeFromLeft (112));
    row.removeFromLeft (6);
    phrase.setBounds (row.removeFromLeft (124));
    foot.removeFromTop (6);
    row = foot.removeFromTop (28);
    drag.setBounds (row.removeFromRight (116));
    row.removeFromRight (6);
    exportMidi.setBounds (row.removeFromRight (100));
    row.removeFromRight (6);
    fileButton.setBounds (row.removeFromRight (80));
    row.removeFromRight (8);
    status.setBounds (row);
}
} // namespace fm
