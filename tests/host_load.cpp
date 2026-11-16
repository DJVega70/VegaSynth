// Loads the built VST3 through JUCE's plugin host, the way a DAW does, and runs audio + MIDI through it.
#include <JuceHeader.h>
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioPluginFormatManager fm; fm.addFormat(new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> found;
    fm.getFormat(0)->findAllTypesForFile(found, argv[1]);
    std::printf("found %d plugin(s) in bundle\n", found.size());
    if (found.isEmpty()) return 1;
    juce::String err;
    auto inst = fm.createPluginInstance(*found[0], 48000, 512, err);
    std::printf("instance: %s %s\n", inst ? "OK" : "FAILED", err.toRawUTF8());
    if (!inst) return 2;
    std::printf("name %s, params %d, in ch %d, out ch %d, synth %d\n", inst->getName().toRawUTF8(), inst->getParameters().size(), inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels(), (int) found[0]->isInstrument);
    inst->prepareToPlay(48000, 512);
    juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    float pk = 0; for (int k = 0; k < 40; ++k) { inst->processBlock(b, m); m.clear(); if (k > 3) pk = std::max(pk, b.getMagnitude(0, 512)); }
    std::printf("audio peak after note on: %.3f\n", pk);
    std::unique_ptr<juce::AudioProcessorEditor> ed(inst->createEditor()); std::printf("editor: %s\n", ed ? "OK" : "none");
    return 0;
}
