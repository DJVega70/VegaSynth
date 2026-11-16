#pragma once
// Multi-effect unit: reverb, delay, chorus, flanger, distortion, rotary speaker. Stereo, in place.
#include <algorithm>
#include <cmath>
#include <vector>

namespace pcm
{
enum FxType { FxOff = 0, FxReverb, FxDelay, FxChorus, FxFlanger, FxDistortion, FxRotary, FxCount };
inline const char* fxName(int t)
{
    static const char* n[FxCount] = { "Off", "Reverb", "Delay", "Chorus", "Flanger", "Distortion", "Rotary" };
    return n[std::max(0, std::min((int) FxCount - 1, t))];
}

class Effect
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        const double k = sr / 44100.0;
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, apT[4] = { 556, 441, 341, 225 };
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 8; ++i) { comb[c][i].buf.assign((size_t) ((combT[i] + c * 23) * k) + 1, 0.0f); comb[c][i].pos = 0; comb[c][i].lp = 0; }
            for (int i = 0; i < 4; ++i) { ap[c][i].buf.assign((size_t) ((apT[i] + c * 23) * k) + 1, 0.0f); ap[c][i].pos = 0; }
            dly[c].assign((size_t) (sr * 2.0) + 4, 0.0f);
            mod[c].assign((size_t) (sr * 0.12) + 4, 0.0f);
        }
        rotBuf[0].assign(256, 0.0f); rotBuf[1].assign(256, 0.0f);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& x : comb[c]) { std::fill(x.buf.begin(), x.buf.end(), 0.0f); x.lp = 0; }
            for (auto& x : ap[c]) std::fill(x.buf.begin(), x.buf.end(), 0.0f);
            std::fill(dly[c].begin(), dly[c].end(), 0.0f);
            std::fill(mod[c].begin(), mod[c].end(), 0.0f);
            std::fill(rotBuf[c].begin(), rotBuf[c].end(), 0.0f);
            toneLp[c] = 0; xoLp[c] = 0; fbState[c] = 0;
        }
        dPos = mPos = rPos = 0; lfo = 0; rotH = rotD = 0; rotSpeedCur = 0;
    }

    void set(int t, float a_, float b_, float m_)
    {
        if (t != type) { type = t; reset(); }
        a = a_; b = b_; mix = m_;
    }

    void process(float* l, float* r, int n)
    {
        if (type == FxOff || mix <= 0.0001f) return;
        switch (type)
        {
            case FxReverb: reverb(l, r, n); break;
            case FxDelay: delay(l, r, n); break;
            case FxChorus: modFx(l, r, n, false); break;
            case FxFlanger: modFx(l, r, n, true); break;
            case FxDistortion: distortion(l, r, n); break;
            case FxRotary: rotary(l, r, n); break;
            default: break;
        }
    }

