#include "playback/PreviewSynth.h"

#include <algorithm>
#include <cmath>

namespace fm
{
void PreviewSynth::prepare (double sampleRate) noexcept
{
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    allOff();
}

void PreviewSynth::allOff() noexcept
{
    for (auto& v : voices_) v = Voice {};
}

void PreviewSynth::start (int pitch, int velocity) noexcept
{
    // The same pitch restarts its voice; otherwise a free voice, or the oldest.
    Voice* v = nullptr;
    for (auto& x : voices_) if (x.active && x.pitch == pitch) { v = &x; break; }
    if (v == nullptr) for (auto& x : voices_) if (! x.active) { v = &x; break; }
    if (v == nullptr)
    {
        v = &voices_[0];
        for (auto& x : voices_) if (x.age < v->age) v = &x;
    }
    const double x = std::clamp ((pitch - 24) / 72.0, 0.0, 1.0);   // 0 (C0) .. 1 (C6) in Live's names
    const double vel = std::clamp (velocity / 127.0, 0.0, 1.0);
    const auto coef = [this] (double seconds) { return 1.0 - std::exp (-1.0 / (std::max (1.0e-4, seconds) * sr_)); };
    *v = Voice {};
    v->active = true;
    v->pitch = pitch;
    v->age = ++ages_;
    v->freq = 42.0 * std::pow (2.0, x * 5.6);                       // 42 Hz .. ~2 kHz
    v->sweep = 2.6 * (1.0 - x) * (1.0 - x) + 0.2;                    // low notes drop in pitch (a kick's punch)
    v->sweepK = coef (0.006 + 0.01 * (1.0 - x));
    v->noiseMix = std::clamp ((x - 0.3) / 0.5, 0.0, 1.0);
    v->toneLevel = 1.0 - 0.75 * v->noiseMix;
    v->decayK = coef ((0.26 - 0.21 * x) * (0.55 + 0.45 * vel));
    v->releaseK = coef (0.025);
    v->level = 0.55 * std::pow (vel, 1.5);
    v->env = 1.0;
    v->hpK = coef (1.0 / (2.0 * 3.14159 * (700.0 + 7000.0 * x)));
    v->lpK = coef (1.0 / (2.0 * 3.14159 * (3000.0 + 12000.0 * vel)));
}

void PreviewSynth::release (int pitch) noexcept
{
    for (auto& v : voices_) if (v.active && v.pitch == pitch) v.released = true;
}

float PreviewSynth::tick (Voice& v) noexcept
{
    v.sweep += (0.0 - v.sweep) * v.sweepK;
    v.phase += v.freq * (1.0 + v.sweep) / sr_;
    v.phase -= std::floor (v.phase);
    const double tone = std::sin (6.283185307179586 * v.phase);
    noise_ ^= noise_ << 13;
    noise_ ^= noise_ >> 17;
    noise_ ^= noise_ << 5;
    const double wn = (double) noise_ * (2.0 / 4294967296.0) - 1.0;
    v.lp += (wn - v.lp) * v.lpK;                   // brightness by velocity
    v.hp += (v.lp - v.hp) * v.hpK;                 // then high-passed by pitch
    const double noise = v.lp - v.hp;
    v.env -= v.env * (v.released ? std::max (v.releaseK, v.decayK) : v.decayK);
    if (v.env < 1.0e-4) v.active = false;
    return (float) (v.level * v.env * (v.toneLevel * tone + v.noiseMix * 1.4 * noise));
}

void PreviewSynth::render (juce::AudioBuffer<float>& out, int offset, int n, const juce::MidiBuffer& events) noexcept
{
    const int chans = out.getNumChannels();
    int pos = 0;
    auto it = events.begin();
    while (pos < n)
    {
        int nextEvent = n;
        while (it != events.end() && (*it).samplePosition <= pos)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn()) start (m.getNoteNumber(), m.getVelocity());
            else if (m.isNoteOff()) release (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff()) allOff();
            ++it;
        }
        if (it != events.end()) nextEvent = std::min (n, (*it).samplePosition);
        for (int i = pos; i < std::max (pos + 1, nextEvent) && i < n; ++i)
        {
            float s = 0.0f;
            for (auto& v : voices_) if (v.active) s += tick (v);
            s = std::tanh (s);
            for (int ch = 0; ch < chans; ++ch) out.addSample (ch, offset + i, s);
        }
        pos = std::max (pos + 1, nextEvent);
    }
}
} // namespace fm
