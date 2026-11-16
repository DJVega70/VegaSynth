#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String noteName(int n) { static const char* nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return juce::String(nm[n % 12]) + juce::String(n / 12 - 2); }
}

juce::AudioProcessorValueTreeState::ParameterLayout PcmSynthProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using I = juce::AudioParameterInt;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [](const juce::String& s) { return juce::ParameterID { s, 1 }; };

    l.add(std::make_unique<C>(id("mode"), "Mode", juce::StringArray { "Program", "Combination", "Multi" }, 0));
    {
        juce::StringArray combis;
        for (auto& c : pcm::combinations()) combis.add(c.name);
        l.add(std::make_unique<C>(id("combi"), "Combination", combis, 0));
    }
    juce::StringArray progNames;
    for (auto& pr : pcm::programs()) progNames.add(pr.name);
    for (int i = 1; i <= 8; ++i)
    {
        const juce::String s(i);
        l.add(std::make_unique<C>(id("p" + s + "prog"), "Part " + s + " Program", progNames, 0));
        l.add(std::make_unique<F>(id("p" + s + "lvl"), "Part " + s + " Level", R(0.0f, 1.0f, 0.01f), i == 1 ? 0.8f : 0.0f));
        l.add(std::make_unique<F>(id("p" + s + "pan"), "Part " + s + " Pan", R(-1.0f, 1.0f, 0.01f), 0.0f));
        l.add(std::make_unique<I>(id("p" + s + "lo"), "Part " + s + " Low Key", 0, 127, 0, juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return noteName(v); })));
        l.add(std::make_unique<I>(id("p" + s + "hi"), "Part " + s + " High Key", 0, 127, 127, juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return noteName(v); })));
        l.add(std::make_unique<I>(id("p" + s + "tr"), "Part " + s + " Transpose", -24, 24, 0));
        l.add(std::make_unique<I>(id("p" + s + "ch"), "Part " + s + " MIDI Channel", 1, 16, i));
    }

    // tone macros
    l.add(std::make_unique<F>(id("cutoff"), "Brightness", R(-1.0f, 1.0f, 0.01f), 0.0f));
    l.add(std::make_unique<F>(id("res"), "Resonance", R(0.0f, 0.6f, 0.01f), 0.0f));
    l.add(std::make_unique<F>(id("attack"), "Attack Time", R(0.1f, 6.0f, 0.0f, 0.5f), 1.0f));
    l.add(std::make_unique<F>(id("release"), "Release Time", R(0.1f, 6.0f, 0.0f, 0.5f), 1.0f));
    l.add(std::make_unique<F>(id("lfo"), "LFO Depth", R(0.0f, 3.0f, 0.01f), 1.0f));
    l.add(std::make_unique<I>(id("bend"), "Bend Range", 1, 12, 2));

    // effects
    juce::StringArray fxNames;
    for (int i = 0; i < pcm::FxCount; ++i) fxNames.add(pcm::fxName(i));
    for (int i = 1; i <= 2; ++i)
    {
        const juce::String s(i);
        l.add(std::make_unique<C>(id("fx" + s + "type"), "FX" + s + " Type", fxNames, i == 1 ? 1 : 3));
        l.add(std::make_unique<F>(id("fx" + s + "a"), "FX" + s + " A", R(0.0f, 1.0f, 0.01f), 0.5f));
        l.add(std::make_unique<F>(id("fx" + s + "b"), "FX" + s + " B", R(0.0f, 1.0f, 0.01f), 0.5f));
        l.add(std::make_unique<F>(id("fx" + s + "mix"), "FX" + s + " Mix", R(0.0f, 1.0f, 0.01f), i == 1 ? 0.18f : 0.0f));
    }

    l.add(std::make_unique<F>(id("master"), "Master", R(-24.0f, 6.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<F>(id("joyx"), "Joystick X (Bend)", R(-1.0f, 1.0f, 0.001f), 0.0f));
    l.add(std::make_unique<F>(id("joyy"), "Joystick Y (Mod)", R(0.0f, 1.0f, 0.001f), 0.0f));
    return l;
}

PcmSynthProcessor::PcmSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createLayout())
{
    apvts.addParameterListener("combi", this);
    formats.registerBasicFormats();
    for (auto& p : padHit) p = 0.0f;
    published = std::make_shared<const pcm::SampleBank>(slots);
}

