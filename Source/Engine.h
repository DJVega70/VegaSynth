#pragma once
// PCM-style synth engine: 16 voices, 8 parts (multitimbral), two effect units. No JUCE dependency.
#include "Effects.h"
#include "Programs.h"
#include "SampleTools.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace pcm
{
constexpr int kVoices = 16, kParts = 8, kControl = 16;
enum Mode { ProgramMode = 0, CombiMode = 1, MultiMode = 2 };

struct Globals
{
    int mode = ProgramMode;
    float masterDb = 0.0f;
    float cutoffOffset = 0.0f;   // -1..1  (+-2 octaves)
    float resOffset = 0.0f;      // 0..0.6
    float attackScale = 1.0f, releaseScale = 1.0f, lfoScale = 1.0f;
    float bendRange = 2.0f;      // semitones
};

struct Voice
{
    bool active = false, releasing = false, sustained = false;
    int note = 60, part = 0, ch = 0; float vel = 0.8f; uint64_t age = 0;
    const Program* prog = nullptr; int zoneNote = 60;
    float level = 1.0f, gl = 1.0f, gr = 1.0f;
    double ph1 = 0, ph2 = 0, atkPos = 0, tNote = 0, lfoPh = 0;
    int ampStage = 0, fltStage = 0; double ampLvl = 0, fltLvl = 0;
    double s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    // user-sample voices
    bool isSample = false, kill = false; double pos = 0;
    std::shared_ptr<const Audio> audio;
    struct SampleParams { int root = 60, mode = 0; float tune = 0, cutoff = 20000, att = 0.002f, rel = 0.15f, loopS = 0.4f, loopE = 0.9f; } sp;
    // control-rate values
    double inc1 = 0, inc2 = 0, atkInc = 0, fG = 0.1, fK = 1.0, ampGain = 0;
};

class Engine
{
public:
    Engine() { parts[0].level = 0.8f; }

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        rom.build(sr);
        fx[0].prepare(sr); fx[1].prepare(sr);
        for (auto& v : voices) v = Voice();
        for (int c = 0; c < 16; ++c) { bend[c] = 0; mod[c] = 0; pedal[c] = false; }
        ageCounter = 0;
    }

    Globals g;
    std::array<PartCfg, kParts> parts;
    Effect fx[2];
    float joyX = 0.0f, joyY = 0.0f;   // joystick (-1..1, 0..1)

    // ---------------- MIDI
    void noteOn(int ch, int note, float vel)
    {
        const auto& progs = programs();
        for (int p = 0; p < kParts; ++p)
        {
            if (!partListens(p, ch)) continue;
            const PartCfg& pc = parts[(size_t) p];
            if (pc.level <= 0.0001f || note < pc.lo || note > pc.hi) continue;
            if (std::max(0, std::min(kAllPrograms - 1, pc.prog)) == kUserProgram) { triggerSamples(p, ch, note, vel); continue; }
            Voice& v = allocate();
            v = Voice();
            v.active = true; v.note = note; v.part = p; v.ch = ch; v.vel = vel; v.age = ++ageCounter;
            v.prog = &progs[(size_t) std::max(0, std::min(kAllPrograms - 1, pc.prog))];
            v.zoneNote = note + pc.transpose;
            v.level = pc.level * v.prog->level;
            const double pn = std::max(-1.0f, std::min(1.0f, pc.pan)) * 0.7853981633974483;   // equal power
            v.gl = (float) std::cos(pn + 0.7853981633974483); v.gr = (float) std::sin(pn + 0.7853981633974483);
            v.ph1 = 0; v.ph2 = 0.37;
        }
    }

    void noteOff(int ch, int note)
    {
        for (auto& v : voices)
            if (v.active && !v.releasing && v.ch == ch && v.note == note)
            {
                if (pedal[ch & 15]) v.sustained = true; else v.releasing = true;
            }
    }

    void pedalChange(int ch, bool down)
    {
        pedal[ch & 15] = down;
        if (!down) for (auto& v : voices) if (v.active && v.sustained && v.ch == ch) { v.sustained = false; v.releasing = true; }
    }
    void pitchBend(int ch, float v) { bend[ch & 15] = v; }
    void modWheel(int ch, float v) { mod[ch & 15] = v; }
    void allNotesOff() { for (auto& v : voices) if (v.active) { v.releasing = true; v.kill = true; } }
    void setBank(std::shared_ptr<const SampleBank> b) { bank = std::move(b); }
    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active ? 1 : 0; return n; }

    // ---------------- audio
    void process(float* l, float* r, int n)
    {
        std::fill(l, l + n, 0.0f); std::fill(r, r + n, 0.0f);
        for (auto& v : voices)
            if (v.active) renderVoice(v, l, r, n);
        fx[0].process(l, r, n);
        fx[1].process(l, r, n);
        const float master = std::pow(10.0f, g.masterDb / 20.0f);
        for (int i = 0; i < n; ++i)
        {
            l[i] = softClip(l[i] * master); r[i] = softClip(r[i] * master);
        }
    }

    const WaveRom& waveRom() const { return rom; }
    double sampleRate() const { return sr; }

    // For tests / tools: render a voice with an arbitrary program.
    void renderTestVoice(Voice& v, float* l, float* r, int n) { renderVoice(v, l, r, n); }

