#pragma once
// 100 programs (25 sound families x 4 voicings) and 100 combinations. All original.
#include "WaveRom.h"
#ifndef PCM_NO_TRIM
#include "Levels.h"
#endif
#include <cstdio>
#include <cstring>
#include <vector>

namespace pcm
{
struct Program
{
    char name[24] = "";
    int w1 = 0, w2 = 0;               // loop waveforms
    float lvl2 = 0, semi2 = 0, det = 0; // second oscillator level / transpose (semitones) / detune (cents)
    int atk = -1; float atkLvl = 0;   // attack transient (-1 = none)
    float xfade = 0;                  // seconds for the loop to fade in under the transient
    float cutoff = 4000, res = 0.1f, envAmt = 0, keyTrack = 0.5f;   // filter (cutoff Hz at key 60, octaves of envelope)
    float aA = 0.005f, aD = 0.2f, aS = 1.0f, aR = 0.2f;              // amp envelope (s, s, level, s)
    float fA = 0.005f, fD = 0.3f, fS = 0.5f, fR = 0.3f;              // filter envelope
    float lfoRate = 5.0f, lfoPitch = 0, lfoFilt = 0, lfoAmp = 0;     // LFO (Hz, semitones, octaves, depth)
    float velAmp = 0.5f, velFlt = 0.5f;                              // velocity sensitivity
    float level = 0.7f;
};

struct PartCfg
{
    int prog = 0; float level = 0.8f, pan = 0.0f; int lo = 0, hi = 127, transpose = 0, channel = 1;
};

struct Combination
{
    char name[28] = "";
    PartCfg part[8];
};

constexpr int kNumPrograms = 100, kNumCombis = 100, kNumBases = 25;
constexpr int kUserProgram = 100, kAllPrograms = 101;   // program 100 = "User Samples" (plays the sample slots)

namespace detail
{
inline Program mk(const char* n, int w1, int w2, float lvl2, float semi2, float det, int atk, float atkLvl, float xf,
                  float cut, float res, float env, float kt, float aA, float aD, float aS, float aR,
                  float fA, float fD, float fS, float fR, float lr, float lp, float lf, float va, float vf, float lvl)
{
    Program p; std::snprintf(p.name, sizeof p.name, "%s", n);
    p.w1 = w1; p.w2 = w2; p.lvl2 = lvl2; p.semi2 = semi2; p.det = det; p.atk = atk; p.atkLvl = atkLvl; p.xfade = xf;
    p.cutoff = cut; p.res = res; p.envAmt = env; p.keyTrack = kt; p.aA = aA; p.aD = aD; p.aS = aS; p.aR = aR;
    p.fA = fA; p.fD = fD; p.fS = fS; p.fR = fR; p.lfoRate = lr; p.lfoPitch = lp; p.lfoFilt = lf; p.velAmp = va; p.velFlt = vf; p.level = lvl;
    return p;
}

inline std::vector<Program> makePrograms()
{
    //                name            w1 w2 l2   s2  det atk aLv xf    cut  res  env  kt   aA    aD   aS   aR    fA    fD   fS   fR   lfoR lfoP  lfoF va   vf   lvl
    std::vector<Program> b = {
        mk("Grand Piano", 9, 4, 0.0f, 0, 0, 1, 0.9f, 0.12f, 5000, .10f, 2.5f, .9f, .002f, 2.2f, .0f, .35f, .001f, .9f, .1f, .3f, 0, 0, 0, .7f, .6f, .8f),
        mk("E.Piano", 10, 0, 0.25f, 12, 0, 2, 0.8f, 0.10f, 5000, .15f, 1.5f, .7f, .002f, 1.8f, .15f, .4f, .001f, .5f, .3f, .3f, 0, 0, 0, .7f, .8f, .8f),
        mk("Organ", 5, 5, 0.0f, 0, 0, 10, 0.2f, 0.0f, 9000, .05f, 0.0f, .5f, .005f, .1f, 1.0f, .05f, .005f, .3f, .5f, .3f, 6.0f, 0, 0, .1f, .5f, .7f),
        mk("Organ 2", 6, 5, 0.5f, 12, 0, 10, 0.5f, 0.0f, 7000, .10f, 0.0f, .5f, .003f, .1f, 1.0f, .08f, .005f, .3f, .5f, .3f, 6.0f, 0, 0, .1f, .5f, .7f),
        mk("Strings", 7, 7, 0.8f, 0, 12, 8, 0.35f, 0.15f, 3500, .10f, 1.2f, .6f, .35f, .4f, .9f, .5f, .3f, .6f, .7f, .5f, 5.2f, .05f, 0, .4f, .5f, .7f),
        mk("Warm Pad", 27, 22, 0.7f, 0, 9, -1, 0.0f, 0.0f, 2500, .20f, 1.5f, .5f, 1.2f, .8f, .9f, 1.5f, 1.5f, 2.0f, .6f, 1.5f, 0.3f, .05f, 0, .4f, .5f, .7f),
        mk("Brass", 8, 1, 0.4f, 0, 6, 9, 0.6f, 0.05f, 2000, .15f, 3.5f, .5f, .04f, .2f, .85f, .12f, .06f, .3f, .5f, .15f, 5.5f, .04f, 0, .6f, .9f, .7f),
        mk("Soft Brass", 8, 22, 0.5f, 0, 6, 7, 0.3f, 0.05f, 1400, .10f, 2.5f, .5f, .12f, .3f, .85f, .25f, .15f, .4f, .5f, .25f, 5.5f, .04f, 0, .6f, .8f, .7f),
        mk("Sitar", 23, 24, 0.3f, 12, 3, 4, 1.0f, 0.20f, 5500, .35f, 2.5f, .8f, .002f, 1.4f, .0f, .5f, .001f, .5f, .1f, .4f, 0, 0, 0, .6f, .6f, .8f),
        mk("Harpsichord", 24, 1, 0.3f, 12, 0, 3, 0.6f, 0.10f, 7000, .20f, 1.0f, .6f, .001f, .9f, .0f, .18f, .001f, .3f, .3f, .2f, 0, 0, 0, .4f, .5f, .8f),
        mk("Marimba", 26, 0, 0.2f, 24, 0, 5, 1.0f, 0.05f, 6000, .00f, 1.0f, .6f, .001f, .5f, .0f, .25f, .001f, .3f, .2f, .2f, 0, 0, 0, .7f, .5f, .8f),
        mk("Glass Bell", 20, 0, 0.4f, 19, 0, 6, 0.9f, 0.15f, 9000, .10f, 0.0f, .6f, .001f, 3.0f, .2f, 1.2f, .001f, .5f, .5f, .5f, 0, 0, 0, .5f, .5f, .7f),
        mk("Choir", 11, 12, 0.7f, 12, 8, 12, 0.35f, 0.2f, 3500, .10f, 1.0f, .5f, .25f, .3f, .9f, .6f, .2f, .5f, .6f, .5f, 5.0f, .05f, 0, .4f, .5f, .7f),
        mk("Voice Oo", 13, 12, 0.5f, 0, 7, 12, 0.2f, 0.15f, 3000, .10f, 1.0f, .5f, .2f, .3f, .9f, .5f, .2f, .5f, .6f, .5f, 5.0f, .05f, 0, .4f, .5f, .7f),
        mk("Flute", 16, 0, 0.2f, 12, 0, 11, 0.5f, 0.05f, 5000, .05f, 1.0f, .6f, .05f, .1f, .9f, .12f, .04f, .2f, .6f, .15f, 5.0f, .07f, 0, .5f, .5f, .7f),
        mk("Clarinet", 14, 19, 0.3f, 12, 3, 7, 0.2f, 0.05f, 3000, .10f, 1.5f, .5f, .03f, .1f, .9f, .1f, .03f, .2f, .6f, .15f, 5.0f, .03f, 0, .5f, .5f, .7f),
        mk("Synth Bass", 1, 2, 0.6f, -12, 5, -1, 0.0f, 0.0f, 700, .35f, 3.5f, .5f, .003f, .25f, .7f, .1f, .001f, .25f, .2f, .1f, 0, 0, 0, .3f, .8f, .8f),
        mk("Pluck Bass", 17, 4, 0.5f, 0, 0, 3, 0.6f, 0.05f, 900, .15f, 2.5f, .6f, .002f, .6f, .0f, .15f, .001f, .3f, .1f, .1f, 0, 0, 0, .5f, .6f, .85f),
        mk("Saw Lead", 1, 1, 0.7f, 0, 14, -1, 0.0f, 0.0f, 3500, .30f, 2.0f, .8f, .005f, .2f, .85f, .15f, .005f, .3f, .5f, .2f, 5.5f, .08f, 0, .4f, .5f, .7f),
        mk("Square Lead", 2, 3, 0.5f, 12, 4, -1, 0.0f, 0.0f, 3000, .25f, 2.0f, .8f, .005f, .2f, .85f, .15f, .005f, .3f, .5f, .2f, 5.5f, .06f, 0, .4f, .5f, .7f),
        mk("Digital Pad", 18, 21, 0.5f, 7, 6, -1, 0.0f, 0.0f, 4000, .15f, 1.5f, .5f, .5f, .6f, .8f, .9f, .6f, 1.0f, .6f, .9f, 0.5f, .04f, 0, .4f, .5f, .6f),
        mk("Sweep Pad", 1, 22, 0.8f, 0, 16, -1, 0.0f, 0.0f, 500, .45f, 4.0f, .4f, .9f, .6f, .85f, 1.2f, 2.5f, 2.0f, .4f, 1.5f, 0, 0, 0, .4f, .5f, .6f),
        mk("House Piano", 10, 9, 0.6f, 0, 5, 1, 0.8f, 0.10f, 7000, .10f, 2.0f, .8f, .002f, 1.2f, .0f, .25f, .001f, .6f, .1f, .2f, 0, 0, 0, .7f, .7f, .85f),
        mk("Accordion", 25, 15, 0.6f, 0, 10, 7, 0.3f, 0.05f, 4500, .10f, 1.0f, .5f, .04f, .1f, .95f, .12f, .04f, .2f, .6f, .15f, 6.0f, .03f, 0, .3f, .5f, .7f),
        mk("Atmosphere", 20, 27, 0.6f, 12, 9, 6, 0.35f, 0.2f, 3000, .20f, 2.0f, .5f, .6f, .8f, .8f, 1.5f, 1.0f, 2.0f, .5f, 1.5f, 0.4f, .04f, 0, .4f, .5f, .6f),
    };
    std::vector<Program> all;
    all.reserve(101);
    for (int v = 0; v < 4; ++v)
        for (auto p : b)
        {
            const char* suf[4] = { "", " Br", " So", " Wd" };
            char nm[24]; std::snprintf(nm, sizeof nm, "%.15s%s", p.name, suf[v]);
            if (v == 1) { p.cutoff *= 2.0f; p.res = std::min(0.8f, p.res + 0.1f); p.velFlt = std::min(1.0f, p.velFlt + 0.15f); }
            if (v == 2) { p.cutoff *= 0.45f; p.aA = p.aA * 1.8f + 0.01f; p.aR *= 1.4f; p.level *= 0.9f; }
            if (v == 3) { p.det = p.det * 2.2f + 4.0f; p.lvl2 = std::min(1.0f, p.lvl2 * 1.4f + 0.15f); p.cutoff *= 1.1f; p.lfoPitch *= 1.5f; }
            std::snprintf(p.name, sizeof p.name, "%s", nm);
#ifndef PCM_NO_TRIM
            if (all.size() < 100) p.level *= kLevelTrim[all.size()];   // measured loudness matching (see tools/gen_levels.cpp)
#endif
            all.push_back(p);
        }
    {
        Program u; std::snprintf(u.name, sizeof u.name, "User Samples"); u.level = 1.0f; all.push_back(u);
    }
    return all;
}

inline std::vector<Combination> makeCombinations(const std::vector<Program>& progs)
{
    std::vector<Combination> out;
    auto P = [](int prog, float lvl, float pan = 0, int lo = 0, int hi = 127, int tr = 0) {
        PartCfg c; c.prog = prog; c.level = lvl; c.pan = pan; c.lo = lo; c.hi = hi; c.transpose = tr; return c; };
    auto add = [&](const char* name, std::vector<PartCfg> parts) {
        Combination c; std::snprintf(c.name, sizeof c.name, "%s", name);
        for (size_t i = 0; i < 8; ++i)
        {
            if (i < parts.size()) c.part[i] = parts[i]; else { c.part[i] = PartCfg(); c.part[i].level = 0.0f; }
            c.part[i].channel = (int) i + 1;
        }
        out.push_back(c); };

    add("Piano+Strings",     { P(0, .9f), P(4, .5f) });
    add("Bass/Piano Split",  { P(17, .9f, 0, 0, 47), P(0, .9f, 0, 48, 127) });
    add("EP+Pad",            { P(1, .85f), P(5, .5f) });
    add("Organ+Brass",       { P(2, .8f), P(6, .5f) });
    add("Choir+Strings",     { P(12, .8f), P(4, .6f) });
    add("Lead+Pad",          { P(18, .8f), P(20, .45f) });
    add("House Stack",       { P(22, .9f, 0, 36, 127), P(3, .5f, 0, 36, 127), P(16, .9f, 0, 0, 35) });
    add("Bell+Pad",          { P(11, .7f), P(24, .6f) });
    add("Sitar+Strings",     { P(8, .9f), P(4, .4f) });
    add("Brass+Strings",     { P(6, .8f), P(4, .5f) });
    add("Bass/EP Split",     { P(16, .9f, 0, 0, 47), P(1, .85f, 0, 48, 127) });
    add("Harpsi+Piano",      { P(9, .8f), P(0, .4f) });
    add("Marimba+Pad",       { P(10, .8f), P(5, .4f) });
    add("Flute+Strings",     { P(14, .8f), P(4, .4f) });
    add("Wide Pad Stack",    { P(5, .6f, -.5f), P(21, .6f, .5f), P(20, .4f) });
    add("Organ/Lead Split",  { P(2, .8f, 0, 0, 59), P(18, .8f, 0, 60, 127) });
    add("Accordion+Clar",    { P(23, .8f), P(15, .5f) });
    add("Choir+Bell",        { P(12, .8f), P(11, .5f, 0, 0, 127, 12) });
    add("Dream Stack",       { P(24, .6f), P(5, .6f), P(13, .4f) });
    {   // multitimbral setup: one sound per MIDI channel 1-8
        add("Multi Studio Kit", { P(0, .8f, -.3f), P(4, .6f, .3f), P(6, .6f, -.5f), P(16, .8f), P(22, .8f, .4f), P(2, .6f, -.2f), P(12, .6f, .5f), P(11, .6f, 0) });
    }
    const char* fam[kNumBases];
    for (int i = 0; i < kNumBases; ++i) fam[i] = progs[(size_t) i].name;
    // 40 generated layers
    for (int i = 0; i < 40; ++i)
    {
        const int a = i % kNumBases, v = (i / kNumBases + i) % 4;
        int bpart = (a * 3 + 5) % kNumBases; if (bpart == a) bpart = (a + 1) % kNumBases;
        char nm[28]; std::snprintf(nm, sizeof nm, "%.11s+%.11s", fam[a], fam[bpart]);
        add(nm, { P(v * 25 + a, .85f, -.15f), P(((v + 1) % 4) * 25 + bpart, .5f, .15f) });
    }
    // 39 generated bass splits
    for (int k = 0; (int) out.size() < kNumCombis; ++k)
    {
        const int bass = 16 + (k % 2), v = k % 4, up = (k * 2 + 1) % kNumBases;
        if (up == 16 || up == 17) continue;
        char nm[28]; std::snprintf(nm, sizeof nm, "Bass/%.18s", fam[up]);
        add(nm, { P(v * 25 + bass, .9f, 0, 0, 47), P(((k / 3) % 4) * 25 + up, .85f, 0, 48, 127) });
    }
    return out;
}
} // namespace detail

inline const std::vector<Program>& programs() { static const auto p = detail::makePrograms(); return p; }
inline const std::vector<Combination>& combinations() { static const auto c = detail::makeCombinations(programs()); return c; }
} // namespace pcm