PcmSynthProcessor::~PcmSynthProcessor() { apvts.removeParameterListener("combi", this); cancelPendingUpdate(); }

bool PcmSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PcmSynthProcessor::parameterChanged(const juce::String& id, float v)
{
    if (id == "combi" && !restoring.load())
    {
        pendingCombi = juce::roundToInt(v);
        triggerAsyncUpdate();
    }
}

void PcmSynthProcessor::handleAsyncUpdate()
{
    const int c = pendingCombi.exchange(-1);
    if (c >= 0) loadCombination(c);
}

void PcmSynthProcessor::loadCombination(int index)
{
    const auto& cb = pcm::combinations()[(size_t) juce::jlimit(0, pcm::kNumCombis - 1, index)];
    auto setP = [this](const juce::String& id, float real) {
        if (auto* prm = apvts.getParameter(id)) prm->setValueNotifyingHost(prm->convertTo0to1(real)); };
    for (int i = 0; i < 8; ++i)
    {
        const juce::String s(i + 1); const auto& pc = cb.part[i];
        setP("p" + s + "prog", (float) pc.prog); setP("p" + s + "lvl", pc.level); setP("p" + s + "pan", pc.pan);
        setP("p" + s + "lo", (float) pc.lo); setP("p" + s + "hi", (float) pc.hi);
        setP("p" + s + "tr", (float) pc.transpose); setP("p" + s + "ch", (float) pc.channel);
    }
}

void PcmSynthProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    applyParameters();
    keyboardState.reset();
}

void PcmSynthProcessor::applyParameters()
{
    auto& g = engine.g;
    g.mode = juce::roundToInt(p("mode"));
    g.masterDb = p("master");
    g.cutoffOffset = p("cutoff"); g.resOffset = p("res");
    g.attackScale = p("attack"); g.releaseScale = p("release"); g.lfoScale = p("lfo");
    g.bendRange = (float) juce::roundToInt(p("bend"));
    engine.joyX = p("joyx"); engine.joyY = p("joyy");
    for (int i = 0; i < 8; ++i)
    {
        const juce::String s(i + 1); auto& pc = engine.parts[(size_t) i];
        pc.prog = juce::roundToInt(p("p" + s + "prog")); pc.level = p("p" + s + "lvl"); pc.pan = p("p" + s + "pan");
        pc.lo = juce::roundToInt(p("p" + s + "lo")); pc.hi = juce::roundToInt(p("p" + s + "hi"));
        pc.transpose = juce::roundToInt(p("p" + s + "tr")); pc.channel = juce::roundToInt(p("p" + s + "ch"));
    }
    for (int i = 0; i < 2; ++i)
    {
        const juce::String s(i + 1);
        engine.fx[i].set(juce::roundToInt(p("fx" + s + "type")), p("fx" + s + "a"), p("fx" + s + "b"), p("fx" + s + "mix"));
    }
}

void PcmSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || n == 0) return;

    keyboardState.processNextMidiBuffer(midi, 0, n, true);   // on-screen keyboard -> same stream
    applyParameters();
    {
        auto pb = std::atomic_load(&published);
        if (pb != audioBank) { audioBank = pb; engine.setBank(pb); }
    }

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);
    int pos = 0;
    auto handle = [this](const juce::MidiMessage& m) {
        const int ch = m.getChannel() - 1;
        if (m.isNoteOn())
        {
            engine.noteOn(ch, m.getNoteNumber(), m.getFloatVelocity());
            const int pad = m.getNoteNumber() - kPadBase;
            if (pad >= 0 && pad < 16) padHit[(size_t) pad] = m.getFloatVelocity();
        }
        else if (m.isNoteOff()) engine.noteOff(ch, m.getNoteNumber());
        else if (m.isPitchWheel()) engine.pitchBend(ch, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (cc == 1) engine.modWheel(ch, (float) v / 127.0f);
            else if (cc == 64) engine.pedalChange(ch, v >= 64);
            else if (cc == 120 || cc == 123) engine.allNotesOff();
        } };
    for (const auto meta : midi)
    {
        const int at = juce::jlimit(pos, n, meta.samplePosition);
        if (at > pos) { engine.process(l + pos, r + pos, at - pos); pos = at; }
        handle(meta.getMessage());
    }
    if (pos < n) engine.process(l + pos, r + pos, n - pos);

    outPeak = juce::jmax(buffer.getMagnitude(0, 0, n), buffer.getMagnitude(1, 0, n));
    voicesActive = engine.activeVoices();
}

