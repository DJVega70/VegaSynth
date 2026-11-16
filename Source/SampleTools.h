#pragma once
// User-sample data model and helpers (no JUCE dependency, so it can be unit-tested on its own).
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace pcm
{
constexpr int kUserSlots = 16;
enum SampleMode { SmpGate = 0, SmpLoop = 1, SmpOneShot = 2 };

struct Audio
{
    std::vector<float> l, r;     // r empty for mono
    double rate = 44100.0;
    size_t frames() const { return l.size(); }
    bool stereo() const { return !r.empty(); }
};

struct SampleSlot
{
    std::shared_ptr<const Audio> audio;
    std::string name;
    int root = 60, lo = 0, hi = 127, velLo = 1, velHi = 127;
    float tune = 0.0f;           // cents
    float level = 1.0f;          // linear
    float pan = 0.0f;            // -1..1
    int mode = SmpGate;
    float loopStart = 0.40f, loopEnd = 0.90f;   // fraction of the sample length
    float cutoff = 20000.0f;     // Hz, 20000 = filter off
    float attack = 0.002f, release = 0.15f;     // seconds
    bool has() const { return audio && audio->frames() > 1; }
};

using SampleBank = std::array<SampleSlot, kUserSlots>;

inline std::shared_ptr<const Audio> makeAudio(std::vector<float> l, std::vector<float> r, double rate)
{
    auto a = std::make_shared<Audio>(); a->l = std::move(l); a->r = std::move(r); a->rate = rate; return a;
}

// Estimates the pitch of a (mostly) pitched sample by normalised autocorrelation.
// Returns false when the sample does not look pitched. note = nearest MIDI note, cents = deviation.
inline bool detectPitch(const Audio& a, int& note, float& cents)
{
    const size_t n = a.frames();
    if (n < 2048) return false;
    std::vector<float> m(n);
    for (size_t i = 0; i < n; ++i) m[i] = a.stereo() ? 0.5f * (a.l[i] + a.r[i]) : a.l[i];

    const int lagMin = std::max(8, (int) (a.rate / 2000.0)), lagMax = std::min((int) (a.rate / 30.0), (int) n / 3);
    const size_t win = std::min<size_t>(8192, n - (size_t) lagMax - 1);
    if (win < 1024 || lagMax <= lagMin + 2) return false;

    // loudest window within the first 6 seconds
    size_t best = 0; double bestE = -1.0;
    const size_t limit = std::min(n - win - (size_t) lagMax, (size_t) (a.rate * 6.0));
    for (size_t s = 0; s <= limit; s += 2048)
    {
        double e = 0; for (size_t i = 0; i < win; i += 4) e += (double) m[s + i] * m[s + i];
        if (e > bestE) { bestE = e; best = s; }
    }
    if (bestE <= 1e-9) return false;

    std::vector<double> r((size_t) lagMax + 2, 0.0);
    double e0 = 0; for (size_t i = 0; i < win; ++i) e0 += (double) m[best + i] * m[best + i];
    for (int lag = 1; lag <= lagMax + 1; ++lag)
    {
        double c = 0, e1 = 0;
        for (size_t i = 0; i < win; ++i) { c += (double) m[best + i] * m[best + i + (size_t) lag]; e1 += (double) m[best + i + (size_t) lag] * m[best + i + (size_t) lag]; }
        r[(size_t) lag] = c / std::sqrt(std::max(e0 * e1, 1e-12));
    }
    int start = 1; while (start < lagMax && r[(size_t) start] > 0.0) ++start;   // skip the main lobe around lag 0
    start = std::max(start, lagMin);
    double rmax = 0; for (int lag = start; lag <= lagMax; ++lag) rmax = std::max(rmax, r[(size_t) lag]);
    if (rmax < 0.5) return false;
    int pick = -1;
    for (int lag = start + 1; lag < lagMax; ++lag)     // first peak that is nearly as strong as the best
        if (r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1] && r[(size_t) lag] >= 0.9 * rmax) { pick = lag; break; }
    if (pick < 0) return false;
    const double y0 = r[(size_t) pick - 1], y1 = r[(size_t) pick], y2 = r[(size_t) pick + 1];
    const double den = y0 - 2 * y1 + y2, off = std::fabs(den) > 1e-12 ? 0.5 * (y0 - y2) / den : 0.0;
    const double freq = a.rate / ((double) pick + off);
    const double midi = 69.0 + 12.0 * std::log2(freq / 440.0);
    note = (int) std::lround(midi); cents = (float) ((midi - note) * 100.0);
    return note >= 0 && note <= 127;
}

// Spreads the loaded slots over the keyboard: each slot covers the keys between its neighbours' roots.
inline void autoMapKeys(SampleBank& bank)
{
    std::vector<int> idx;
    for (int i = 0; i < kUserSlots; ++i) if (bank[(size_t) i].has()) idx.push_back(i);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return bank[(size_t) a].root < bank[(size_t) b].root; });
    for (size_t k = 0; k < idx.size(); ++k)
    {
        auto& s = bank[(size_t) idx[k]];
        s.lo = k == 0 ? 0 : (bank[(size_t) idx[k - 1]].root + s.root) / 2 + 1;
        s.hi = k + 1 == idx.size() ? 127 : (s.root + bank[(size_t) idx[k + 1]].root) / 2;
        if (s.lo > s.hi) s.lo = s.hi;
    }
}

// One key per slot, one-shot playback, starting at startKey (36 = C1 in the Yamaha numbering used by the UI).
inline void drumMap(SampleBank& bank, int startKey = 36)
{
    int key = startKey;
    for (auto& s : bank)
        if (s.has() && key <= 127) { s.root = s.lo = s.hi = key++; s.mode = SmpOneShot; s.velLo = 1; s.velHi = 127; s.tune = 0.0f; }
}
} // namespace pcm
