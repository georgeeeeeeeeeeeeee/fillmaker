#include "engine/Pattern.h"

#include <algorithm>
#include <cmath>

namespace fm
{
Event& Pattern::add (Event e)
{
    e.id = nextId++;
    events.push_back (e);
    return events.back();
}

Event* Pattern::find (uint32_t id) noexcept
{
    for (auto& e : events) if (e.id == id) return &e;
    return nullptr;
}

bool Pattern::remove (uint32_t id)
{
    const auto n = events.size();
    events.erase (std::remove_if (events.begin(), events.end(), [id] (const Event& e) { return e.id == id; }), events.end());
    return events.size() != n;
}

void Pattern::sort()
{
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) {
        if (a.time() != b.time()) return a.time() < b.time();
        return a.pitch < b.pitch;
    });
}

void Pattern::sanitise()
{
    if (! std::isfinite (length) || length < 0.0625) length = 0.0625;
    length = std::min (length, 256.0);
    if (! std::isfinite (beatsPerBar) || beatsPerBar <= 0.0) beatsPerBar = 4.0;
    std::vector<Event> keep;
    keep.reserve (events.size());
    for (auto e : events)
    {
        if (! std::isfinite (e.start) || ! std::isfinite (e.offset) || ! std::isfinite (e.length)) continue;
        e.pitch = std::clamp (e.pitch, 0, 127);
        e.velocity = std::clamp (e.velocity, 1, 127);
        e.probability = std::clamp (std::isfinite (e.probability) ? e.probability : 1.0f, 0.0f, 1.0f);
        // Inside the pattern: a displacement can't push a note before 0 or past the end.
        e.start = std::clamp (e.start, 0.0, length - kMinLength);
        if (e.time() < 0.0) e.offset = -e.start;
        if (e.time() < 0.0) { e.offset = 0.0; e.start = 0.0; }   // rounding can leave start + (-start) a hair below 0
        if (e.time() > length - kMinLength) e.offset = (length - kMinLength) - e.start;   // the shortest note must still fit
        e.length = std::clamp (e.length, kMinLength, std::max (kMinLength, length - e.time()));
        if (e.id == 0) e.id = nextId++;
        nextId = std::max (nextId, e.id + 1);
        keep.push_back (e);
    }
    events.swap (keep);
    sort();
    // Same pitch at (nearly) the same moment is one hit: keep the louder.
    std::vector<Event> uniq;
    uniq.reserve (events.size());
    for (const auto& e : events)
    {
        bool dup = false;
        for (auto it = uniq.rbegin(); it != uniq.rend() && e.time() - it->time() < 1.0e-6; ++it)
            if (it->pitch == e.pitch) { if (e.velocity > it->velocity) *it = e; dup = true; break; }
        if (! dup) uniq.push_back (e);
    }
    events.swap (uniq);
    if ((int) events.size() > kMaxEvents)
    {
        std::vector<size_t> order (events.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort (order.begin(), order.end(), [this] (size_t a, size_t b) { return events[a].velocity > events[b].velocity; });
        std::vector<Event> top;
        for (size_t i = 0; i < (size_t) kMaxEvents; ++i) top.push_back (events[order[i]]);
        events.swap (top);
        sort();
    }
}

juce::var Pattern::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("length", length);
    o->setProperty ("beatsPerBar", beatsPerBar);
    o->setProperty ("seed", (juce::int64) seed);
    o->setProperty ("nextId", (juce::int64) nextId);
    juce::Array<juce::var> list;
    for (const auto& e : events)
    {
        auto* x = new juce::DynamicObject();
        x->setProperty ("id", (juce::int64) e.id);
        x->setProperty ("start", e.start);
        x->setProperty ("offset", e.offset);
        x->setProperty ("length", e.length);
        x->setProperty ("pitch", e.pitch);
        x->setProperty ("velocity", e.velocity);
        x->setProperty ("probability", e.probability);
        x->setProperty ("accent", e.accent);
        list.add (juce::var (x));
    }
    o->setProperty ("events", list);
    return juce::var (o);
}

Pattern Pattern::fromVar (const juce::var& v)
{
    Pattern p;
    if (! v.isObject()) return p;
    const auto num = [&] (const juce::var& o, const char* k, double d) { const auto x = o.getProperty (k, d); return x.isDouble() || x.isInt() || x.isInt64() ? (double) x : d; };
    p.length = num (v, "length", 4.0);
    p.beatsPerBar = num (v, "beatsPerBar", 4.0);
    p.seed = (uint32_t) (juce::int64) num (v, "seed", 0.0);
    p.nextId = (uint32_t) std::max (1.0, num (v, "nextId", 1.0));
    if (const auto* list = v.getProperty ("events", {}).getArray())
        for (const auto& x : *list)
        {
            Event e;
            e.id = (uint32_t) (juce::int64) num (x, "id", 0.0);
            e.start = num (x, "start", 0.0);
            e.offset = num (x, "offset", 0.0);
            e.length = num (x, "length", 0.125);
            e.pitch = (int) num (x, "pitch", 36.0);
            e.velocity = (int) num (x, "velocity", 100.0);
            e.probability = (float) num (x, "probability", 1.0);
            e.accent = (bool) x.getProperty ("accent", false);
            p.events.push_back (e);
        }
    p.sanitise();
    return p;
}

juce::String Pattern::toJson() const { return juce::JSON::toString (toVar(), true); }
Pattern Pattern::fromJson (const juce::String& s) { return fromVar (juce::JSON::parse (s)); }
} // namespace fm