juce::AudioProcessorEditor* PcmSynthProcessor::createEditor() { return new PcmSynthEditor(*this); }

void PcmSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild(state.getChildWithName("SAMPLES"), nullptr);
    state.appendChild(samplesToTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void PcmSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            restoring = true;                      // do not let the restored "combi" overwrite restored parts
            auto tree = juce::ValueTree::fromXml(*xml);
            auto smp = tree.getChildWithName("SAMPLES");
            if (smp.isValid()) tree.removeChild(smp, nullptr);
            apvts.replaceState(tree);
            restoring = false;
            if (smp.isValid()) samplesFromTree(smp);
        }
}

//==============================================================================
// user samples
pcm::SampleSlot PcmSynthProcessor::getSlot(int i) const
{
    const juce::ScopedLock sl(slotLock);
    return slots[(size_t) juce::jlimit(0, pcm::kUserSlots - 1, i)];
}

void PcmSynthProcessor::publishBank()
{
    std::atomic_store(&published, std::shared_ptr<const pcm::SampleBank>(std::make_shared<const pcm::SampleBank>(slots)));
    ++slotsVersion;
}

void PcmSynthProcessor::setSlot(int i, const pcm::SampleSlot& s)
{
    { const juce::ScopedLock sl(slotLock); slots[(size_t) juce::jlimit(0, pcm::kUserSlots - 1, i)] = s; publishBank(); }
}

bool PcmSynthProcessor::loadSampleFile(int i, const juce::File& f, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> rd(formats.createReaderFor(f));
    if (!rd || rd->lengthInSamples < 2 || rd->sampleRate <= 0) { error = "Could not read " + f.getFileName(); return false; }
    const juce::int64 cap = (juce::int64) (20.0 * rd->sampleRate);
    const int frames = (int) std::min<juce::int64>(rd->lengthInSamples, cap);
    const int chans = (int) std::min<unsigned>(rd->numChannels, 2u);
    juce::AudioBuffer<float> buf(chans, frames);
    rd->read(&buf, 0, frames, 0, true, chans > 1);
    std::vector<float> l(buf.getReadPointer(0), buf.getReadPointer(0) + frames), r;
    if (chans > 1) r.assign(buf.getReadPointer(1), buf.getReadPointer(1) + frames);
    {
        const juce::ScopedLock sl(slotLock);
        auto& s = slots[(size_t) juce::jlimit(0, pcm::kUserSlots - 1, i)];
        s.audio = pcm::makeAudio(std::move(l), std::move(r), rd->sampleRate);
        s.name = f.getFileNameWithoutExtension().toStdString();
        publishBank();
    }
    if (rd->lengthInSamples > cap) error = "Sample was longer than 20 s and was shortened.";
    return true;
}

void PcmSynthProcessor::clearSlot(int i)
{
    const juce::ScopedLock sl(slotLock);
    slots[(size_t) juce::jlimit(0, pcm::kUserSlots - 1, i)] = pcm::SampleSlot();
    publishBank();
}

void PcmSynthProcessor::doAutoMap() { const juce::ScopedLock sl(slotLock); pcm::autoMapKeys(slots); publishBank(); }
void PcmSynthProcessor::doDrumMap() { const juce::ScopedLock sl(slotLock); pcm::drumMap(slots, kPadBase); publishBank(); }

bool PcmSynthProcessor::doDetectRoot(int i)
{
    const juce::ScopedLock sl(slotLock);
    auto& s = slots[(size_t) juce::jlimit(0, pcm::kUserSlots - 1, i)];
    int note; float cents;
    if (!s.has() || !pcm::detectPitch(*s.audio, note, cents)) return false;
    s.root = note; s.tune = -cents;   // tune so the sample sounds exactly at the detected key
    publishBank();
    return true;
}

void PcmSynthProcessor::useSamplesInProgramMode()
{
    auto setP = [this](const juce::String& id, float real) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(real)); };
    setP("mode", 0.0f); setP("p1prog", (float) pcm::kUserProgram); setP("p1lvl", 0.9f); setP("p1lo", 0.0f); setP("p1hi", 127.0f); setP("p1tr", 0.0f);
}