private:
    struct Comb { std::vector<float> buf; size_t pos = 0; float lp = 0; };
    struct Ap { std::vector<float> buf; size_t pos = 0; };

    void reverb(float* l, float* r, int n)
    {
        const float fb = 0.70f + 0.28f * a, damp = 0.15f + 0.65f * b, wetMix = mix;
        for (int i = 0; i < n; ++i)
        {
            const float in = (l[i] + r[i]) * 0.015f;
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                float s = 0;
                for (auto& cb : comb[c])
                {
                    const float y = cb.buf[cb.pos];
                    cb.lp = y * (1.0f - damp) + cb.lp * damp;
                    cb.buf[cb.pos] = in + cb.lp * fb;
                    if (++cb.pos >= cb.buf.size()) cb.pos = 0;
                    s += y;
                }
                for (auto& p : ap[c])
                {
                    const float y = p.buf[p.pos];
                    p.buf[p.pos] = s + y * 0.5f;
                    if (++p.pos >= p.buf.size()) p.pos = 0;
                    s = y - s;
                }
                out[c] = s;
            }
            l[i] = l[i] * (1.0f - 0.35f * wetMix) + out[0] * 2.2f * wetMix;
            r[i] = r[i] * (1.0f - 0.35f * wetMix) + out[1] * 2.2f * wetMix;
        }
    }

    void delay(float* l, float* r, int n)
    {
        const double target = (0.04 + 0.86 * a) * sr;
        const float fb = 0.9f * b;
        const size_t size = dly[0].size();
        for (int i = 0; i < n; ++i)
        {
            dTime += (target - dTime) * 0.0005;
            double rp = (double) dPos - dTime; while (rp < 0) rp += (double) size;
            const size_t i0 = (size_t) rp % size, i1 = (i0 + 1) % size; const float fr = (float) (rp - std::floor(rp));
            const float tl = dly[0][i0] * (1 - fr) + dly[0][i1] * fr, tr = dly[1][i0] * (1 - fr) + dly[1][i1] * fr;
            dly[0][dPos] = l[i] + tr * fb;           // ping-pong
            dly[1][dPos] = r[i] + tl * fb;
            if (++dPos >= size) dPos = 0;
            l[i] += tl * mix; r[i] += tr * mix;
        }
    }

    void modFx(float* l, float* r, int n, bool flanger)
    {
        const double rate = flanger ? 0.05 + 2.0 * a : 0.1 + 3.0 * a;
        const double baseMs = flanger ? 3.5 : 14.0, depthMs = flanger ? 2.8 * (0.2 + b) : 6.0 * (0.15 + b);
        const float fb = flanger ? 0.8f * b : 0.0f;
        const size_t size = mod[0].size();
        for (int i = 0; i < n; ++i)
        {
            lfo += rate / sr; if (lfo >= 1.0) lfo -= 1.0;
            float in[2] = { l[i], r[i] }, wet[2];
            for (int c = 0; c < 2; ++c)
            {
                const double ph = lfo + (c ? 0.25 : 0.0);
                const double dMs = baseMs + depthMs * std::sin(6.283185307179586 * ph);
                double rp = (double) mPos - dMs * 0.001 * sr; while (rp < 0) rp += (double) size;
                const size_t i0 = (size_t) rp % size, i1 = (i0 + 1) % size; const float fr = (float) (rp - std::floor(rp));
                wet[c] = mod[c][i0] * (1 - fr) + mod[c][i1] * fr;
                mod[c][mPos] = in[c] + wet[c] * fb;
            }
            if (++mPos >= size) mPos = 0;
            l[i] = in[0] * (flanger ? 1.0f : 1.0f - 0.4f * mix) + wet[0] * mix * (flanger ? 0.8f : 0.9f);
            r[i] = in[1] * (flanger ? 1.0f : 1.0f - 0.4f * mix) + wet[1] * mix * (flanger ? 0.8f : 0.9f);
        }
    }

    void distortion(float* l, float* r, int n)
    {
        const float drive = 1.0f + a * 30.0f;
        const float cutoff = 800.0f + 9000.0f * b;
        const float c = 1.0f - std::exp(-6.283185307f * cutoff / (float) sr);
        const float comp = 1.0f / std::tanh(drive * 0.6f + 0.01f) * 0.5f;
        float* ch[2] = { l, r };
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < n; ++i)
            {
                const float dry = ch[k][i];
                float w = std::tanh(drive * dry) * comp * 0.6f;
                toneLp[k] += c * (w - toneLp[k]);
                ch[k][i] = dry * (1.0f - mix) + toneLp[k] * mix;
            }
    }

    void rotary(float* l, float* r, int n)
    {
        const double speedTarget = a;                     // 0 = slow, 1 = fast
        const float depth = 0.2f + 0.8f * b;
        const float xc = 1.0f - std::exp(-6.283185307f * 800.0f / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            rotSpeedCur += (speedTarget - rotSpeedCur) * (1.0 / (1.2 * sr));     // rotor inertia
            const double hornHz = 0.8 + 5.9 * rotSpeedCur, drumHz = 0.66 + 4.9 * rotSpeedCur;
            rotH += hornHz / sr; rotH -= std::floor(rotH);
            rotD += drumHz / sr; rotD -= std::floor(rotD);
            const float m = 0.5f * (l[i] + r[i]);
            xoLp[0] += xc * (m - xoLp[0]);
            const float low = xoLp[0], high = m - low;
            const float sh = (float) std::sin(6.283185307179586 * rotH), sd = (float) std::sin(6.283185307179586 * rotD + 1.0);
            const float hl = high * (1.0f + 0.5f * depth * sh), hr = high * (1.0f - 0.5f * depth * sh);
            const float dl = low * (1.0f + 0.35f * depth * sd), dr = low * (1.0f - 0.35f * depth * sd);
            const float wl = hl + dl, wr = hr + dr;
            l[i] = l[i] * (1.0f - mix) + wl * mix; r[i] = r[i] * (1.0f - mix) + wr * mix;
        }
    }

    double sr = 44100.0;
    int type = FxOff; float a = 0.5f, b = 0.5f, mix = 0.0f;
    Comb comb[2][8]; Ap ap[2][4];
    std::vector<float> dly[2], mod[2], rotBuf[2];
    size_t dPos = 0, mPos = 0, rPos = 0; double dTime = 0.3 * 44100.0;
    double lfo = 0, rotH = 0, rotD = 0, rotSpeedCur = 0;
    float toneLp[2] = {}, xoLp[2] = {}, fbState[2] = {};
};
} // namespace pcm
