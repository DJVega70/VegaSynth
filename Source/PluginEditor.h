#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PcmSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit PcmSynthEditor(PcmSynthProcessor&);
    ~PcmSynthEditor() override;
    void resized() override;
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

    static constexpr int kBaseW = 980, kBaseH = 700;
private:
    class Content;
    std::unique_ptr<Content> content;
    juce::ComponentBoundsConstrainer constrainer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PcmSynthEditor)
};
