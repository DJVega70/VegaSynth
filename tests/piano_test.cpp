// Piano loader checks. Usage: piano_test <folder-of-fake-salamander-files>
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
static float peak(PcmSynthProcessor& pr, int note, int vel, double* freq = nullptr)
{
    juce::MidiBuffer off; off.addEvent(juce::MidiMessage::allNotesOff(1), 0); { juce::AudioBuffer<float> b(2, 512); for (int k = 0; k < 120; ++k) { juce::MidiBuffer m; if (k == 0) m = off; pr.processBlock(b, m); } }
    juce::AudioBuffer<float> b(2, 512), acc(1, 512 * 30); float pk = 0;
    for (int k = 0; k < 30; ++k) { juce::MidiBuffer m; if (k == 0) m.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) vel), 0); pr.processBlock(b, m); pk = std::max(pk, b.getMagnitude(0, 512)); acc.copyFrom(0, 512 * k, b, 0, 0, 512); }
    if (freq && note == 21) { FILE* fo = std::fopen("/tmp/a0.txt", "w"); const float* d = acc.getReadPointer(0); for (int i = 0; i < 512 * 30; i += 20) std::fprintf(fo, "%d %f\n", i, d[i]); std::fclose(fo); }
    if (freq) { const float* d = acc.getReadPointer(0); int z = 0; double first = -1, last = 0; for (int i = 600; i < 512 * 30 - 1; ++i) if (d[i] <= 0 && d[i + 1] > 0) { const double t = i + (-d[i]) / (d[i + 1] - d[i]); if (first < 0) first = t; last = t; ++z; } *freq = z > 1 ? (z - 1) * 48000.0 / (last - first) : 0; std::printf("  note %d: measured %.2f Hz\n", note, *freq); }
    return pk;
}
int main(int, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init; int fails = 0;
    auto chk = [&](bool ok, const char* m) { std::printf("%s  %s\n", ok ? "ok  " : "FAIL", m); if (!ok) ++fails; };
    int midi, layer;
    chk(pcm::parsePianoName("A0v1", midi, layer) && midi == 21 && layer == 1, "A0v1 -> 21, layer 1");
    chk(pcm::parsePianoName("D#1v16", midi, layer) && midi == 27 && layer == 16, "D#1v16 -> 27, layer 16");
    chk(pcm::parsePianoName("Fs3v8", midi, layer) && midi == 54 && layer == 8, "Fs3v8 -> 54");
    chk(pcm::parsePianoName("Bb2_v3", midi, layer) && midi == 46 && layer == 3, "Bb2_v3 -> 46");
    chk(pcm::parsePianoName("C4", midi, layer) && midi == 60 && layer == 1, "C4 -> 60, layer 1");
    chk(pcm::parsePianoName("C8v10", midi, layer) && midi == 108, "C8v10 -> 108");
    chk(!pcm::parsePianoName("rel12", midi, layer) && !pcm::parsePianoName("pedalD1", midi, layer) && !pcm::parsePianoName("harmD3", midi, layer), "release / pedal / harmonic files ignored");

    PcmSynthProcessor pr; pr.setPlayConfigDetails(0, 2, 48000, 512); pr.prepareToPlay(48000, 512);
    for (const char* id : { "fx1mix", "fx2mix" }) { auto* q = pr.apvts.getParameter(id); q->setValueNotifyingHost(0.0f); }   // effects would shift zero crossings in the pitch check
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    pr.loadPianoFolder(juce::File(argv[1]), 1);
    while (pr.pianoBusy.load() || !pr.pianoReady.load()) juce::Thread::sleep(20);
    std::printf("load took %.2f s: %s\n", (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, pr.getPianoStatus().toRawUTF8());
    chk(pr.getPianoStatus().contains("30 notes x 8 layers"), "30 notes x 8 layers (Medium keeps 8 of 16)");
    { juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m; pr.processBlock(b, m); }     // lets the audio thread pick up the piano
    double f; 
    peak(pr, 60, 100, &f); chk(std::fabs(f - 261.6) < 2.0, "C4 plays at 261.6 Hz");
    peak(pr, 61, 100, &f); chk(std::fabs(f - 277.2) < 2.5, "C#4 (pitched up from C4 sample) plays at 277.2 Hz");
    peak(pr, 62, 100, &f); chk(std::fabs(f - 293.7) < 2.5, "D4 (pitched down from D#4 sample) plays at 293.7 Hz");
    peak(pr, 24, 100, &f); chk(std::fabs(f - 32.7) < 0.6, "C1 plays at 32.7 Hz");
    peak(pr, 21, 100, &f); std::printf("A0 measured %.2f Hz\n", f); chk(std::fabs(f - 27.5) < 1.0, "A0 plays at 27.5 Hz");
    const float soft = peak(pr, 60, 20), hard = peak(pr, 60, 120);
    std::printf("C4 velocity 20 peak %.3f, velocity 120 peak %.3f\\n", soft, hard);
    chk(hard > soft * 2.0f, "hard velocity picks a louder recorded layer");
    pr.setPianoEnabled(false); { juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m; pr.processBlock(b, m); }
    { const float sp = peak(pr, 60, 100); std::printf("synth piano peak %.3f\n", sp); chk(sp > 0.02f, "with the real piano switched off the synth piano plays instead"); }
    pr.setPianoEnabled(true);
    juce::MemoryBlock mb; pr.getStateInformation(mb);
    PcmSynthProcessor p2; p2.setPlayConfigDetails(0, 2, 48000, 512); p2.setStateInformation(mb.getData(), (int) mb.getSize()); p2.prepareToPlay(48000, 512);
    while (p2.pianoBusy.load() || !p2.pianoReady.load()) juce::Thread::sleep(20);
    { juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m; p2.processBlock(b, m); }
    peak(p2, 60, 100, &f); chk(std::fabs(f - 261.6) < 2.0 && p2.getPianoFolder() == juce::File(argv[1]), "project reload remembers the folder and reloads the piano");
    pr.loadPianoFolder(juce::File("/nonexistent_dir_xyz"), 1);
    while (pr.pianoBusy.load()) juce::Thread::sleep(20); juce::Thread::sleep(100);
    std::printf("bad folder status: %s\n", pr.getPianoStatus().toRawUTF8());
    std::printf("\n%s (%d failures)\n", fails ? "SOME CHECKS FAILED" : "ALL PIANO CHECKS PASSED", fails);
    return fails;
}
