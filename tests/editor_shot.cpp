// Renders the editor tabs to PNG files. Usage: editor_shot <outdir> [sample.wav ...]
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
static juce::Button* findButton(juce::Component* c, const juce::String& text)
{
    if (auto* b = dynamic_cast<juce::Button*>(c)) if (b->getButtonText() == text) return b;
    for (auto* ch : c->getChildren()) if (auto* r = findButton(ch, text)) return r;
    return nullptr;
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out(argv[1]); out.createDirectory();
    PcmSynthProcessor pr; pr.setPlayConfigDetails(0, 2, 48000, 512); pr.prepareToPlay(48000, 512);
    for (int i = 2; i < argc && i - 2 < 16; ++i) { juce::String e; pr.loadSampleFile(i - 2, juce::File(argv[i]), e); }
    pr.doDrumMap();
    std::unique_ptr<juce::AudioProcessorEditor> ed(pr.createEditor()); ed->setSize(980, 700);
    ed->setVisible(true);
    auto shot = [&](const char* name) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png; juce::FileOutputStream os(out.getChildFile(name)); os.setPosition(0); os.truncate(); png.writeImageToStream(img, os); };
    auto click = [&](const char* t) { if (auto* b = findButton(ed.get(), t)) b->triggerClick(); else std::printf("button %s not found\n", t); };
    shot("tab_synth.png");
    click("SAMPLES"); shot("tab_samples.png");
    click("PADS"); pr.keyboardState.noteOn(1, kPadBase + 1, 0.9f); pr.keyboardState.noteOn(1, kPadBase + 6, 0.4f);
    { juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m; pr.processBlock(b, m); }
    shot("tab_pads.png");
    return 0;
}