private:
    static float softClip(float x)
    {
        const float a = std::fabs(x);
        if (a < 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f);
        return x < 0 ? -y : y;
    }

    bool partListens(int p, int ch) const
    {
        const PartCfg& pc = parts[(size_t) p];
        switch (g.mode)
        {
            case ProgramMode: return p == 0;
            case CombiMode: return true;
            default: return pc.channel - 1 == (ch & 15);
        }
    }

    void triggerSamples(int p, int ch, int note, float vel)
    {
        if (!bank) return;
        const PartCfg& pc = parts[(size_t) p];
        const int iv = (int) std::lround(vel * 127.0f);
        for (const auto& s : *bank)
        {
            if (!s.has() || note < s.lo || note > s.hi || iv < s.velLo || iv > s.velHi) continue;
            Voice& v = allocate();
            v = Voice();
            v.active = true; v.isSample = true; v.audio = s.audio; v.note = note; v.part = p; v.ch = ch; v.vel = vel; v.age = ++ageCounter;
            v.zoneNote = note + pc.transpose;
            v.level = pc.level * s.level;
            const double pn = std::max(-1.0f, std::min(1.0f, pc.pan + s.pan)) * 0.7853981633974483;
            v.gl = (float) std::cos(pn + 0.7853981633974483); v.gr = (float) std::sin(pn + 0.7853981633974483);
            v.sp.root = s.root; v.sp.mode = s.mode; v.sp.tune = s.tune; v.sp.cutoff = s.cutoff; v.sp.att = s.attack; v.sp.rel = s.release;
            v.sp.loopS = s.loopStart; v.sp.loopE = s.loopEnd;
        }
    }

    static inline float hermite(const float* x, int n, double pos)
    {
        const int i = (int) pos; const float f = (float) (pos - i);
        auto at = [&](int k) { return x[std::max(0, std::min(n - 1, k))]; };
        const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    void renderSample(Voice& v, float* l, float* r, int n)
    {
        const Audio& A = *v.audio; const auto& sp = v.sp;
        const double frames = (double) A.frames(), invSr = 1.0 / sr, rr = A.rate * invSr;
        const bool loop = sp.mode == SmpLoop, oneShot = sp.mode == SmpOneShot;
        const double ls = std::max(0.0, std::min((double) sp.loopS * frames, frames - 66.0));
        const double le = std::max(ls + 64.0, std::min((double) sp.loopE * frames, frames - 1.0));
        const double xf = std::min(256.0, 0.5 * (le - ls));
        const float* L = A.l.data(); const float* R = A.stereo() ? A.r.data() : L; const int N = (int) A.frames();
        const bool filt = sp.cutoff < std::min(19000.0, 0.45 * sr);
        const double aA = sp.att * g.attackScale, aR = sp.rel * g.releaseScale;
        const bool rel = v.releasing && (!oneShot || v.kill);
        const double velG = 0.4 + 0.6 * v.vel;

        for (int i0 = 0; i0 < n; i0 += kControl)
        {
            const int m = std::min(kControl, n - i0);
            const double bendSemi = ((double) bend[v.ch & 15] + joyX) * g.bendRange;
            const double ratio = std::exp2(((double) v.zoneNote - sp.root + sp.tune * 0.01 + bendSemi) / 12.0) * rr;
            double fG = 0, fK = 1.85, gD = 1;
            if (filt) { fG = std::tan(3.14159265358979 * std::min((double) sp.cutoff, 0.45 * sr) * invSr); gD = 1.0 / (1.0 + fG * (fG + fK)); }
            for (int i = 0; i < m; ++i)
            {
                double pos = v.pos;
                if (loop && pos >= le) pos -= (le - ls);
                if (!loop && pos >= frames - 1.0) { v.active = false; return; }
                float sl = hermite(L, N, pos), sr2 = A.stereo() ? hermite(R, N, pos) : sl;
                if (loop && pos > le - xf)
                {
                    const float t = (float) ((pos - (le - xf)) / xf); const double op = pos - (le - ls);
                    sl = sl * (1.0f - t) + hermite(L, N, op) * t;
                    sr2 = A.stereo() ? sr2 * (1.0f - t) + hermite(R, N, op) * t : sl;
                }
                v.pos = pos + ratio;
                double gain = velG;
                if (!loop) { const double rem = frames - 1.0 - pos; if (rem < 64.0) gain *= std::max(0.0, rem / 64.0); }
                if (filt)
                {
                    double hp = (sl - (fK + fG) * v.s1 - v.s2) * gD, bp = fG * hp + v.s1; v.s1 = fG * hp + bp; double lp = fG * bp + v.s2; v.s2 = fG * bp + lp; sl = (float) lp;
                    hp = (sr2 - (fK + fG) * v.s3 - v.s4) * gD; bp = fG * hp + v.s3; v.s3 = fG * hp + bp; lp = fG * bp + v.s4; v.s4 = fG * bp + lp; sr2 = (float) lp;
                }
                stepEnv(v.ampStage, v.ampLvl, rel, aA, 0.1, 1.0, aR);
                const float amp = (float) (v.ampLvl * gain * v.level * 1.41421356);
                l[i0 + i] += sl * amp * v.gl; r[i0 + i] += sr2 * amp * v.gr;
            }
            if (rel && v.ampLvl < 1.0e-5) { v.active = false; return; }
        }
    }

    Voice& allocate()
    {
        for (auto& v : voices) if (!v.active) return v;
        Voice* best = nullptr;
        for (auto& v : voices)                                     // quietest releasing voice
            if (v.releasing && (!best || v.ampLvl < best->ampLvl)) best = &v;
        if (best) return *best;
        best = &voices[0];
        for (auto& v : voices) if (v.age < best->age) best = &v;   // else the oldest
        return *best;
    }

    // envelope step: returns new level
    inline void stepEnv(int& stage, double& lvl, bool releasing, double a, double d, double s, double r) const
    {
        if (releasing && stage != 3) stage = 3;
        switch (stage)
        {
            case 0: lvl += 1.0 / std::max(1.0, a * sr); if (lvl >= 1.0) { lvl = 1.0; stage = 1; } break;
            case 1: { const double c = std::exp(-4.6 / std::max(1.0, d * sr)); lvl = s + (lvl - s) * c; if (std::fabs(lvl - s) < 1.0e-4) stage = 2; break; }
            case 2: lvl = s; break;
            default: { const double c = std::exp(-6.9 / std::max(1.0, r * sr)); lvl *= c; break; }
        }
    }

    void renderVoice(Voice& v, float* l, float* r, int n)
    {
        if (v.isSample) { renderSample(v, l, r, n); return; }
        const Program& p = *v.prog;
        const double aA = p.aA * g.attackScale, aR = p.aR * g.releaseScale, fR = p.fR * g.releaseScale;
        const int zone = std::max(0, std::min(kZones - 1, v.zoneNote / 6));
        const int zone2 = std::max(0, std::min(kZones - 1, (int) std::floor((v.zoneNote + p.semi2 + 0.5) / 6.0)));
        const float* t1 = rom.loop(p.w1, zone);
        const float* t2 = rom.loop(p.w2, zone2);
        const std::vector<float>* atk = p.atk >= 0 ? &rom.attack(p.atk) : nullptr;
        const double nyq = 0.45 * sr;
        const double invSr = 1.0 / sr;

        for (int i0 = 0; i0 < n; i0 += kControl)
        {
            const int m = std::min(kControl, n - i0);
            // ---- control rate update
            const double dt = (double) m * invSr;
            const double lfo = std::sin(6.283185307179586 * v.lfoPh);
            v.lfoPh += p.lfoRate * g.lfoScale * dt; v.lfoPh -= std::floor(v.lfoPh);
            const double modAmt = std::max((double) mod[v.ch & 15], (double) joyY);
            const double bendSemi = ((double) bend[v.ch & 15] + joyX) * g.bendRange;
            const double vibr = lfo * (p.lfoPitch * g.lfoScale + modAmt * 0.5) + bendSemi;
            const double n1 = v.zoneNote + vibr - p.det * 0.005, n2 = v.zoneNote + p.semi2 + vibr + p.det * 0.005;
            const double f1 = 440.0 * std::exp2((n1 - 69.0) / 12.0), f2 = 440.0 * std::exp2((n2 - 69.0) / 12.0);
            v.inc1 = f1 * kTable * invSr; v.inc2 = f2 * kTable * invSr;
            v.atkInc = std::exp2(((double) v.zoneNote + vibr - 60.0) / 12.0) * (kAttackRate * invSr);

            double cut = p.cutoff * std::exp2(p.envAmt * v.fltLvl + p.keyTrack * (v.zoneNote - 60) / 12.0
                                              + (v.vel - 0.64) * 2.0 * p.velFlt + lfo * p.lfoFilt * g.lfoScale
                                              + modAmt * 0.8 + g.cutoffOffset * 2.0);
            cut = std::max(30.0, std::min(nyq, cut));
            const double res = std::min(0.95, (double) p.res + g.resOffset);
            v.fG = std::tan(3.14159265358979 * cut * invSr);
            v.fK = 2.0 - 1.95 * res;
            const double velG = 1.0 - p.velAmp + p.velAmp * v.vel;
            const double gainBase = v.level * velG * (1.0 + p.lfoAmp * lfo * 0.5);
            const double gD = 1.0 / (1.0 + v.fG * (v.fG + v.fK));

            // ---- per-sample
            for (int i = 0; i < m; ++i)
            {
                // oscillators (linear interpolation)
                int ix = (int) v.ph1; double fr = v.ph1 - ix;
                double s = (double) t1[ix] * (1.0 - fr) + (double) t1[ix + 1] * fr;
                if (p.lvl2 > 0.0001f)
                {
                    const int jx = (int) v.ph2; const double fr2 = v.ph2 - jx;
                    s += p.lvl2 * ((double) t2[jx] * (1.0 - fr2) + (double) t2[jx + 1] * fr2);
                    s *= 1.0 / (1.0 + 0.6 * p.lvl2);
                }
                v.ph1 += v.inc1; if (v.ph1 >= kTable) v.ph1 -= kTable;
                v.ph2 += v.inc2; if (v.ph2 >= kTable) v.ph2 -= kTable;

                if (p.xfade > 0.0f) s *= std::min(1.0, v.tNote / (double) p.xfade);
                if (atk && v.atkPos < (double) atk->size() - 1.0)
                {
                    const int a0 = (int) v.atkPos; const double af = v.atkPos - a0;
                    s += p.atkLvl * ((double) (*atk)[(size_t) a0] * (1.0 - af) + (double) (*atk)[(size_t) a0 + 1] * af);
                    v.atkPos += v.atkInc;
                }
                v.tNote += invSr;

                // 12 dB/oct low-pass state-variable filter
                const double hp = (s - (v.fK + v.fG) * v.s1 - v.s2) * gD;
                const double bp = v.fG * hp + v.s1; v.s1 = v.fG * hp + bp;
                const double lp = v.fG * bp + v.s2; v.s2 = v.fG * bp + lp;

                stepEnv(v.ampStage, v.ampLvl, v.releasing, aA, p.aD, p.aS, aR);
                stepEnv(v.fltStage, v.fltLvl, v.releasing, p.fA * g.attackScale, p.fD, p.fS, fR);
                const float o = (float) (lp * v.ampLvl * gainBase);
                l[i0 + i] += o * v.gl; r[i0 + i] += o * v.gr;
            }
            if ((v.releasing && v.ampLvl < 1.0e-5) || (v.ampStage >= 1 && v.ampLvl < 1.0e-5 && p.aS < 1.0e-4f))
            {
                v.active = false; return;
            }
        }
    }

    double sr = 44100.0;
    WaveRom rom;
    std::array<Voice, kVoices> voices;
    float bend[16] = {}, mod[16] = {}; bool pedal[16] = {};
    uint64_t ageCounter = 0;
    std::shared_ptr<const SampleBank> bank;
};
} // namespace pcm
