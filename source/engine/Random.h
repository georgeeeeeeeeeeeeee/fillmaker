#pragma once
// Seeded PCG32 with our own distributions (std:: distributions differ between standard libraries, so
// they'd break "the same seed gives the same fill" across Mac and Windows builds).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fm
{
class Rng
{
public:
    explicit Rng (uint64_t seed = 0x853c49e6748fea9bULL, uint64_t stream = 0xda3e39cb94b95bdbULL) noexcept { reseed (seed, stream); }

    void reseed (uint64_t seed, uint64_t stream = 0xda3e39cb94b95bdbULL) noexcept
    {
        state_ = 0;
        inc_ = (stream << 1u) | 1u;
        next();
        state_ += mix (seed);
        next();
    }

    // An independent stream for one part of the generation (so turning on one engine doesn't change
    // what another one does with the same seed).
    Rng fork (uint64_t salt) noexcept { return Rng (mix (next() ^ (salt * 0x9e3779b97f4a7c15ULL)), salt + 1); }

    uint32_t next() noexcept
    {
        const uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const auto x = (uint32_t) (((old >> 18u) ^ old) >> 27u);
        const auto rot = (uint32_t) (old >> 59u);
        return (x >> rot) | (x << ((~rot + 1u) & 31u));
    }

    double uniform() noexcept { return (double) next() * (1.0 / 4294967296.0); }               // [0, 1)
    double uniform (double lo, double hi) noexcept { return lo + (hi - lo) * uniform(); }
    bool chance (double p) noexcept { return uniform() < p; }
    int below (int n) noexcept                                                                // [0, n)
    {
        if (n <= 1) return 0;
        const auto bound = (uint32_t) n, threshold = (uint32_t) (-bound) % bound;
        for (;;)
            if (const uint32_t r = next(); r >= threshold) return (int) (r % bound);
    }
    int range (int lo, int hi) noexcept { return lo + below (hi - lo + 1); }                  // [lo, hi]
    double gauss() noexcept                                                                   // ~N(0, 1)
    {
        double s = 0.0;
        for (int i = 0; i < 4; ++i) s += uniform();
        return (s - 2.0) * 1.7320508075688772;   // Irwin-Hall, 4 terms: close enough, bounded (+-3.5)
    }
    // An index drawn by weight (-1 when every weight is zero).
    int weighted (const std::vector<double>& w) noexcept
    {
        double total = 0.0;
        for (double x : w) total += std::max (0.0, x);
        if (total <= 0.0) return -1;
        double r = uniform() * total;
        for (size_t i = 0; i < w.size(); ++i)
        {
            r -= std::max (0.0, w[i]);
            if (r < 0.0) return (int) i;
        }
        for (size_t i = w.size(); i-- > 0;) if (w[i] > 0.0) return (int) i;
        return -1;
    }
    template <typename T> void shuffle (std::vector<T>& v) noexcept
    {
        for (size_t i = v.size(); i > 1; --i) std::swap (v[i - 1], v[(size_t) below ((int) i)]);
    }

    static uint64_t mix (uint64_t x) noexcept
    {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }

private:
    uint64_t state_ = 0, inc_ = 1;
};
} // namespace fm
