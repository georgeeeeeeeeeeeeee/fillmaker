#pragma once
// Fill Maker: a generative MIDI drum fill instrument.
//
// Threads:
//   message thread  generation, mutation, edits, history, presets, files (everything that allocates)
//   audio thread    reads the newest pattern snapshot (lock-free), plays it to MIDI out in time with
//                   Live (loop, or at the end of each phrase), plays auditions, renders the preview
//                   voice, and passes incoming MIDI through while noting it for groove-aware mode
//
// Live: put it on a MIDI track (its clip, if any, is the groove it hears); on the drum track set MIDI
// From to that track and "Fill Maker". Its audio out is the preview voice only.

#include "engine/Generator.h"
#include "engine/Groove.h"
#include "engine/MidiExport.h"
#include "engine/Mutation.h"
#include "engine/Presets.h"
#include "playback/Player.h"
#include "playback/PreviewSynth.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace fm
{
// Generations and edits in one list: Undo and Redo walk it; the strip in the editor shows it.
class PatternHistory
{
public:
    struct Entry { Pattern pattern; juce::String label, structure; uint32_t seed = 0; bool generated = false; };
    static constexpr int kMax = 64;
    void push (Entry);
    bool canUndo() const noexcept { return cursor_ > 0; }
    bool canRedo() const noexcept { return cursor_ + 1 < (int) list_.size(); }
    const Entry* undo() noexcept;
    const Entry* redo() noexcept;
    const Entry* jump (int index) noexcept;
    const Entry* current() const noexcept { return cursor_ >= 0 && cursor_ < (int) list_.size() ? &list_[(size_t) cursor_] : nullptr; }
    int cursor() const noexcept { return cursor_; }
    int size() const noexcept { return (int) list_.size(); }
    const Entry& at (int i) const { return list_[(size_t) i]; }
    juce::var toVar() const;
    void fromVar (const juce::var&);
    void clear() { list_.clear(); cursor_ = -1; }

private:
    std::vector<Entry> list_;
    int cursor_ = -1;
};

class FillMakerProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    FillMakerProcessor();
    ~FillMakerProcessor() override;

    void prepareToPlay (double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Fill Maker"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program_; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }

    // ── Message thread ─────────────────────────────────────────────────────────
    Settings settings() const;                    // from the parameters, the pool and Live's meter
    MutationSettings mutationSettings() const;
    void generate();                              // a new seed (the same one when the seed is locked)
    void regenerate();                            // the current seed again, with the current settings
    void mutate (double regionStart = 0.0, double regionEnd = -1.0, const std::vector<uint32_t>& onlyIds = {});
    void commitEdit (const Pattern&, const juce::String& label);   // an edit from the pattern editor (undoable)
    void previewEdit (const Pattern&);            // mid-drag: heard, not yet in the history
    void undo();
    void redo();
    void jumpToHistory (int index);
    const Pattern& pattern() const noexcept { return pattern_; }
    const PatternHistory& history() const noexcept { return history_; }
    juce::String structure() const { return structure_; }
    juce::String lastAction() const { return lastAction_; }
    uint32_t seed() const noexcept { return seed_; }
    void setSeed (uint32_t s) noexcept { seed_ = s; }

    void audition();                              // play the pattern now (loops when Loop is on)
    void stopAudition();
    bool auditioning() const noexcept { return auditioning_.load(); }
    double playheadBeat() const noexcept { return playhead_.load(); }   // in the pattern; -1 when not playing

    std::vector<PitchEntry> pool() const { return pool_; }
    void setPool (std::vector<PitchEntry>);

    // Groove-aware mode: notes heard on MIDI in (the last whole bars), or a MIDI file.
    const GrooveProfile& groove() const noexcept { return groove_; }
    juce::String grooveStatus() const;
    bool importGroove (const juce::File&, juce::String* error = nullptr);
    void captureGroove();                         // the last two whole bars heard, now
    void clearGroove();
    bool followInput = true;                      // keep the groove updated from MIDI in

    bool exportMidi (const juce::File&, int repeats = 1, juce::String* error = nullptr) const;
    juce::File writeDragFile() const;             // a .mid in the temp folder, for dragging into Live
    double hostBpm() const noexcept { return bpm_.load(); }
    double beatsPerBar() const noexcept { return beatsPerBar_.load(); }

    // Presets: factory programs, user files.
    void applyPreset (const Preset&);
    bool saveUserPreset (const juce::String& name, bool withPattern, juce::String* error = nullptr);
    bool loadUserPreset (const juce::File&, juce::String* error = nullptr);
    juce::String presetName() const { return presetName_; }
    bool savePattern (const juce::File&, juce::String* error = nullptr) const;
    bool loadPattern (const juce::File&, juce::String* error = nullptr);

    uint32_t patternVersion() const noexcept { return version_; }   // bumps whenever the pattern changes

    static juce::String eid (int engine);    // parameter ids
    static juce::String gid (int grammar);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override;
    void setPattern (const Pattern&, const juce::String& label, bool pushHistory, bool generated);
    void generateAs (const juce::String& label);
    void publish();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    Pattern pattern_;
    PatternHistory history_;
    std::vector<PitchEntry> pool_;
    uint32_t seed_ = 1, version_ = 0;
    juce::String structure_, lastAction_, presetName_ { "Init" };
    int program_ = 0;
    juce::Random seeds_;

    GrooveProfile groove_;
    juce::String grooveSourceName_;
    struct Heard { double ppq; uint8_t pitch, velocity; };
    juce::AbstractFifo heardFifo_ { 2048 };
    std::array<Heard, 2048> heardData_ {};
    std::vector<Heard> heard_;            // message thread: the recent past
    double lastGrooveBar_ = -1.0;

    // Audio thread.
    SnapshotExchange exchange_;
    Player hostPlayer_, auditionPlayer_;
    PreviewSynth synth_;
    juce::MidiBuffer out_, auditionOut_, scratch_;   // sized in prepareToPlay, reused (no allocation per block)
    Rng playRng_ { 0xa11ce };
    double sr_ = 48000.0, auditionBeat_ = 0.0;
    std::atomic<bool> auditioning_ { false }, auditionStart_ { false }, auditionStop_ { false };
    std::atomic<double> playhead_ { -1.0 }, bpm_ { 120.0 }, beatsPerBar_ { 4.0 };
    int lastChannel_ = 1, lastMode_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FillMakerProcessor)
};

// Parameter choice lists the editor shares.
inline constexpr std::array<const char*, 3> kPlayModeNames { "Off", "Loop", "Phrase end" };
inline constexpr std::array<int, 5> kPhraseBars { 1, 2, 4, 8, 16 };
inline constexpr std::array<const char*, 5> kPhraseNames { "Every bar", "Every 2 bars", "Every 4 bars", "Every 8 bars", "Every 16 bars" };
inline constexpr std::array<const char*, 3> kPreviewNames { "Off", "Audition", "Audition + Live" };
} // namespace fm
