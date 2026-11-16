// Measured tests for the synth engine. Prints real numbers; exits non-zero on failure.
#include "../Source/Engine.h"
#include <chrono>
#include <cstdio>
#include <set>
#include <string>

using namespace pcm;
static int fails = 0;
static void check(bool ok, const char* name, const char* fmt = "", double a = 0, double b = 0, double c = 0)
{
    char buf[220]; std::snprintf(buf, sizeof buf, fmt, a, b, c);
    std::printf("[%s] %-46s %s\n", ok ? "PASS" : "FAIL", name, buf);
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
static double peak(const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (!to) { to = v.size(); }
    double p = 0; for (size_t i = from; i < to; ++i) p = std::max(p, (double) std::fabs(v[i])); return p;
}
static double rms(const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double) (to - from));
}
// amplitude at frequency f (Goertzel-style DFT)
static double tone(const std::vector<float>& x, double f, double sr, size_t from, size_t len)
{
    double re = 0, im = 0;
    for (size_t i = 0; i < len; ++i) { const double w = 6.283185307179586 * f * (double) i / sr; re += x[from + i] * std::cos(w); im += x[from + i] * std::sin(w); }
    return 2 * std::sqrt(re * re + im * im) / (double) len;
}
static double findFreq(const std::vector<float>& x, double sr, double guess)
{
    double best = guess, bv = 0;
    for (double f = guess * 0.97; f <= guess * 1.03; f += guess * 0.0002)
    { const double a = tone(x, f, sr, x.size() / 2, std::min<size_t>(x.size() / 2, 24000)); if (a > bv) { bv = a; best = f; } }
    return best;
}

