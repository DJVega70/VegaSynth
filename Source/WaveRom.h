#pragma once
// Original "wave ROM" for the PCM-style synth. Nothing here is sampled from any hardware:
// every looped waveform and every attack transient is generated mathematically at start-up
// (additive synthesis, simple physical models, shaped noise).
//
// Like a PCM workstation, a sound is an ATTACK one-shot (played once, pitch-shifted across the
// keyboard) plus a LOOP (a single-cycle table that sustains). Loops are band-limited per
// half-octave zone, so notes stay clean up the keyboard.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pcm
{
constexpr int kTable = 2048;          // samples per single-cycle loop
constexpr int kZones = 22;            // 6 semitones per zone
constexpr int kNumLoops = 28;
constexpr int kNumAttacks = 14;
constexpr double kAttackRate = 44100.0;
constexpr double kAttackRoot = 261.6255653005986;   // attacks are stored at C4
constexpr double kTwoPi = 6.283185307179586;

inline const char* loopName(int i)
{
    static const char* n[kNumLoops] = { "Sine", "Saw", "Square", "Pulse 25", "Triangle", "Organ Full", "Organ Perc",
        "Strings", "Brass", "Piano", "E.Piano", "Choir Ah", "Choir Oo", "Voice Ee", "Clarinet", "Oboe", "Flute",
        "Bass", "Digi Bright", "Hollow", "Glass", "Metal Vox", "Soft Saw", "Sitar", "Harpsi", "Accordion",
        "Marimba", "Digi Pad" };
    return n[std::max(0, std::min(kNumLoops - 1, i))];
}

class WaveRom
{
public:
    // Builds everything for the given host sample rate (band limiting depends on it).
    void build(double sampleRate)
    {
        sr = sampleRate;
        loops.assign((size_t) kNumLoops * kZones * (kTable + 1), 0.0f);
        std::vector<float> sinLut(kTable);
        for (int i = 0; i < kTable; ++i) sinLut[(size_t) i] = (float) std::sin(kTwoPi * i / kTable);

        std::vector<double> acc(kTable);
        for (int w = 0; w < kNumLoops; ++w)
            for (int z = 0; z < kZones; ++z)
            {
                const double zoneRootNote = z * 6 + 3, topNote = z * 6 + 5 + 2;
                const double f0 = 440.0 * std::pow(2.0, (zoneRootNote - 69.0) / 12.0);
                const double fTop = 440.0 * std::pow(2.0, (topNote - 69.0) / 12.0);
                const int hMax = std::max(1, std::min(220, (int) std::floor(0.46 * sr / fTop)));
                std::fill(acc.begin(), acc.end(), 0.0);
                for (int h = 1; h <= hMax; ++h)
                {
                    const double a = harmonic(w, h, f0);
                    if (std::fabs(a) < 1.0e-4) continue;
                    for (int i = 0, idx = 0; i < kTable; ++i, idx = (idx + h) & (kTable - 1))
                        acc[(size_t) i] += a * sinLut[(size_t) idx];
                }
                double pk = 1.0e-9, mean = 0.0;
                for (double v : acc) mean += v;
                mean /= kTable;
                for (double& v : acc) { v -= mean; pk = std::max(pk, std::fabs(v)); }
                float* t = &loops[((size_t) w * kZones + z) * (kTable + 1)];
                for (int i = 0; i < kTable; ++i) t[i] = (float) (acc[(size_t) i] / pk);
                t[kTable] = t[0];
            }

        attacks.assign(kNumAttacks, {});
        for (int a = 0; a < kNumAttacks; ++a) attacks[(size_t) a] = makeAttack(a);
    }

    const float* loop(int wave, int zone) const
    {
        return &loops[((size_t) std::max(0, std::min(kNumLoops - 1, wave)) * kZones + (size_t) std::max(0, std::min(kZones - 1, zone))) * (kTable + 1)];
    }
    const std::vector<float>& attack(int id) const { return attacks[(size_t) std::max(0, std::min(kNumAttacks - 1, id))]; }
    size_t totalSamples() const { size_t n = loops.size(); for (auto& a : attacks) n += a.size(); return n; }

    static const char* attackName(int i)
    {
        static const char* n[kNumAttacks] = { "Hammer", "Piano Strike", "Tine", "Pluck", "Sitar Pluck", "Mallet", "Bell",
            "Blow", "Bow", "Brass Blat", "Key Click", "Chiff", "Breath", "Thump" };
        return n[std::max(0, std::min(kNumAttacks - 1, i))];
    }

private:
    // amplitude of harmonic h for fundamental f0 (zone centre); signed, sums of sines
    static double harmonic(int w, int h, double f0)
    {
        const double fh = h * f0, H = (double) h;
        auto formant = [&](double fc, double bw) { const double d = (fh - fc) / bw; return 1.0 / (1.0 + d * d); };
        const bool odd = (h & 1) != 0;
        switch (w)
        {
            case 0: return h == 1 ? 1.0 : 0.0;
            case 1: return 1.0 / H;
            case 2: return odd ? 1.0 / H : 0.0;
            case 3: return std::sin(3.14159265358979 * H * 0.25) / H;
            case 4: return odd ? ((h % 4 == 1) ? 1.0 : -1.0) / (H * H) : 0.0;
            case 5: { static const double a[9] = { 0, 1.0, 0.9, 0.6, 0.7, 0, 0.35, 0, 0.3 }; return h <= 8 ? a[h] : 0.0; }
            case 6: { static const double a[9] = { 0, 0.7, 0.9, 0.5, 0.4, 0.25, 0.3, 0, 0.2 }; return h <= 8 ? a[h] : 0.0; }
            case 7: return std::pow(H, -1.05) * (1.0 - ((h % 3) == 0 ? 0.35 : 0.0));
            case 8: return std::pow(H, -0.9) * (0.3 + 2.0 * formant(1000.0, 700.0));
            case 9: return std::pow(H, -1.3) * (1.0 + 0.5 * std::cos(H * 0.9)) / (1.0 + std::pow(H / 14.0, 4.0));
            case 10: { static const double a[7] = { 0, 1.0, 0.55, 0.12, 0.25, 0.05, 0.08 }; return h <= 6 ? a[h] : 0.0; }
            case 11: return std::pow(H, -1.0) * (formant(800, 130) + 0.6 * formant(1150, 180) + 0.25 * formant(2900, 300) + 0.05);
            case 12: return std::pow(H, -1.0) * (formant(350, 90) + 0.4 * formant(800, 120) + 0.03);
            case 13: return std::pow(H, -1.0) * (formant(280, 70) + 0.7 * formant(2300, 250) + 0.3 * formant(3000, 300) + 0.03);
            case 14: return (odd ? 1.0 : 0.03) * std::pow(H, -1.1);
            case 15: return std::pow(H, -0.6) * (0.2 + 1.6 * formant(1300, 500));
            case 16: { static const double a[6] = { 0, 1.0, 0.25, 0.08, 0.03, 0.015 }; return h <= 5 ? a[h] : 0.0; }
            case 17: return std::pow(H, -1.6);
            case 18: return h <= 24 ? std::pow(H, -0.4) : 0.0;
            case 19: return odd ? std::pow(H, -0.7) : 0.0;
            case 20: return (h % 4 == 1) ? std::pow(H, -0.6) : 0.1 / H;
            case 21: return std::sin(3.14159265358979 * H * 0.1) / H * (1.0 + 0.8 * std::cos(H * 2.2));
            case 22: return std::pow(H, -1.6);
            case 23: return std::pow(H, -0.5) * (odd ? 1.0 : 0.4) / (1.0 + (H / 30.0) * (H / 30.0));
            case 24: return h <= 30 ? std::pow(H, -0.8) * (1.0 + 0.8 * std::cos(H * 1.3)) : 0.0;
            case 25: return std::pow(H, -0.7) * (1.0 + 0.6 * std::cos(H * 0.55));
            case 26: return h == 1 ? 1.0 : h == 4 ? 0.08 : h == 10 ? 0.03 : 0.0;
            case 27: { const double s = std::sin(H * 0.9); return h <= 16 ? std::pow(H, -0.8) * (0.5 + 0.5 * s * s) : 0.0; }
        }
        return 0.0;
    }

    struct Rng
    {
        uint32_t s = 22222u;
        float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 2147483648.0 - 1.0); }
    };

    static std::vector<float> makeAttack(int id)
    {
        const double fs = kAttackRate, f0 = kAttackRoot;
        Rng rng;
        auto env = [&](double t, double tau) { return std::exp(-t / tau); };
        auto addPartial = [&](std::vector<float>& v, double f, double a, double tau) {
            for (size_t i = 0; i < v.size(); ++i) { const double t = (double) i / fs; v[i] += (float) (a * std::sin(kTwoPi * f * t) * env(t, tau)); } };
        auto lpNoise = [&](std::vector<float>& v, double cutoff, double amp, double tau, double attackT) {
            const double c = 1.0 - std::exp(-kTwoPi * cutoff / fs); double y = 0;
            for (size_t i = 0; i < v.size(); ++i) { const double t = (double) i / fs; y += c * (rng.next() - y);
                const double e = (attackT > 0 ? (1.0 - env(t, attackT)) : 1.0) * env(t, tau); v[i] += (float) (amp * y * e); } };

        std::vector<float> v;
        switch (id)
        {
            case 0: v.assign(3000, 0.0f); addPartial(v, 130.0, 1.0, 0.02); lpNoise(v, 1200.0, 3.0, 0.015, 0.0); break;
            case 1:
                v.assign(20000, 0.0f);
                for (int k = 1; k <= 12; ++k)
                    addPartial(v, f0 * k * std::sqrt(1.0 + 0.0004 * k * k), std::pow((double) k, -0.8), 0.9 / (1.0 + 0.5 * k) + 0.05);
                lpNoise(v, 3000.0, 2.0, 0.012, 0.0);
                break;
            case 2:
                v.assign(16000, 0.0f);
                addPartial(v, f0, 1.0, 0.5); addPartial(v, f0 * 2.0, 0.5, 0.25);
                addPartial(v, f0 * 5.5, 0.35, 0.06); addPartial(v, f0 * 8.3, 0.2, 0.03);
                lpNoise(v, 800.0, 1.0, 0.01, 0.0);
                break;
            case 3: case 4:
            {
                const int len = id == 3 ? 14000 : 18000;
                const int D = std::max(2, (int) std::lround(fs / f0));
                std::vector<float> ring((size_t) D);
                for (auto& x : ring) x = rng.next();
                v.assign((size_t) len, 0.0f);
                const float decay = id == 3 ? 0.996f : 0.9985f;
                int pos = 0; float prev = 0;
                for (int i = 0; i < len; ++i)
                {
                    const float cur = ring[(size_t) pos];
                    const float nxt = decay * 0.5f * (cur + prev); prev = cur;
                    ring[(size_t) pos] = nxt; v[(size_t) i] = cur; pos = (pos + 1) % D;
                }
                if (id == 4) for (auto& x : v) x = std::tanh(3.0f * x + 0.3f) - 0.29f;   // sitar-style "buzz"
                break;
            }
            case 5: v.assign(14000, 0.0f); addPartial(v, f0, 1.0, 0.25); addPartial(v, f0 * 3.93, 0.5, 0.06); addPartial(v, f0 * 9.2, 0.25, 0.02); break;
            case 6:
            {
                v.assign(30000, 0.0f);
                const double r[5] = { 1, 2.76, 5.4, 8.93, 11.34 }, a[5] = { 1, 0.7, 0.5, 0.35, 0.25 }, t[5] = { 1.2, 0.8, 0.5, 0.35, 0.25 };
                for (int k = 0; k < 5; ++k) addPartial(v, f0 * r[k], a[k], t[k]);
                break;
            }
            case 7: v.assign(9000, 0.0f); lpNoise(v, 3000.0, 3.0, 0.07, 0.03); break;
            case 8:
                v.assign(10000, 0.0f); lpNoise(v, 2000.0, 2.0, 0.08, 0.01);
                for (int k = 1; k <= 8; ++k) addPartial(v, f0 * k, 0.3 / k, 0.12);
                break;
            case 9:
                v.assign(8000, 0.0f);
                for (size_t i = 0; i < v.size(); ++i)
                {
                    const double t = (double) i / fs; double s = 0;
                    for (int k = 1; k <= 20; ++k) s += std::sin(kTwoPi * f0 * k * t) / k;
                    v[i] = (float) (s * (1.0 - env(t, 0.02)) * env(t, 0.12));
                }
                break;
            case 10: v.assign(900, 0.0f); lpNoise(v, 6000.0, 3.0, 0.002, 0.0); addPartial(v, 1200.0, 0.6, 0.002); break;
            case 11: v.assign(4000, 0.0f); lpNoise(v, 2500.0, 1.5, 0.03, 0.004); addPartial(v, f0, 0.6, 0.05); break;
            case 12: v.assign(12000, 0.0f); lpNoise(v, 1500.0, 3.0, 0.1, 0.04); break;
            case 13:
                v.assign(6000, 0.0f);
                { double ph = 0; for (size_t i = 0; i < v.size(); ++i) { const double t = (double) i / fs; const double f = 50.0 + 100.0 * env(t, 0.03);
                    ph += kTwoPi * f / fs; v[i] = (float) (std::sin(ph) * env(t, 0.08)); } }
                break;
            default: v.assign(100, 0.0f);
        }
        float pk = 1.0e-9f;
        for (float x : v) pk = std::max(pk, std::fabs(x));
        for (float& x : v) x /= pk;
        // short fade-out at the end so the one-shot never clicks
        const size_t fade = std::min<size_t>(v.size() / 4, 200);
        for (size_t i = 0; i < fade; ++i) v[v.size() - 1 - i] *= (float) i / (float) fade;
        return v;
    }

    double sr = 44100.0;
    std::vector<float> loops;
    std::vector<std::vector<float>> attacks;
};
} // namespace pcm
