// Measured tests for the user-sample engine. Prints real numbers; exits non-zero on failure.
#include "../Source/Engine.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace pcm;
static int fails = 0;
static void check(bool ok, const char* name, const char* fmt = "", double a = 0, double b = 0, double c = 0)
{
    char buf[220]; std::snprintf(buf, sizeof buf, fmt, a, b, c);
    std::printf("[%s] %-48s %s\n", ok ? "PASS" : "FAIL", name, buf);
    if (!ok) ++fails;
}
static double db(double x) { return 20 * std::log10(std::max(x, 1e-30)); }
struct Out { std::vector<float> l, r; };
static Out run(Engine& e, int n, int block = 256)
{
    Out o; o.l.assign((size_t) n, 0); o.r.assign((size_t) n, 0);
    for (int i = 0; i < n; i += block) e.process(o.l.data() + i, o.r.data() + i, std::min(block, n - i));
    return o;
}
static double tone(const std::vector<float>& x, double f, double sr, size_t from, size_t len)
{
    double re = 0, im = 0;
    for (size_t i = 0; i < len; ++i) { const double w = 6.283185307179586 * f * (double) i / sr; re += x[from + i] * std::cos(w); im += x[from + i] * std::sin(w); }
    return 2 * std::sqrt(re * re + im * im) / (double) len;
}
static double findFreq(const std::vector<float>& x, double sr, double guess)
{
    double best = guess, bv = 0;
    for (double f = guess * 0.985; f <= guess * 1.015; f += guess * 0.00005)
    { const double a = tone(x, f, sr, x.size() / 4, std::min<size_t>(x.size() / 2, 24000)); if (a > bv) { bv = a; best = f; } }
    return best;
}
static std::vector<float> sine(double f, double rate, double sec, double amp = 0.5)
{
    std::vector<float> v((size_t) (rate * sec)); for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (amp * std::sin(6.283185307179586 * f * (double) i / rate)); return v;
}
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double) (b - a)); }
static double peak(const std::vector<float>& v, size_t a = 0, size_t b = 0) { if (!b) b = v.size(); double p = 0; for (size_t i = a; i < b; ++i) p = std::max(p, (double) std::fabs(v[i])); return p; }

static std::shared_ptr<SampleBank> newBank() { return std::make_shared<SampleBank>(); }
static void setup(Engine& e, std::shared_ptr<SampleBank> b)
{
    e.g.mode = ProgramMode; e.parts[0] = PartCfg(); e.parts[0].prog = kUserProgram; e.parts[0].level = 1.0f;
    e.setBank(b);
}

