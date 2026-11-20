#include "engine/Mutation.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace fm
{
namespace
{
Proto toProto (const Event& e, double bar)
{
    Proto p;
    p.time = e.start;
    p.offset = e.offset;
    p.length = e.length;
    p.pitch = e.pitch;
    p.level = e.velocity / 100.0;
    p.accent = e.accent;
    p.metric = metricWeight (e.start, bar);
    return p;
}
} // namespace

Pattern mutate (const Pattern& in, const Settings& s, const MutationSettings& m, uint32_t seed, juce::String* whatChanged)
{
    Rng rng (seed, 0x307a7e);
    Pattern out = in;
    const double L = in.length, bar = in.beatsPerBar;
    const double amount = std::pow (0.05 + 0.95 * (1.0 - std::clamp ((double) m.preservation, 0.0, 1.0)), 1.15);
    const double r0 = m.regionEnd > m.regionStart ? std::clamp (m.regionStart, 0.0, L) : 0.0;
    const double r1 = m.regionEnd > m.regionStart ? std::clamp (m.regionEnd, 0.0, L) : L;
    const std::set<uint32_t> only (m.onlyIds.begin(), m.onlyIds.end());
    const auto inScope = [&] (const Event& e) { return e.start >= r0 - 1.0e-9 && e.start < r1 - 1.0e-9 && (only.empty() || only.count (e.id) > 0); };
    const double step = 1.0 / s.stepsPerBeat();
    PitchPicker picker (s, nullptr);
    picker.beginPhrase (s.motion, rng);
    juce::StringArray did;
    const auto dim = [&] (float d) { return std::clamp (amount * d, 0.0, 1.0); };

    // Structure: cells of the region rearranged (order shuffled, rotated or reversed), and the inside
    // of some cells permuted. Notes travel with their cell.
    if (only.empty() && rng.chance (std::min (1.0, dim (m.structure) * 1.6)))
    {
        const double cell = (r1 - r0) >= 2.0 ? 1.0 : std::max (step, (r1 - r0) * 0.5);
        const int n = (int) std::floor ((r1 - r0) / cell + 1.0e-9);
        if (n >= 2)
        {
            std::vector<int> order ((size_t) n);
            for (int i = 0; i < n; ++i) order[(size_t) i] = i;
            const int how = rng.below (3);
            if (how == 0) rng.shuffle (order);
            else if (how == 1) std::rotate (order.begin(), order.begin() + 1 + rng.below (n - 1), order.end());
            else std::reverse (order.begin(), order.end());
            // Not every cell moves at high preservation: only a share of them swap places.
            for (int i = 0; i < n; ++i)
                if (! rng.chance (dim (m.structure) * 1.4)) order[(size_t) i] = i;
            std::vector<int> fixed ((size_t) n, -1);
            std::vector<bool> used ((size_t) n, false);
            for (int i = 0; i < n; ++i) if (order[(size_t) i] == i) { fixed[(size_t) i] = i; used[(size_t) i] = true; }
            for (int i = 0; i < n; ++i)
                if (fixed[(size_t) i] < 0)
                    for (int k : order) if (! used[(size_t) k]) { fixed[(size_t) i] = k; used[(size_t) k] = true; break; }
            for (auto& e : out.events)
            {
                if (! inScope (e)) continue;
                const int c = std::clamp ((int) std::floor ((e.start - r0) / cell), 0, n - 1);
                // The cell now at position i is the one that was at fixed[i]: find where c went.
                for (int i = 0; i < n; ++i)
                    if (fixed[(size_t) i] == c) { e.start += (i - c) * cell; break; }
            }
            did.add ("cells rearranged");
        }
        // Inside one cell: a permutation.
        if (rng.chance (dim (m.structure)))
        {
            const double c0 = r0 + std::floor (rng.uniform() * std::max (1.0, (r1 - r0) / cell)) * cell;
            std::vector<Proto> motif;
            std::vector<uint32_t> ids;
            for (const auto& e : out.events)
                if (inScope (e) && e.start >= c0 && e.start < c0 + cell) { auto p = toProto (e, bar); p.time -= c0; p.step = step; motif.push_back (p); ids.push_back (e.id); }
            if (motif.size() >= 2)
            {
                const auto op = pickPermutation (rng, s.behaviour, s.complexity, false);
                // Keep ids where the note count is unchanged (fragment can change it).
                auto moved = permute (motif, cell, op, rng, step);
                for (auto id : ids) out.remove (id);
                for (const auto& p : moved)
                {
                    Event e;
                    e.start = c0 + p.time;
                    e.offset = p.offset;
                    e.length = p.length;
                    e.pitch = p.pitch;
                    e.velocity = (int) std::lround (p.level * 100.0);
                    e.accent = p.accent;
                    out.add (e);
                }
                did.add (juce::String ("a cell ") + nameOf (op));
            }
        }
    }

    // Subdivision: a cell moved onto another grid (a tuplet), or a note split into a ratchet.
    if (rng.chance (dim (m.subdivision) * 1.3))
    {
        if (rng.chance (0.5))
        {
            static const double grids[] = { 3.0, 5.0, 6.0, 7.0, 8.0 };
            const double g = grids[rng.below (5)];
            const double c0 = r0 + std::floor (rng.uniform() * std::max (1.0, r1 - r0)), c1 = std::min (r1, c0 + 1.0);
            for (auto& e : out.events)
                if (inScope (e) && e.start >= c0 && e.start < c1)
                    e.start = c0 + std::round ((e.start - c0) * g) / g;
            did.add ("a beat re-gridded to " + juce::String ((int) g) + "/beat");
        }
        else
        {
            std::vector<size_t> cand;
            for (size_t i = 0; i < out.events.size(); ++i) if (inScope (out.events[i])) cand.push_back (i);
            if (! cand.empty())
            {
                const Event e = out.events[cand[(size_t) rng.below ((int) cand.size())]];
                const int count = pickRatchetCount (rng, s.complexity, s.behaviour, s.ratchetMax);
                Settings rs = s;
                GrammarPlan none;
                none.length = L;
                PitchPicker pp (rs, nullptr);
                Ctx c { rs, none, pp, nullptr, L, bar };
                std::vector<Proto> r;
                addRatchet (r, e.start, std::max (step, std::min (e.length * 2.0, 0.5)), count, e.pitch, c, rng, false);
                out.remove (e.id);
                for (const auto& p : r)
                {
                    Event x;
                    x.start = p.time;
                    x.length = p.length;
                    x.pitch = p.pitch;
                    x.velocity = std::clamp ((int) std::lround (e.velocity * p.level), 1, 127);
                    out.add (x);
                }
                did.add ("a note became a " + juce::String (count) + "-ratchet");
            }
        }
    }

    // Per note: timing, velocity, pitch, probability, removal.
    int moved = 0, revoiced = 0, removed = 0, retimed = 0;
    std::vector<uint32_t> drop;
    for (auto& e : out.events)
    {
        if (! inScope (e)) continue;
        if (rng.chance (dim (m.timing) * 0.8))
        {
            if (rng.chance (0.45)) e.start += (rng.chance (0.5) ? -1.0 : 1.0) * step;
            else e.offset += (rng.chance (0.5) ? -1.0 : 1.0) * step * rng.uniform (0.05, 0.35);
            ++retimed;
        }
        if (rng.chance (dim (m.velocity) * 1.2))
        {
            e.velocity = std::clamp ((int) std::lround (e.velocity + rng.gauss() * amount * m.velocity * 32.0), 1, 127);
            if (rng.chance (dim (m.velocity) * 0.25)) e.accent = ! e.accent;
            ++moved;
        }
        if (picker.size() > 1 && rng.chance (dim (m.pitch) * 0.8))
        {
            const auto& pool = picker.pool();
            auto it = std::find_if (pool.begin(), pool.end(), [&e] (const PitchEntry& x) { return x.pitch == e.pitch; });
            if (it != pool.end() && rng.chance (0.6))
            {
                const int idx = (int) (it - pool.begin()) + (rng.chance (0.5) ? 1 : -1);
                e.pitch = pool[(size_t) std::clamp (idx, 0, picker.size() - 1)].pitch;
            }
            else
                e.pitch = picker.pickOther (rng, e.pitch);
            e.probability = picker.probabilityOf (e.pitch);
            ++revoiced;
        }
        if (rng.chance (dim (m.probability) * 0.6))
            e.probability = (float) std::clamp (e.probability + (rng.chance (0.5) ? -1.0 : 1.0) * rng.uniform (0.1, 0.5), 0.3, 1.0);
        // Removal prefers the weak and the quiet.
        const double weak = 1.0 - metricWeight (e.start, bar) * 0.7 - e.velocity / 254.0;
        if (rng.chance (dim (m.density) * 0.45 * (0.5 + weak))) { drop.push_back (e.id); ++removed; }
    }
    for (auto id : drop) out.remove (id);

    // Addition: new notes in the empty places of the region, at about the pattern's own loudness.
    int added = 0;
    {
        int inRegion = 0;
        double vel = 0.0;
        for (const auto& e : out.events) if (inScope (e)) { ++inRegion; vel += e.velocity; }
        vel = inRegion > 0 ? vel / inRegion : s.velocityBase;
        const double want = amount * m.density * 0.55 * std::max (2, inRegion);
        const int n = (int) std::floor (want) + (rng.chance (want - std::floor (want)) ? 1 : 0);
        const auto grid = buildGrid (r0, r1 - r0, s.stepsPerBeat(), bar, L);
        for (int k = 0; k < n && ! grid.empty(); ++k)
        {
            std::vector<double> w;
            for (const auto& gp : grid)
            {
                bool taken = false;
                for (const auto& e : out.events) if (std::abs (e.start - gp.time) < 1.0e-6) { taken = true; break; }
                w.push_back (taken ? 0.0 : 0.25 + gp.metric * (1.0 - s.complexity) + (1.0 - gp.metric) * s.complexity);
            }
            const int i = rng.weighted (w);
            if (i < 0) break;
            Event e;
            e.start = grid[(size_t) i].time;
            e.length = step * 0.6;
            e.pitch = picker.next (rng, grid[(size_t) i].metric);
            e.velocity = std::clamp ((int) std::lround (vel + rng.gauss() * 12.0), 1, 127);
            e.probability = picker.probabilityOf (e.pitch);
            out.add (e);
            ++added;
        }
    }
    out.sanitise();
    if (out.events.empty() && ! in.events.empty()) out = in;   // never mutate a fill away to nothing
    if (whatChanged != nullptr)
    {
        if (retimed) did.add (juce::String (retimed) + " retimed");
        if (moved) did.add (juce::String (moved) + " louder/softer");
        if (revoiced) did.add (juce::String (revoiced) + " new pitch");
        if (removed) did.add (juce::String (removed) + " removed");
        if (added) did.add (juce::String (added) + " added");
        *whatChanged = did.isEmpty() ? juce::String ("barely anything") : did.joinIntoString (", ");
    }
    return out;
}

double similarity (const Pattern& a, const Pattern& b)
{
    const auto share = [] (const Pattern& x, const Pattern& y) {
        if (x.events.empty()) return y.events.empty() ? 1.0 : 0.0;
        int kept = 0;
        for (const auto& e : x.events)
            for (const auto& f : y.events)
                if (e.pitch == f.pitch && std::abs (e.time() - f.time()) <= 1.0 / 16.0 + 1.0e-9) { ++kept; break; }
        return (double) kept / (double) x.events.size();
    };
    return 0.5 * (share (a, b) + share (b, a));
}

juce::var toVar (const MutationSettings& m)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("preservation", m.preservation);
    o->setProperty ("timing", m.timing);
    o->setProperty ("velocity", m.velocity);
    o->setProperty ("pitch", m.pitch);
    o->setProperty ("density", m.density);
    o->setProperty ("structure", m.structure);
    o->setProperty ("subdivision", m.subdivision);
    o->setProperty ("probability", m.probability);
    return juce::var (o);
}

MutationSettings mutationFromVar (const juce::var& v)
{
    MutationSettings m;
    const auto f = [&] (const char* k, float d) { const auto x = v.getProperty (k, d); return (float) std::clamp (x.isDouble() || x.isInt() ? (double) x : (double) d, 0.0, 1.0); };
    m.preservation = f ("preservation", m.preservation);
    m.timing = f ("timing", m.timing);
    m.velocity = f ("velocity", m.velocity);
    m.pitch = f ("pitch", m.pitch);
    m.density = f ("density", m.density);
    m.structure = f ("structure", m.structure);
    m.subdivision = f ("subdivision", m.subdivision);
    m.probability = f ("probability", m.probability);
    return m;
}
} // namespace fm