int main()
{
    const double sr = 48000;
    Engine e;
    auto t0 = std::chrono::steady_clock::now();
    e.prepare(sr);
    const double buildSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(buildSec < 4.0, "wave ROM generation time", "%.2f s, %.2f M samples", buildSec, (double) e.waveRom().totalSamples() / 1e6);

    check(programs().size() == 101 && combinations().size() == 100, "100 programs + User Samples, 100 combinations", "%.0f / %.0f", (double) programs().size(), (double) combinations().size());
    { std::set<std::string> names; for (auto& p : programs()) names.insert(p.name); check(names.size() == 101, "all program names unique", "%.0f unique", (double) names.size()); }

    // ---- pitch accuracy
    for (int note : { 36, 69, 96 })
    {
        e.g.mode = ProgramMode; e.parts[0].prog = 2; e.parts[0].level = 1; e.parts[0].lo = 0; e.parts[0].hi = 127;
        e.allNotesOff(); run(e, 48000);
        e.noteOn(0, note, 0.8f); auto o = run(e, 192000);
        const double want = 440.0 * std::pow(2.0, (note - 69) / 12.0), got = findFreq(o.l, sr, want);
        check(std::fabs(1200 * std::log2(got / want)) < 3.0, "pitch accuracy (organ)", "note %.0f: %.2f Hz vs %.2f Hz", note, got, want);
    }

    // ---- polyphony and voice stealing
    {
        e.allNotesOff(); run(e, 96000);
        e.g.mode = ProgramMode; e.parts[0].prog = 4;
        for (int i = 0; i < 20; ++i) e.noteOn(0, 40 + i, 0.8f);
        auto o = run(e, 4800);
        check(e.activeVoices() == 16, "16-voice polyphony cap (20 notes in)", "%.0f voices active", e.activeVoices());
        check(peak(o.l) < 1.0 && std::isfinite(peak(o.l)), "chord of 16 stays under 0 dBFS", "peak %.3f", peak(o.l));
        e.allNotesOff(); auto tail = run(e, 48000 * 6);
        check(e.activeVoices() == 0 && peak(tail.l, tail.l.size() - 4800) < 1e-6, "voices release to silence", "%.0f active, last 0.1 s peak %.2g", e.activeVoices(), peak(tail.l, tail.l.size() - 4800));
    }

    // ---- multitimbral routing, key split, combination load
    {
        e.allNotesOff(); run(e, 96000);
        e.g.mode = MultiMode;
        const auto& cb = combinations()[19];
        for (int i = 0; i < 8; ++i) e.parts[(size_t) i] = cb.part[i];
        e.noteOn(1, 60, 0.8f);   // MIDI channel 2 -> part 2 only
        check(e.activeVoices() == 1, "multi mode: note on ch2 plays 1 part", "%.0f voice", e.activeVoices());
        e.allNotesOff(); run(e, 96000);
        e.g.mode = CombiMode; const auto& sp = combinations()[1];
        for (int i = 0; i < 8; ++i) e.parts[(size_t) i] = sp.part[i];
        e.noteOn(0, 40, 0.8f); const int lowV = e.activeVoices(); e.noteOn(0, 72, 0.8f);
        check(lowV == 1 && e.activeVoices() == 2, "combi split: bass below, piano above", "low note %.0f voice, then %.0f total", lowV, e.activeVoices());
        e.allNotesOff(); run(e, 96000);
        e.g.mode = CombiMode; const auto& ly = combinations()[0];
        for (int i = 0; i < 8; ++i) e.parts[(size_t) i] = ly.part[i];
        e.noteOn(0, 60, 0.8f);
        check(e.activeVoices() == 2, "combi layer: piano+strings = 2 voices", "%.0f voices", e.activeVoices());
        e.allNotesOff(); run(e, 96000);
    }

    // ---- every program: finite, audible, level spread
    {
        e.g.mode = ProgramMode; double minPk = 9, maxPk = 0; int bad = 0, silent = 0; std::string loudest, quietest;
        for (int p = 0; p < kNumPrograms; ++p)
            for (int note : { 36, 60, 84 })
            {
                e.allNotesOff(); run(e, 24000);
                e.parts[0].prog = p; e.parts[0].level = 1.0f; e.parts[0].lo = 0; e.parts[0].hi = 127; e.parts[0].transpose = 0;
                e.noteOn(0, note, 0.8f); auto o = run(e, 192000);
                const double pk = peak(o.l);
                if (!std::isfinite(pk) || pk > 1.0) ++bad;
                if (pk < 0.02) ++silent;
                if (note == 60) { if (pk < minPk) { minPk = pk; quietest = programs()[(size_t) p].name; } if (pk > maxPk) { maxPk = pk; loudest = programs()[(size_t) p].name; } }
            }
        check(bad == 0 && silent == 0, "all programs: finite, audible, no clipping", "%.0f bad, %.0f silent of 300 renders", bad, silent);
        std::printf("       level spread at C4: quietest %s %.3f (%.1f dBFS), loudest %s %.3f (%.1f dBFS)\n", quietest.c_str(), minPk, db(minPk), loudest.c_str(), maxPk, db(maxPk));
        check(db(maxPk) - db(minPk) < 14.0, "program loudness spread", "%.1f dB between quietest and loudest", db(maxPk) - db(minPk));
    }

    // ---- aliasing: bright saw, high note, filter wide open
    {
        Engine e2; e2.prepare(sr);
        Program p; p.w1 = 1; p.w2 = 0; p.lvl2 = 0; p.atk = -1; p.cutoff = 20000; p.res = 0; p.envAmt = 0; p.keyTrack = 0;
        p.aA = 0.001f; p.aD = 0.1f; p.aS = 1; p.aR = 0.1f; p.fS = 0; p.velAmp = 0; p.velFlt = 0; p.level = 0.5f; p.lfoPitch = 0;
        Voice v; v.active = true; v.prog = &p; v.note = 100; v.zoneNote = 100; v.vel = 0.8f; v.level = 0.5f; v.gl = v.gr = 1.0f;
        std::vector<float> l(48000), r(48000);
        e2.renderTestVoice(v, l.data(), r.data(), 48000);
        const double f0 = 440.0 * std::pow(2.0, (100 - 69) / 12.0);
        // remove harmonics (least squares) and compare what is left to the signal
        const size_t from = 8000, len = 36000; double tot = 0, harm = 0;
        for (size_t i = 0; i < len; ++i) tot += (double) l[from + i] * l[from + i];
        for (int h = 1; h * f0 < 0.5 * sr; ++h) { const double a = tone(l, h * f0, sr, from, len); harm += a * a / 2.0 * (double) len; }
        const double resid = std::max(tot - harm, 1e-12);
        check(db(std::sqrt(resid / tot)) < -40.0, "aliasing: saw at note 100", "non-harmonic energy %.1f dB below signal", db(std::sqrt(resid / tot)));
    }

    // ---- envelopes, velocity, joystick, pedal
    {
        e.g.mode = ProgramMode; e.parts[0].prog = 0; e.parts[0].level = 1; e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 60, 0.3f); auto soft = run(e, 24000); e.allNotesOff(); run(e, 48000);
        e.noteOn(0, 60, 1.0f); auto loud = run(e, 24000); e.allNotesOff(); run(e, 48000);
        check(peak(loud.l) > peak(soft.l) * 1.15, "velocity changes loudness", "vel 1.0: %.3f, vel 0.3: %.3f", peak(loud.l), peak(soft.l));

        e.noteOn(0, 60, 0.8f); run(e, 24000); const double base = findFreq(run(e, 48000).l, sr, 261.63); e.joyX = 1.0f;
        const double bent = findFreq(run(e, 48000).l, sr, 293.0); e.joyX = 0; e.allNotesOff(); run(e, 48000);
        check(std::fabs(1200 * std::log2(bent / base) - 200.0) < 8.0, "joystick pitch bend = 2 semitones", "%.1f cents", 1200 * std::log2(bent / base));

        e.pedalChange(0, true); e.noteOn(0, 60, 0.8f); run(e, 4800); e.noteOff(0, 60); auto held = run(e, 24000);
        const bool stillOn = e.activeVoices() == 1 && rms(held.l, 0, 24000) > 0.01;
        e.pedalChange(0, false); run(e, 96000);
        check(stillOn && e.activeVoices() == 0, "sustain pedal holds then releases", "held ok: %.0f", stillOn ? 1.0 : 0.0);
    }

    // ---- effects: finite, dry pass-through at mix 0, tails decay
    {
        for (int t = 1; t < FxCount; ++t)
        {
            Engine e3; e3.prepare(sr); e3.parts[0].prog = 0; e3.parts[0].level = 1;
            e3.fx[0].set(t, 0.6f, 0.5f, 0.5f);
            e3.noteOn(0, 60, 0.9f); e3.noteOn(0, 67, 0.9f); auto o = run(e3, 48000 * 2); e3.allNotesOff();
            auto tail = run(e3, 48000 * 12);
            const double pkAll = std::max(peak(o.l), peak(o.r));
            const double pkTail = std::max(peak(tail.l, tail.l.size() - 24000), peak(tail.r, tail.r.size() - 24000));
            char nm[60]; std::snprintf(nm, sizeof nm, "effect: %s", fxName(t));
            check(std::isfinite(pkAll) && pkAll < 1.0 && pkTail < 1e-4, nm, "peak %.3f, tail after 12 s: %.2g", pkAll, pkTail);
        }
        Engine a, b; a.prepare(sr); b.prepare(sr);
        a.parts[0].prog = 0; b.parts[0].prog = 0; a.parts[0].level = b.parts[0].level = 1;
        b.fx[0].set(FxReverb, 0.9f, 0.1f, 0.0f); b.fx[1].set(FxDelay, 0.5f, 0.5f, 0.0f);
        a.noteOn(0, 60, 0.8f); b.noteOn(0, 60, 0.8f); auto oa = run(a, 24000), ob = run(b, 24000);
        double d = 0; for (size_t i = 0; i < oa.l.size(); ++i) d = std::max(d, (double) std::fabs(oa.l[i] - ob.l[i]));
        check(d == 0.0, "effects at mix 0 leave signal untouched", "max difference %.3g", d);
    }

    // ---- CPU: 16 voices + 2 effects
    {
        Engine e4; e4.prepare(sr); e4.g.mode = CombiMode;
        const auto& st = combinations()[14]; for (int i = 0; i < 8; ++i) e4.parts[(size_t) i] = st.part[i];
        e4.fx[0].set(FxReverb, 0.7f, 0.4f, 0.3f); e4.fx[1].set(FxChorus, 0.4f, 0.5f, 0.4f);
        for (int i = 0; i < 6; ++i) e4.noteOn(0, 48 + i * 3, 0.8f);
        const int n = 48000 * 10; auto t1 = std::chrono::steady_clock::now(); auto o = run(e4, n, 512);
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
        check(sec < 3.0, "CPU: 16 voices + 2 effects", "%.2f s to render 10 s audio (%.1f%% of one core)", sec, sec * 10.0);
        (void) o;
    }

    std::printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