juce::ValueTree PcmSynthProcessor::samplesToTree() const
{
    juce::ValueTree t("SAMPLES");
    const juce::ScopedLock sl(slotLock);
    for (int i = 0; i < pcm::kUserSlots; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (!s.has()) continue;
        juce::ValueTree v("SLOT");
        v.setProperty("index", i, nullptr); v.setProperty("name", juce::String(s.name), nullptr);
        v.setProperty("root", s.root, nullptr); v.setProperty("lo", s.lo, nullptr); v.setProperty("hi", s.hi, nullptr);
        v.setProperty("velLo", s.velLo, nullptr); v.setProperty("velHi", s.velHi, nullptr);
        v.setProperty("tune", s.tune, nullptr); v.setProperty("level", s.level, nullptr); v.setProperty("pan", s.pan, nullptr);
        v.setProperty("mode", s.mode, nullptr); v.setProperty("loopStart", s.loopStart, nullptr); v.setProperty("loopEnd", s.loopEnd, nullptr);
        v.setProperty("cutoff", s.cutoff, nullptr); v.setProperty("attack", s.attack, nullptr); v.setProperty("release", s.release, nullptr);
        const auto& a = *s.audio; const int ch = a.stereo() ? 2 : 1; const size_t n = a.frames();
        v.setProperty("rate", a.rate, nullptr); v.setProperty("channels", ch, nullptr); v.setProperty("frames", (juce::int64) n, nullptr);
        std::vector<juce::int16> pcm16(n * (size_t) ch);
        for (size_t k = 0; k < n; ++k)
        {
            pcm16[k * (size_t) ch] = (juce::int16) juce::roundToInt(juce::jlimit(-1.0f, 1.0f, a.l[k]) * 32767.0f);
            if (ch == 2) pcm16[k * 2 + 1] = (juce::int16) juce::roundToInt(juce::jlimit(-1.0f, 1.0f, a.r[k]) * 32767.0f);
        }
        v.setProperty("data", juce::Base64::toBase64(pcm16.data(), pcm16.size() * sizeof(juce::int16)), nullptr);
        t.appendChild(v, nullptr);
    }
    return t;
}

void PcmSynthProcessor::samplesFromTree(const juce::ValueTree& t)
{
    const juce::ScopedLock sl(slotLock);
    for (auto& s : slots) s = pcm::SampleSlot();
    for (int k = 0; k < t.getNumChildren(); ++k)
    {
        const auto v = t.getChild(k);
        const int i = (int) v.getProperty("index", -1);
        if (i < 0 || i >= pcm::kUserSlots) continue;
        const int ch = juce::jlimit(1, 2, (int) v.getProperty("channels", 1)); const size_t n = (size_t) (juce::int64) v.getProperty("frames", 0);
        juce::MemoryOutputStream out;
        if (n < 2 || !juce::Base64::convertFromBase64(out, v.getProperty("data").toString()) || out.getDataSize() < n * (size_t) ch * 2) continue;
        const auto* p = static_cast<const juce::int16*>(out.getData());
        std::vector<float> l(n), r(ch == 2 ? n : 0);
        for (size_t j = 0; j < n; ++j) { l[j] = (float) p[j * (size_t) ch] / 32767.0f; if (ch == 2) r[j] = (float) p[j * 2 + 1] / 32767.0f; }
        auto& s = slots[(size_t) i];
        s.audio = pcm::makeAudio(std::move(l), std::move(r), (double) v.getProperty("rate", 44100.0));
        s.name = v.getProperty("name").toString().toStdString();
        s.root = v.getProperty("root", 60); s.lo = v.getProperty("lo", 0); s.hi = v.getProperty("hi", 127);
        s.velLo = v.getProperty("velLo", 1); s.velHi = v.getProperty("velHi", 127);
        s.tune = (float) (double) v.getProperty("tune", 0.0); s.level = (float) (double) v.getProperty("level", 1.0); s.pan = (float) (double) v.getProperty("pan", 0.0);
        s.mode = v.getProperty("mode", 0); s.loopStart = (float) (double) v.getProperty("loopStart", 0.4); s.loopEnd = (float) (double) v.getProperty("loopEnd", 0.9);
        s.cutoff = (float) (double) v.getProperty("cutoff", 20000.0); s.attack = (float) (double) v.getProperty("attack", 0.002); s.release = (float) (double) v.getProperty("release", 0.15);
    }
    publishBank();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PcmSynthProcessor(); }