int main()
{
    const double sr = 48000;
    Engine e; e.prepare(sr);

    // ---- pitch tracking across the keyboard, sample recorded at 44.1 kHz, host at 48 kHz
    {
        auto b = newBank(); (*b)[0].audio = makeAudio(sine(440.0, 44100, 3.0), {}, 44100); (*b)[0].root = 69; (*b)[0].mode = SmpLoop; (*b)[0].loopStart = 0.2f; (*b)[0].loopEnd = 0.9f;
        setup(e, b);
        for (int note : { 45, 57, 69, 81, 93 })
        {
            e.allNotesOff(); run(e, 48000);
            e.noteOn(0, note, 0.8f); auto o = run(e, 48000);
            const double want = 440.0 * std::pow(2.0, (note - 69) / 12.0), got = findFreq(o.l, sr, want);
            check(std::fabs(1200 * std::log2(got / want)) < 1.5, "sample pitch tracking", "note %.0f: %.3f Hz vs %.3f Hz", note, got, want);
        }
        e.allNotesOff(); run(e, 48000);
        // tune offset
        (*b)[0].tune = 50.0f; e.setBank(b); e.noteOn(0, 69, 0.8f); auto o = run(e, 48000);
        const double got = findFreq(o.l, sr, 440.0 * std::pow(2.0, 50.0 / 1200.0));
        check(std::fabs(1200 * std::log2(got / 440.0) - 50.0) < 1.5, "fine tune +50 cents", "%.2f cents", 1200 * std::log2(got / 440.0));
        e.allNotesOff(); run(e, 48000);
        // other host sample rates
        for (double hr : { 44100.0, 96000.0 })
        {
            Engine e2; e2.prepare(hr); (*b)[0].tune = 0; setup(e2, b); e2.noteOn(0, 69, 0.8f); auto o2 = run(e2, (int) hr);
            const double g2 = findFreq(o2.l, hr, 440.0);
            check(std::fabs(1200 * std::log2(g2 / 440.0)) < 1.5, "pitch at other host rates", "%.3f Hz @ %.0f Hz host", g2, hr);
        }
    }

    // ---- pitch detection
    {
        int note; float cents;
        Audio a1; a1.l = sine(220.0, 44100, 1.5); a1.rate = 44100;
        const bool ok1 = detectPitch(a1, note, cents);
        check(ok1 && note == 57 && std::fabs(cents) < 3, "detect pitch: 220 Hz sine", "note %.0f, %.1f cents", note, cents);
        Audio a2; a2.rate = 48000; a2.l.resize(48000);
        for (size_t i = 0; i < a2.l.size(); ++i) { double s = 0; for (int h = 1; h <= 12; ++h) s += std::sin(6.283185307179586 * 110.0 * h * (double) i / 48000.0) / h; a2.l[i] = (float) (0.3 * s); }
        const bool ok2 = detectPitch(a2, note, cents);
        check(ok2 && note == 45 && std::fabs(cents) < 3, "detect pitch: 110 Hz saw", "note %.0f, %.1f cents", note, cents);
        Audio a3; a3.rate = 44100; a3.l.resize(44100); std::mt19937 rng(3); std::uniform_real_distribution<float> d(-1, 1); for (auto& x : a3.l) x = d(rng);
        check(!detectPitch(a3, note, cents), "detect pitch: noise is rejected");
        Audio a4; a4.rate = 44100; a4.l = sine(261.63 * std::pow(2.0, 30.0 / 1200.0), 44100, 1.0);
        detectPitch(a4, note, cents);
        check(note == 60 && std::fabs(cents - 30.0f) < 4, "detect pitch: C4 + 30 cents", "note %.0f, %.1f cents", note, cents);
    }

    // ---- gate mode: plays to the end, release on note-off, voice freed
    {
        auto b = newBank(); (*b)[0].audio = makeAudio(sine(440.0, 44100, 0.5), {}, 44100); (*b)[0].root = 69; (*b)[0].mode = SmpGate;
        setup(e, b); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 69, 0.8f); run(e, 48000);
        check(e.activeVoices() == 0, "gate mode: voice frees at end of sample", "%.0f active after 1 s", e.activeVoices());
        e.noteOn(0, 69, 0.8f); run(e, 4800); e.noteOff(0, 69); auto o = run(e, 24000);
        check(peak(o.l, 20000, 24000) < 1e-3, "gate mode: note-off releases", "peak %.2g after release", peak(o.l, 20000, 24000));
    }

    // ---- loop mode: sustains indefinitely, stable level, seam is smooth
    {
        auto b = newBank();
        (*b)[0].audio = makeAudio(sine(440.7, 44100, 1.0), {}, 44100); (*b)[0].root = 69; (*b)[0].mode = SmpLoop; (*b)[0].loopStart = 0.25f; (*b)[0].loopEnd = 0.8037f;
        setup(e, b); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 69, 1.0f); auto o = run(e, 48000 * 8);
        const double r1 = rms(o.l, 24000, 48000), r2 = rms(o.l, 48000 * 7, 48000 * 8);
        check(e.activeVoices() == 1 && std::fabs(db(r2 / r1)) < 1.0, "loop mode: holds 8 s at steady level", "drift %.2f dB", db(r2 / r1));
        double maxStep = 0; for (size_t i = 24001; i < o.l.size(); ++i) maxStep = std::max(maxStep, (double) std::fabs(o.l[i] - o.l[i - 1]));
        const double amp = peak(o.l, 24000), idealStep = amp * 6.283185307179586 * 440.7 * 44100.0 / 44100.0 / sr;
        check(maxStep < idealStep * 1.35, "loop seam: no clicks", "largest step %.4f vs smooth sine %.4f", maxStep, idealStep);
        e.allNotesOff(); auto t = run(e, 48000);
        check(e.activeVoices() == 0 && peak(t.l, 40000, 48000) < 1e-5, "loop mode: releases to silence", "%.0f voices, tail %.2g", e.activeVoices(), peak(t.l, 40000, 48000));
    }

    // ---- one-shot: ignores note-off, plays to end, all-notes-off still kills it
    {
        auto b = newBank(); (*b)[0].audio = makeAudio(sine(300.0, 44100, 1.0), {}, 44100); (*b)[0].root = 60; (*b)[0].mode = SmpOneShot;
        setup(e, b); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 60, 0.8f); run(e, 2400); e.noteOff(0, 60); auto o = run(e, 24000);
        check(e.activeVoices() == 1 && rms(o.l, 12000, 24000) > 0.05, "one-shot: ignores note-off", "rms %.3f at 0.3 s", rms(o.l, 12000, 24000));
        run(e, 48000);
        check(e.activeVoices() == 0, "one-shot: ends with the sample", "%.0f voices", e.activeVoices());
        e.noteOn(0, 60, 0.8f); run(e, 2400); e.allNotesOff(); auto k = run(e, 24000);
        check(e.activeVoices() == 0 && peak(k.l, 20000, 24000) < 1e-3, "one-shot: all-notes-off stops it", "%.0f voices", e.activeVoices());
    }

    // ---- key and velocity mapping, auto-map, drum map
    {
        auto b = newBank();
        (*b)[0].audio = makeAudio(sine(200.0, 44100, 2.0), {}, 44100); (*b)[0].root = 36; (*b)[0].lo = 0; (*b)[0].hi = 59; (*b)[0].mode = SmpLoop;
        (*b)[1].audio = makeAudio(sine(500.0, 44100, 2.0), {}, 44100); (*b)[1].root = 69; (*b)[1].lo = 60; (*b)[1].hi = 127; (*b)[1].velLo = 1;  (*b)[1].velHi = 63;  (*b)[1].mode = SmpLoop;
        (*b)[2].audio = makeAudio(sine(900.0, 44100, 2.0), {}, 44100); (*b)[2].root = 69; (*b)[2].lo = 60; (*b)[2].hi = 127; (*b)[2].velLo = 64; (*b)[2].velHi = 127; (*b)[2].mode = SmpLoop;
        setup(e, b); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 40, 0.8f); const int v1 = e.activeVoices(); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 69, 0.3f); auto soft = run(e, 24000); const double fs = findFreq(soft.l, sr, 500.0); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 69, 1.0f); auto hard = run(e, 24000); const double fh = findFreq(hard.l, sr, 900.0);
        const double aSoft = tone(soft.l, 500.0, sr, 8000, 16000), aHard = tone(hard.l, 900.0, sr, 8000, 16000);
        check(v1 == 1, "key range: low note plays only the low zone", "%.0f voice", v1);
        check(aSoft > 0.05 && aHard > 0.05 && std::fabs(fs - 500) < 2 && std::fabs(fh - 900) < 2, "velocity layers pick the right sample", "soft vel -> %.1f Hz, hard vel -> %.1f Hz", fs, fh);
        e.allNotesOff(); run(e, 48000);
        SampleBank m; m[0].audio = (*b)[0].audio; m[0].root = 48; m[1].audio = (*b)[0].audio; m[1].root = 60; m[2].audio = (*b)[0].audio; m[2].root = 72;
        autoMapKeys(m);
        check(m[0].lo == 0 && m[0].hi == 54 && m[1].lo == 55 && m[1].hi == 66 && m[2].lo == 67 && m[2].hi == 127, "auto-map keys", "zones end at %.0f, %.0f, %.0f", m[0].hi, m[1].hi, m[2].hi);
        SampleBank dm; for (int i = 0; i < 4; ++i) dm[(size_t) i].audio = (*b)[0].audio; dm[7].audio = (*b)[0].audio;
        drumMap(dm, 36);
        check(dm[0].lo == 36 && dm[3].hi == 39 && dm[7].lo == 40 && dm[2].mode == SmpOneShot, "drum map from C1", "keys %.0f..%.0f, slot 7 -> %.0f", dm[0].lo, dm[3].hi, dm[7].lo);
    }

    // ---- stereo, pan, filter
    {
        auto b = newBank(); (*b)[0].audio = makeAudio(sine(440.0, 44100, 1.0), std::vector<float>(44100, 0.0f), 44100); (*b)[0].root = 69; (*b)[0].mode = SmpLoop;
        setup(e, b); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 69, 0.9f); auto o = run(e, 24000);
        check(peak(o.l, 8000) > 0.1 && peak(o.r, 8000) < 1e-6, "stereo sample keeps its image", "L %.3f, R %.2g", peak(o.l, 8000), peak(o.r, 8000));
        e.allNotesOff(); run(e, 48000);
        auto b2 = newBank(); (*b2)[0].audio = makeAudio(sine(440.0, 44100, 1.0), {}, 44100); (*b2)[0].root = 69; (*b2)[0].mode = SmpLoop; (*b2)[0].pan = -1.0f;
        e.setBank(b2); e.noteOn(0, 69, 0.9f); auto p = run(e, 24000);
        check(peak(p.l, 8000) > 0.1 && peak(p.r, 8000) < 1e-3, "pan hard left", "L %.3f, R %.2g", peak(p.l, 8000), peak(p.r, 8000));
        e.allNotesOff(); run(e, 48000);
        auto b3 = newBank(); (*b3)[0].audio = makeAudio(sine(4000.0, 44100, 1.0), {}, 44100); (*b3)[0].root = 69; (*b3)[0].mode = SmpLoop;
        e.setBank(b3); e.noteOn(0, 69, 0.9f); auto open = run(e, 24000); e.allNotesOff(); run(e, 48000);
        (*b3)[0].cutoff = 500.0f; e.setBank(b3); e.noteOn(0, 69, 0.9f); auto filt = run(e, 24000);
        check(db(peak(filt.l, 8000) / peak(open.l, 8000)) < -20, "sample low-pass filter", "%.1f dB at 4 kHz with 500 Hz cutoff", db(peak(filt.l, 8000) / peak(open.l, 8000)));
        e.allNotesOff(); run(e, 48000);
    }

    // ---- bank swap during playback, 16 stereo looping voices (CPU)
    {
        auto b = newBank(); (*b)[0].audio = makeAudio(sine(440.0, 44100, 2.0), sine(660.0, 44100, 2.0), 44100); (*b)[0].root = 69; (*b)[0].mode = SmpLoop; (*b)[0].cutoff = 6000.0f;
        setup(e, b); e.allNotesOff(); run(e, 48000);
        for (int i = 0; i < 16; ++i) e.noteOn(0, 48 + i, 0.8f);
        run(e, 4800); e.setBank(newBank()); auto o = run(e, 4800);
        check(e.activeVoices() == 16 && peak(o.l) > 0.01 && std::isfinite(peak(o.l)), "swapping the bank mid-play is safe", "%.0f voices still sounding", e.activeVoices());
        e.setBank(b);
        const int n = 48000 * 10; auto t0 = std::chrono::steady_clock::now(); auto big = run(e, n, 512);
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        check(sec < 3.0, "CPU: 16 stereo looping sample voices", "%.2f s for 10 s audio (%.1f%% of one core)", sec, sec * 10.0);
        (void) big;
    }

    std::printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
