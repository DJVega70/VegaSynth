#pragma once
#include <JuceHeader.h>
#include "Engine.h"
#include "SampleTools.h"
#include "PianoSet.h"

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

    // ---- real piano (multisampled folder, e.g. Salamander Grand Piano) - loaded on a background thread
    void loadPianoFolder(const juce::File& folder, int quality);      // quality 0 = 4 layers, 1 = 8, 2 = all
    void setPianoEnabled(bool on) { pianoEnabled = on; }
    bool isPianoEnabled() const { return pianoEnabled.load(); }
    juce::String getPianoStatus() const { const juce::ScopedLock sl(pianoLock); return pianoStatus; }
    juce::File getPianoFolder() const { const juce::ScopedLock sl(pianoLock); return pianoFolder; }
    int getPianoQuality() const { return pianoQuality.load(); }
    std::atomic<float> pianoProgress { 0.0f };
    std::atomic<bool> pianoBusy { false }, pianoReady { false };

private:
    class PianoLoader;
    void startDefaultPianoLoad();
    void savePianoDefault() const;
    mutable juce::CriticalSection pianoLock;
    juce::String pianoStatus { "No piano folder loaded - using the built-in synth piano." };
    juce::File pianoFolder;
    std::atomic<int> pianoQuality { 1 };
    std::atomic<bool> pianoEnabled { true }, pianoRequested { false };
    std::shared_ptr<const pcm::PianoSet> pianoPub, pianoHeld, pianoRetired, audioPiano;
    bool audioPianoOn = false;
    std::unique_ptr<PianoLoader> pianoLoader;

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
