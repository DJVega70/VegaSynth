#pragma once
#include <JuceHeader.h>
#include "Engine.h"
#include "SampleTools.h"

constexpr int kPadBase = 36;   // the 16 touch pads play keys 36..51 (C1 up)

class PcmSynthProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::AudioProcessorValueTreeState::Listener
{
public:
    PcmSynthProcessor();
    ~PcmSynthProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PCM Workstation"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // read by the editor
    std::atomic<float> outPeak { 0.0f };
    std::atomic<int> voicesActive { 0 };

    void loadCombination(int index);   // copies a combination into the 8 part parameters

    // ---- user samples (all called from the message thread)
    pcm::SampleSlot getSlot(int i) const;
    void setSlot(int i, const pcm::SampleSlot& s);
    bool loadSampleFile(int i, const juce::File& f, juce::String& error);
    void clearSlot(int i);
    void doAutoMap();
    void doDrumMap();
    bool doDetectRoot(int i);
    void useSamplesInProgramMode();
    std::atomic<int> slotsVersion { 0 };
    std::array<std::atomic<float>, 16> padHit;   // last hit velocity per pad (0..1), set by incoming MIDI

private:
    void parameterChanged(const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void applyParameters();
    float p(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }

    void publishBank();
    juce::ValueTree samplesToTree() const;
    void samplesFromTree(const juce::ValueTree&);
    mutable juce::CriticalSection slotLock;
    pcm::SampleBank slots;
    std::shared_ptr<const pcm::SampleBank> published, audioBank;
    juce::AudioFormatManager formats;

    pcm::Engine engine;
    juce::MidiBuffer keyboardEvents;
    std::atomic<bool> restoring { false };
    std::atomic<int> pendingCombi { -1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PcmSynthProcessor)
};
