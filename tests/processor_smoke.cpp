// Smoke test of the real plugin processor with MIDI. Not shipped in the plugin.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
static float runBlocks(PcmSynthProcessor& pr, juce::MidiBuffer midi, int blocks, int& sampleCount)
{
    juce::AudioBuffer<float> b(2, 512); float pk = 0;
    for (int k = 0; k < blocks; ++k)
    {
        juce::MidiBuffer m; if (k == 0) m = midi;
        pr.processBlock(b, m); if (k > 2) pk = std::max(pk, b.getMagnitude(0, 512)); sampleCount += 512;
    }
    return pk;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    PcmSynthProcessor pr; pr.setPlayConfigDetails(0, 2, 48000, 512); pr.prepareToPlay(48000, 512);
    auto set = [&](const char* id, float v) { auto* p = pr.apvts.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(v)); };
    int n = 0;
    juce::MidiBuffer on; on.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    std::printf("program mode, note on -> peak %.3f (expect > 0.02)\n", runBlocks(pr, on, 60, n));
    juce::MidiBuffer off; off.addEvent(juce::MidiMessage::allNotesOff(1), 0); runBlocks(pr, off, 400, n);
    set("mode", 1.0f); set("combi", 1.0f);   // combination 1: bass/piano split
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    std::printf("combi 1 loaded: part1 prog = %d (expect 17), part2 prog = %d (expect 0), part1 hi key = %d (expect 47)\n",
        juce::roundToInt(pr.apvts.getRawParameterValue("p1prog")->load()), juce::roundToInt(pr.apvts.getRawParameterValue("p2prog")->load()),
        juce::roundToInt(pr.apvts.getRawParameterValue("p1hi")->load()));
    juce::MidiBuffer lo; lo.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100), 0);
    { const float pk = runBlocks(pr, lo, 30, n); std::printf("split, low note -> peak %.3f, voices %d (expect 1)\n", pk, pr.voicesActive.load()); }
    runBlocks(pr, off, 400, n);
    juce::MidiBuffer hi; hi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8) 100), 0);
    runBlocks(pr, hi, 10, n); std::printf("split, high note -> voices %d (expect 1)\n", pr.voicesActive.load());
    runBlocks(pr, off, 400, n);
    juce::MemoryBlock mb; pr.getStateInformation(mb);
    PcmSynthProcessor p2; p2.setStateInformation(mb.getData(), (int) mb.getSize());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    std::printf("state restore keeps edited parts: p1prog = %d (expect 17), state %zu bytes\n", juce::roundToInt(p2.apvts.getRawParameterValue("p1prog")->load()), mb.getSize());
    // ---- user samples: real WAV file -> slot -> pad key -> sound -> state round trip
    {
        auto wav = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("smoke_kick.wav");
        wav.deleteFile();
        { juce::WavAudioFormat fmt; std::unique_ptr<juce::FileOutputStream> os(wav.createOutputStream());
          std::unique_ptr<juce::AudioFormatWriter> w(fmt.createWriterFor(os.get(), 44100.0, 2, 16, {}, 0)); if (w) os.release();
          juce::AudioBuffer<float> sb(2, 22050); for (int i = 0; i < 22050; ++i) { const float v = 0.8f * std::sin(6.2831853f * 110.0f * i / 44100.0f) * std::exp(-i / 6000.0f); sb.setSample(0, i, v); sb.setSample(1, i, v); }
          w->writeFromAudioSampleBuffer(sb, 0, 22050); }
        juce::String err; const bool ok = pr.loadSampleFile(0, wav, err);
        std::printf("load WAV into slot 1: %s, frames %zu (expect 22050), rate %.0f\n", ok ? "ok" : "FAILED", pr.getSlot(0).audio ? pr.getSlot(0).audio->frames() : 0, pr.getSlot(0).audio ? pr.getSlot(0).audio->rate : 0.0);
        pr.doDrumMap(); pr.useSamplesInProgramMode();
        std::printf("use-in-program: mode %d, p1prog %d (expect 0, 100)\n", juce::roundToInt(pr.apvts.getRawParameterValue("mode")->load()), juce::roundToInt(pr.apvts.getRawParameterValue("p1prog")->load()));
        for (int vel : { 30, 120 })
        {
            juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, kPadBase, (juce::uint8) vel), 0);
            juce::AudioBuffer<float> b(2, 512); pr.processBlock(b, m); float pk = b.getMagnitude(0, 512);
            std::printf("pad 1 (key %d) vel %3d -> peak %.3f, voices %d\n", kPadBase, vel, pk, pr.voicesActive.load());
            runBlocks(pr, off, 100, n);
        }
        { juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, kPadBase + 5, (juce::uint8) 100), 0); juce::AudioBuffer<float> b(2, 512); pr.processBlock(b, m);
          std::printf("empty pad 6 -> peak %.4f (expect 0)\n", b.getMagnitude(0, 512)); }
        runBlocks(pr, off, 100, n);
        juce::MemoryBlock sm; pr.getStateInformation(sm);
        PcmSynthProcessor p3; p3.setPlayConfigDetails(0, 2, 48000, 512); p3.prepareToPlay(48000, 512);
        p3.setStateInformation(sm.getData(), (int) sm.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        const auto s3 = p3.getSlot(0);
        std::printf("state with sample: %zu bytes; restored name '%s', frames %zu, key %d-%d, mode %d (expect smoke_kick, 22050, 36-36, 2)\n", sm.getSize(), s3.name.c_str(), s3.audio ? s3.audio->frames() : 0, s3.lo, s3.hi, s3.mode);
        juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, kPadBase, (juce::uint8) 100), 0); juce::AudioBuffer<float> b(2, 512); p3.processBlock(b, m);
        std::printf("restored plugin plays the pad -> peak %.3f\n", b.getMagnitude(0, 512));
        // time to publish bank during playback: no crash
        for (int k = 0; k < 50; ++k) { juce::MidiBuffer mm; if (k % 5 == 0) mm.addEvent(juce::MidiMessage::noteOn(1, kPadBase, (juce::uint8) 90), 0); pr.setSlot(0, pr.getSlot(0)); pr.processBlock(b, mm); }
        std::printf("bank swapped 50x while playing: ok\n");
    }
    return 0;
}
