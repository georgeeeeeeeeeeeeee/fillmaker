#include "playback/Player.h"

#include <algorithm>
#include <cmath>

namespace fm
{
void Snapshot::fill (Snapshot& s, const Pattern& p, uint32_t version)
{
    s.count = 0;
    s.length = std::max (0.0625, p.length);
    s.version = version;
    for (const auto& e : p.events)
    {
        if (s.count >= kMax) break;
        auto& n = s.notes[(size_t) s.count++];
        n.on = std::clamp (e.time(), 0.0, s.length - 1.0e-9);
        n.off = std::max (n.on + Pattern::kMinLength, e.end());
        n.pitch = (uint8_t) std::clamp (e.pitch, 0, 127);
        n.velocity = (uint8_t) std::clamp (e.velocity, 1, 127);
        n.probability = e.probability;
    }
    std::sort (s.notes.begin(), s.notes.begin() + s.count, [] (const PlayNote& a, const PlayNote& b) { return a.on < b.on; });
}

void SnapshotExchange::publish (const Pattern& p, uint32_t version)
{
    Snapshot::fill (buf_[(size_t) back_], p, version);
    back_ = middle_.exchange (back_ | 4, std::memory_order_acq_rel) & 3;
}

const Snapshot& SnapshotExchange::read() noexcept
{
    if (middle_.load (std::memory_order_acquire) & 4)
        front_ = middle_.exchange (front_, std::memory_order_acq_rel) & 3;
    return buf_[(size_t) front_];
}

// ---------------------------------------------------------------------------------------------- player
void Player::prepare (double sampleRate) noexcept
{
    sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    for (auto& a : active_) a.on = false;
    running_ = false;
    lastEnd_ = -1.0e18;
    position_ = -1.0;
}

bool Player::anyActive() const noexcept
{
    for (const auto& a : active_) if (a.on) return true;
    return false;
}

void Player::noteOff (juce::MidiBuffer& out, int pitch, int sample) noexcept
{
    auto& a = active_[(size_t) pitch];
    if (! a.on) return;
    out.addEvent (juce::MidiMessage::noteOff (a.channel, pitch), std::max (0, sample));
    a.on = false;
}

void Player::stop (juce::MidiBuffer& out, int sample) noexcept
{
    for (int p = 0; p < 128; ++p) noteOff (out, p, sample);
    running_ = false;
    lastEnd_ = -1.0e18;
    position_ = -1.0;
}

void Player::render (const Snapshot& snap, double beat, double bpm, int n, juce::MidiBuffer& out, const Placement& place, Rng& rng) noexcept
{
    if (n <= 0) return;
    bpm = std::clamp (std::isfinite (bpm) ? bpm : 120.0, 1.0, 2000.0);
    const double spb = sr_ * 60.0 / bpm;               // samples per beat
    const double b1 = beat + n / spb;
    const double L = std::max (0.0625, snap.length);

    // Where this block starts playing from: a block that overlaps the last one (the same position
    // processed twice) plays only the new part; a jump back further than a couple of blocks is a loop
    // or a relocation, and starts afresh.
    double start = beat;
    if (running_ && beat < lastEnd_ - 1.0e-9)
    {
        if (lastEnd_ - beat <= 2.0 * n / spb) start = std::min (lastEnd_, b1);
        else finished_ = false;
    }
    if (! running_) finished_ = false;

    // Note-offs due before each note-on are sent at their own sample; the rest at the end.
    const auto offsDueBy = [&] (int64_t at) {
        for (int p = 0; p < 128; ++p)
            if (active_[(size_t) p].on && active_[(size_t) p].offAt <= at)
                noteOff (out, p, (int) (active_[(size_t) p].offAt - clock_));
    };

    // The pattern's occurrences that overlap [start, b1).
    const auto occurrence = [&] (long long k) {
        return place.phraseBeats > 0.0 ? place.anchor + (k + 1) * place.phraseBeats - L : place.anchor + k * L;
    };
    const double period = place.phraseBeats > 0.0 ? std::max (place.phraseBeats, L) : L;
    long long k0 = (long long) std::floor ((start - place.anchor - (place.phraseBeats > 0.0 ? place.phraseBeats - L : 0.0)) / period) - 1;
    long long k1 = (long long) std::floor ((b1 - place.anchor) / period) + 1;
    if (place.once) { k0 = std::max (k0, 0LL); k1 = std::min (k1, 0LL); }
    position_ = -1.0;
    for (long long k = k0; k <= k1; ++k)
    {
        const double occ = occurrence (k);
        if (place.once && k != 0) continue;
        if (b1 > occ && beat < occ + L) position_ = std::clamp (b1 - occ, 0.0, L);
        const double lo = std::max (start, occ) - occ, hi = std::min (b1, occ + L) - occ;
        if (hi <= lo) continue;
        // First note at or after lo (binary search; the snapshot is sorted by onset).
        int i = (int) (std::lower_bound (snap.notes.begin(), snap.notes.begin() + snap.count, lo - 1.0e-12,
                                         [] (const PlayNote& a, double v) { return a.on < v; }) - snap.notes.begin());
        for (; i < snap.count && snap.notes[(size_t) i].on < hi - 1.0e-12; ++i)
        {
            const auto& nt = snap.notes[(size_t) i];
            const int s = std::clamp ((int) std::floor ((occ + nt.on - beat) * spb + 1.0e-6), 0, n - 1);
            if (nt.probability < 1.0f && ! rng.chance (nt.probability)) continue;
            offsDueBy (clock_ + s);
            if (active_[(size_t) nt.pitch].on) noteOff (out, nt.pitch, s);   // retrigger: end the old one first
            out.addEvent (juce::MidiMessage::noteOn (channel_, nt.pitch, (juce::uint8) nt.velocity), s);
            auto& a = active_[(size_t) nt.pitch];
            a.on = true;
            a.channel = (uint8_t) channel_;
            a.offAt = clock_ + s + std::max<int64_t> (1, (int64_t) std::llround ((nt.off - nt.on) * spb));
        }
    }
    if (place.once && b1 >= occurrence (0) + L) finished_ = true;
    offsDueBy (clock_ + n - 1);
    lastEnd_ = std::max (b1, running_ ? lastEnd_ : b1);
    if (beat < lastEnd_ - 2.0 * n / spb) lastEnd_ = b1;   // after a jump back
    clock_ += n;
    running_ = true;
}
} // namespace fm
