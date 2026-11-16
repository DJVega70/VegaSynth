#include "PluginEditor.h"
#include <map>

namespace
{
    const juce::Colour panelTop { 0xff2b2d33 }, panelBottom { 0xff17181c }, ink { 0xffe9e4d8 }, dim { 0xff9a968c };
    const juce::Colour amber { 0xffffb23e }, lcdBg { 0xff120d05 }, teal { 0xff3fa7a0 }, red { 0xffc8412f },
                       blue { 0xff3f78b8 }, green { 0xff4f9a64 }, slate { 0xff6a707a }, cream { 0xffcfc7b0 };
    constexpr float kPi = juce::MathConstants<float>::pi;

    void screw(juce::Graphics& g, float x, float y)
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(x - 5, y - 4, 10, 10);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8b8e94), x - 4, y - 5, juce::Colour(0xff3c3e43), x + 4, y + 5, false));
        g.fillEllipse(x - 5, y - 5, 10, 10);
        g.setColour(juce::Colour(0xff1b1c1f)); g.drawLine(x - 3, y + 1, x + 3, y - 1, 1.4f);
    }
}

//==============================================================================
class SynthLook : public juce::LookAndFeel_V4
{
public:
    SynthLook()
    {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d0e10));
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.15f));
        setColour(juce::ComboBox::arrowColourId, amber);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1b1c20));
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, amber.darker(0.4f));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff0d0e10));
        setColour(juce::Slider::trackColourId, amber.darker(0.5f));
        setColour(juce::Slider::textBoxTextColourId, ink);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::FontOptions(12.0f); }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float, float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h);
        const float d = juce::jmin(b.getWidth(), b.getHeight()) - 6.0f;
        const auto c = b.getCentre();
        const float ang = startAngle + (float) s.valueToProportionOfLength(s.getValue()) * (endAngle - startAngle);
        const juce::Colour cap((juce::uint32) (int) s.getProperties()["cap"]);
        g.setColour(dim.withAlpha(0.8f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle), r0 = d * 0.5f + 2.5f, r1 = r0 + (i % 5 == 0 ? 4.5f : 2.5f);
            g.drawLine(c.x + std::sin(a) * r0, c.y - std::cos(a) * r0, c.x + std::sin(a) * r1, c.y - std::cos(a) * r1, 1.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillEllipse(c.x - d * 0.5f + 1, c.y - d * 0.5f + 3, d, d);
        g.setColour(juce::Colour(0xff0c0d0f)); g.fillEllipse(c.x - d * 0.5f, c.y - d * 0.5f, d, d);
        const float cd = d - 6.0f;
        g.setGradientFill(juce::ColourGradient(cap.brighter(0.25f), c.x - cd * 0.3f, c.y - cd * 0.5f, cap.darker(0.55f), c.x + cd * 0.3f, c.y + cd * 0.5f, false));
        g.fillEllipse(c.x - cd * 0.5f, c.y - cd * 0.5f, cd, cd);
        g.setColour(juce::Colours::white.withAlpha(0.18f)); g.drawEllipse(c.x - cd * 0.5f + 0.5f, c.y - cd * 0.5f + 0.5f, cd - 1, cd - 1, 1.0f);
        const float td = cd * 0.62f;
        g.setGradientFill(juce::ColourGradient(cap.brighter(0.05f), c.x, c.y - td * 0.5f, cap.darker(0.3f), c.x, c.y + td * 0.5f, false));
        g.fillEllipse(c.x - td * 0.5f, c.y - td * 0.5f, td, td);
        juce::Path p; p.addRoundedRectangle(-1.4f, -cd * 0.5f + 1.0f, 2.8f, cd * 0.42f, 1.2f);
        g.setColour(juce::Colours::white.withAlpha(0.95f)); g.fillPath(p, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }
};

class Knob : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& t, juce::Colour cap,
         std::function<juce::String(double)> f, SynthLook& laf) : title(t.toUpperCase()), fmt(std::move(f))
    {
        slider.setLookAndFeel(&laf); slider.getProperties().set("cap", (int) cap.getARGB());
        slider.setRotaryParameters(kPi * 1.2f, kPi * 2.8f, true); slider.setMouseDragSensitivity(170);
        addAndMakeVisible(slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(a, id, slider);
        if (auto* p = a.getParameter(id)) slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
        value.setJustificationType(juce::Justification::centred);
        value.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::plain));
        value.setColour(juce::Label::textColourId, ink.withAlpha(0.85f)); value.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(value);
        slider.onValueChange = [this] { value.setText(fmt(slider.getValue()), juce::dontSendNotification); };
        slider.onValueChange();
    }
    void setTitle(const juce::String& t) { title = t.toUpperCase(); repaint(); }
    void paint(juce::Graphics& g) override { g.setColour(dim); g.setFont(juce::FontOptions(10.0f, juce::Font::bold)); g.drawText(title, 0, 0, getWidth(), 13, juce::Justification::centred); }
    void resized() override
    {
        const int d = juce::jmin(getWidth() - 4, getHeight() - 30);
        slider.setBounds(juce::Rectangle<int>(d, d).withCentre({ getWidth() / 2, 14 + d / 2 }));
        value.setBounds(0, getHeight() - 14, getWidth(), 14);
    }
private:
    juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::String title; juce::Label value; std::function<juce::String(double)> fmt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ModeButton : public juce::Button
{
public:
    ModeButton(const juce::String& t) : juce::Button(t) {}
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f); const bool on = getToggleState();
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(b.translated(0, 2), 4.0f);
        auto face = down ? b.translated(0, 1) : b;
        g.setColour(juce::Colour(0xff0b0b0d)); g.fillRoundedRectangle(face, 4.0f);
        auto cap = face.reduced(2.5f);
        juce::Colour t(0xff6c7077), bt(0xff383a3f); if (over) { t = t.brighter(0.1f); bt = bt.brighter(0.1f); }
        g.setGradientFill(juce::ColourGradient(t, 0, cap.getY(), bt, 0, cap.getBottom(), false)); g.fillRoundedRectangle(cap, 3.0f);
        auto lamp = juce::Rectangle<float>(cap.getWidth() - 16.0f, 5.0f).withCentre({ cap.getCentreX(), cap.getY() + 7.0f });
        if (on) { g.setColour(amber.withAlpha(0.28f)); g.fillRoundedRectangle(lamp.expanded(4, 3), 5.0f); }
        g.setColour(on ? amber : amber.withAlpha(0.18f)); g.fillRoundedRectangle(lamp, 2.0f);
        g.setColour(ink); g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(getButtonText(), cap.withTrimmedTop(12.0f), juce::Justification::centred);
    }
};

class JoystickPad : public juce::Component, private juce::Timer
{
public:
    JoystickPad(juce::AudioProcessorValueTreeState& a) : px(*a.getParameter("joyx")), py(*a.getParameter("joyy")) { startTimerHz(30); }
    ~JoystickPad() override { stopTimer(); }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(b.translated(0, 2), 10.0f);
        g.setColour(juce::Colour(0xff0b0c0e)); g.fillRoundedRectangle(b, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.12f)); g.drawRoundedRectangle(b, 10.0f, 1.0f);
        g.setColour(dim.withAlpha(0.5f)); g.drawLine(b.getCentreX(), b.getY() + 8, b.getCentreX(), b.getBottom() - 8); g.drawLine(b.getX() + 8, b.getBottom() - 12, b.getRight() - 8, b.getBottom() - 12);
        g.setColour(dim); g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText("BEND", getLocalBounds().removeFromBottom(14), juce::Justification::centred); g.drawText("MOD", getLocalBounds().withTrimmedTop(4).removeFromTop(12), juce::Justification::centred);
        const float cx = 2.0f + (px.getValue() * 0.5f * 0 + (float) (px.convertFrom0to1(px.getValue()) * 0.5 + 0.5)) * (getWidth() - 4.0f);
        const float cy = 2.0f + (1.0f - (float) py.getValue()) * (getHeight() - 16.0f);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(cx - 12 + 1, cy - 12 + 3, 24, 24);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff9b988d), cx - 6, cy - 8, juce::Colour(0xff3a3b3f), cx + 6, cy + 8, false)); g.fillEllipse(cx - 12, cy - 12, 24, 24);
        g.setColour(amber); g.fillEllipse(cx - 4, cy - 4, 8, 8);
    }
    void mouseDown(const juce::MouseEvent& e) override { px.beginChangeGesture(); py.beginChangeGesture(); set(e); }
    void mouseDrag(const juce::MouseEvent& e) override { set(e); }
    void mouseUp(const juce::MouseEvent&) override
    {
        px.setValueNotifyingHost(px.convertTo0to1(0.0f)); py.setValueNotifyingHost(0.0f);   // springs back
        px.endChangeGesture(); py.endChangeGesture();
    }
private:
    void set(const juce::MouseEvent& e)
    {
        const float x = juce::jlimit(0.0f, 1.0f, (float) e.x / (float) getWidth()), y = juce::jlimit(0.0f, 1.0f, 1.0f - (float) e.y / (float) (getHeight() - 12));
        const float sx = std::fabs(x - 0.5f) < 0.04f ? 0.5f : x;   // small dead zone at the centre
        px.setValueNotifyingHost(sx); py.setValueNotifyingHost(y);
    }
    void timerCallback() override { repaint(); }
    juce::RangedAudioParameter &px, &py;
};

//==============================================================================
static juce::String noteName(int n)
{
    static const char* a[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    n = juce::jlimit(0, 127, n); return juce::String(a[n % 12]) + juce::String(n / 12 - 2);
}

class SamplesPage : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    SamplesPage(PcmSynthProcessor& p, SynthLook& laf) : proc(p)
    {
        static const char* bn[6] = { "LOAD WAV...", "CLEAR", "DETECT PITCH", "AUTO-MAP KEYS", "DRUM MAP", "USE IN PROGRAM" };
        for (int i = 0; i < 6; ++i) { btn[i].setButtonText(bn[i]); addAndMakeVisible(btn[i]); }
        btn[0].onClick = [this] { chooseFile(); };
        btn[1].onClick = [this] { proc.clearSlot(sel); refresh(); };
        btn[2].onClick = [this] { status = proc.doDetectRoot(sel) ? "Pitch detected and root set." : "No clear pitch found (try setting Root by hand)."; refresh(); };
        btn[3].onClick = [this] { proc.doAutoMap(); status = "Slots spread across the keyboard by root note."; refresh(); };
        btn[4].onClick = [this] { proc.doDrumMap(); status = "One-shot drum map: slot 1 = C1 (36), each slot one key up."; refresh(); };
        btn[5].onClick = [this] { proc.useSamplesInProgramMode(); status = "Program mode, part 1 = User Samples."; };

        mode.addItem("Gate (hold)", 1); mode.addItem("Loop", 2); mode.addItem("One-Shot", 3); mode.setLookAndFeel(&laf);
        mode.onChange = [this] { if (!updating) edit([&](pcm::SampleSlot& s) { s.mode = mode.getSelectedId() - 1; }); };
        addAndMakeVisible(mode); modeLabel("Mode");

        auto nn = [](double v) { return noteName((int) v); };
        auto pct = [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; };
        add("Root", 0, 127, 1, nn, [](const pcm::SampleSlot& s) { return (double) s.root; }, [](pcm::SampleSlot& s, double v) { s.root = (int) v; }, laf);
        add("Fine tune", -100, 100, 1, [](double v) { return juce::String((int) v) + " ct"; }, [](const pcm::SampleSlot& s) { return (double) s.tune; }, [](pcm::SampleSlot& s, double v) { s.tune = (float) v; }, laf);
        add("Key low", 0, 127, 1, nn, [](const pcm::SampleSlot& s) { return (double) s.lo; }, [](pcm::SampleSlot& s, double v) { s.lo = (int) v; }, laf);
        add("Key high", 0, 127, 1, nn, [](const pcm::SampleSlot& s) { return (double) s.hi; }, [](pcm::SampleSlot& s, double v) { s.hi = (int) v; }, laf);
        add("Vel low", 1, 127, 1, [](double v) { return juce::String((int) v); }, [](const pcm::SampleSlot& s) { return (double) s.velLo; }, [](pcm::SampleSlot& s, double v) { s.velLo = (int) v; }, laf);
        add("Vel high", 1, 127, 1, [](double v) { return juce::String((int) v); }, [](const pcm::SampleSlot& s) { return (double) s.velHi; }, [](pcm::SampleSlot& s, double v) { s.velHi = (int) v; }, laf);
        add("Level", 0, 1.5, 0.01, pct, [](const pcm::SampleSlot& s) { return (double) s.level; }, [](pcm::SampleSlot& s, double v) { s.level = (float) v; }, laf);
        add("Pan", -1, 1, 0.01, [](double v) { const int q = juce::roundToInt(v * 100); return q == 0 ? juce::String("C") : (q < 0 ? "L" : "R") + juce::String(std::abs(q)); }, [](const pcm::SampleSlot& s) { return (double) s.pan; }, [](pcm::SampleSlot& s, double v) { s.pan = (float) v; }, laf);
        add("Loop start", 0, 1, 0.001, pct, [](const pcm::SampleSlot& s) { return (double) s.loopStart; }, [](pcm::SampleSlot& s, double v) { s.loopStart = (float) std::min(v, (double) s.loopEnd - 0.01); }, laf);
        add("Loop end", 0, 1, 0.001, pct, [](const pcm::SampleSlot& s) { return (double) s.loopEnd; }, [](pcm::SampleSlot& s, double v) { s.loopEnd = (float) std::max(v, (double) s.loopStart + 0.01); }, laf);
        add("Cutoff", 0, 1, 0.001, [](double v) { return juce::String(juce::roundToInt(20.0 * std::pow(1000.0, v))) + " Hz"; },
            [](const pcm::SampleSlot& s) { return std::log(std::max(20.0, (double) s.cutoff) / 20.0) / std::log(1000.0); }, [](pcm::SampleSlot& s, double v) { s.cutoff = (float) (20.0 * std::pow(1000.0, v)); }, laf);
        add("Attack", 0, 1, 0.001, [](double v) { return juce::String(v * v * 2000.0, 0) + " ms"; },
            [](const pcm::SampleSlot& s) { return std::sqrt(std::max(0.0, (double) s.attack / 2.0)); }, [](pcm::SampleSlot& s, double v) { s.attack = (float) (v * v * 2.0); }, laf);
        add("Release", 0, 1, 0.001, [](double v) { return juce::String(0.005 + v * v * 5.0, 2) + " s"; },
            [](const pcm::SampleSlot& s) { return std::sqrt(std::max(0.0, ((double) s.release - 0.005) / 5.0)); }, [](pcm::SampleSlot& s, double v) { s.release = (float) (0.005 + v * v * 5.0); }, laf);
        startTimerHz(10);
        refresh();
    }
    ~SamplesPage() override { stopTimer(); mode.setLookAndFeel(nullptr); for (auto& c : ctl) c->s.setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        auto head = [&](const juce::String& t, int x, int y, int w, juce::Colour c) {
            g.setColour(c); g.fillRoundedRectangle((float) x, (float) y + 2, 4.0f, 12.0f, 1.5f);
            g.setColour(ink.withAlpha(0.9f)); g.setFont(juce::FontOptions(11.0f, juce::Font::bold)); g.drawText(t, x + 10, y, w, 16, juce::Justification::centredLeft);
            g.setColour(juce::Colours::white.withAlpha(0.10f)); g.drawHorizontalLine(y + 19, (float) x, (float) (x + w)); };
        head("USER SAMPLES", 4, 0, 244, amber);
        head("WAVEFORM", 266, 0, 674, teal);
        for (int i = 0; i < pcm::kUserSlots; ++i)
        {
            auto r = slotRect(i); const auto s = proc.getSlot(i);
            if (i == sel) { g.setColour(amber.withAlpha(0.18f)); g.fillRoundedRectangle(r.toFloat(), 3.0f); }
            g.setColour(dim); g.setFont(juce::FontOptions(10.0f, juce::Font::bold)); g.drawText(juce::String(i + 1), r.removeFromLeft(20), juce::Justification::centred);
            g.setColour(s.has() ? ink : dim.withAlpha(0.5f)); g.setFont(juce::FontOptions(11.5f));
            const auto info = s.has() ? juce::String(s.mode == 0 ? "G " : s.mode == 1 ? "L " : "1 ") + noteName(s.lo) + "-" + noteName(s.hi) : juce::String();
            g.drawText(s.has() ? juce::String(s.name) : "(empty)", r.removeFromLeft(130), juce::Justification::centredLeft, true);
            g.setColour(dim); g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::plain)); g.drawText(info, r, juce::Justification::centredRight);
        }
        // waveform
        auto w = waveRect();
        g.setColour(lcdBg); g.fillRoundedRectangle(w.toFloat(), 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.12f)); g.drawRoundedRectangle(w.toFloat(), 4.0f, 1.0f);
        const auto s = proc.getSlot(sel);
        if (s.has())
        {
            updatePeaks(s);
            const float mid = (float) w.getCentreY(), hh = (float) w.getHeight() * 0.46f;
            g.setColour(amber.withAlpha(0.85f));
            for (size_t x = 0; x < peaks.size(); ++x)
                g.drawVerticalLine(w.getX() + (int) x, mid - peaks[x].second * hh, mid - peaks[x].first * hh);
            if (s.mode == pcm::SmpLoop)
            {
                const float a = w.getX() + s.loopStart * w.getWidth(), b = w.getX() + s.loopEnd * w.getWidth();
                g.setColour(teal.withAlpha(0.16f)); g.fillRect(a, (float) w.getY(), b - a, (float) w.getHeight());
                g.setColour(teal); g.drawVerticalLine((int) a, (float) w.getY(), (float) w.getBottom()); g.drawVerticalLine((int) b, (float) w.getY(), (float) w.getBottom());
                g.setFont(juce::FontOptions(9.0f, juce::Font::bold)); g.drawText("LOOP", (int) a + 3, w.getY() + 2, 40, 12, juce::Justification::left);
            }
            g.setColour(dim); g.setFont(juce::FontOptions(9.5f));
            g.drawText(juce::String(s.audio->frames() / s.audio->rate, 2) + " s  " + juce::String(juce::roundToInt(s.audio->rate)) + " Hz  " + (s.audio->stereo() ? "stereo" : "mono"),
                       w.getX() + 6, w.getBottom() - 14, 200, 12, juce::Justification::left);
        }
        else { g.setColour(dim); g.setFont(juce::FontOptions(12.0f)); g.drawText("Empty slot - click LOAD WAV or drop WAV / AIFF / FLAC files here", w, juce::Justification::centred); }
        g.setColour(dim); g.setFont(juce::FontOptions(10.5f)); g.drawText(status, 266, 374, 674, 16, juce::Justification::centredLeft);
    }

    void resized() override
    {
        for (int i = 0; i < 6; ++i) btn[i].setBounds(266 + i * 113, 150, 108, 26);
        int k = 0;
        for (auto& c : ctl) place(c->label, c->s, k++);
        place(modeLbl, mode, k);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < pcm::kUserSlots; ++i) if (slotRect(i).contains(e.getPosition())) { sel = i; refresh(); return; }
        dragMarker = -1;
        const auto w = waveRect(); const auto s = proc.getSlot(sel);
        if (s.has() && s.mode == pcm::SmpLoop && w.expanded(6, 0).contains(e.getPosition()))
        {
            const float a = w.getX() + s.loopStart * w.getWidth(), b = w.getX() + s.loopEnd * w.getWidth();
            dragMarker = std::abs(e.x - a) <= std::abs(e.x - b) ? 0 : 1; mouseDrag(e);
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragMarker < 0) return;
        const auto w = waveRect(); const float f = juce::jlimit(0.0f, 1.0f, (float) (e.x - w.getX()) / (float) w.getWidth());
        edit([&](pcm::SampleSlot& s) { if (dragMarker == 0) s.loopStart = std::min(f, s.loopEnd - 0.01f); else s.loopEnd = std::max(f, s.loopStart + 0.01f); });
        refresh();
    }
    void mouseUp(const juce::MouseEvent&) override { dragMarker = -1; }

    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int, int y) override
    {
        int slot = sel; if (y >= 22 && y < 22 + 16 * 22) slot = (y - 22) / 22;
        loadFiles(files, slot);
    }
    void loadFiles(const juce::StringArray& files, int slot)
    {
        juce::String msg; int n = 0;
        for (auto& f : files)
        {
            if (slot >= pcm::kUserSlots) { msg = "Only 16 slots - extra files skipped."; break; }
            juce::String err;
            if (proc.loadSampleFile(slot, juce::File(f), err)) { sel = slot; ++slot; ++n; if (err.isNotEmpty()) msg = err; }
            else msg = err;
        }
        status = juce::String(n) + " file(s) loaded. " + msg; refresh();
    }

private:
    struct Ctl
    {
        juce::Label label; juce::Slider s; std::function<double(const pcm::SampleSlot&)> get; std::function<void(pcm::SampleSlot&, double)> set;
    };
    void add(const char* name, double lo, double hi, double st, std::function<juce::String(double)> fmt,
             std::function<double(const pcm::SampleSlot&)> get, std::function<void(pcm::SampleSlot&, double)> set, SynthLook& laf)
    {
        auto c = std::make_unique<Ctl>();
        c->label.setText(name, juce::dontSendNotification); c->label.setColour(juce::Label::textColourId, dim); c->label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        c->s.setSliderStyle(juce::Slider::LinearBar); c->s.setTextBoxIsEditable(false); c->s.setLookAndFeel(&laf); c->s.setRange(lo, hi, st);
        c->s.textFromValueFunction = fmt; c->get = std::move(get); c->set = std::move(set);
        auto* raw = c.get();
        c->s.onValueChange = [this, raw] { if (!updating) edit([&](pcm::SampleSlot& s) { raw->set(s, raw->s.getValue()); }); };
        addAndMakeVisible(c->label); addAndMakeVisible(c->s); ctl.push_back(std::move(c));
    }
    void modeLabel(const char* t) { modeLbl.setText(t, juce::dontSendNotification); modeLbl.setColour(juce::Label::textColourId, dim); modeLbl.setFont(juce::FontOptions(10.5f, juce::Font::bold)); addAndMakeVisible(modeLbl); }
    void place(juce::Component& l, juce::Component& c, int k)
    {
        const int col = k % 3, row = k / 3, x = 266 + col * 226, y = 188 + row * 36;
        l.setBounds(x, y, 70, 24); c.setBounds(x + 72, y, 142, 24);
    }
    template <typename F> void edit(F f)
    {
        auto s = proc.getSlot(sel); if (!s.has()) return;
        f(s); proc.setSlot(sel, s);
    }
    juce::Rectangle<int> slotRect(int i) const { return { 4, 22 + i * 22, 244, 21 }; }
    juce::Rectangle<int> waveRect() const { return { 266, 22, 674, 120 }; }
    void refresh()
    {
        const auto s = proc.getSlot(sel); updating = true;
        for (auto& c : ctl) { c->s.setEnabled(s.has()); if (s.has()) c->s.setValue(c->get(s), juce::dontSendNotification); c->s.updateText(); }
        mode.setEnabled(s.has()); if (s.has()) mode.setSelectedId(s.mode + 1, juce::dontSendNotification);
        updating = false; lastVer = proc.slotsVersion.load(); repaint();
    }
    void updatePeaks(const pcm::SampleSlot& s)
    {
        if (s.audio.get() == peaksFor && (int) peaks.size() == waveRect().getWidth()) return;
        peaksFor = s.audio.get(); const int W = waveRect().getWidth(); peaks.assign((size_t) W, { 0.0f, 0.0f });
        const size_t n = s.audio->frames();
        for (int x = 0; x < W; ++x)
        {
            const size_t a = n * (size_t) x / (size_t) W, b = std::max(a + 1, n * (size_t) (x + 1) / (size_t) W);
            float lo = 0, hi = 0;
            for (size_t k = a; k < b && k < n; ++k)
            {
                const float v = s.audio->stereo() ? 0.5f * (s.audio->l[k] + s.audio->r[k]) : s.audio->l[k];
                lo = std::min(lo, v); hi = std::max(hi, v);
            }
            peaks[(size_t) x] = { lo, hi };
        }
    }
    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser>("Load a sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                juce::StringArray a; for (auto& f : fc.getResults()) a.add(f.getFullPathName());
                if (a.size() > 0) loadFiles(a, sel);
            });
    }
    void timerCallback() override { if (proc.slotsVersion.load() != lastVer) refresh(); }

    PcmSynthProcessor& proc;
    juce::TextButton btn[6]; juce::ComboBox mode; juce::Label modeLbl;
    std::vector<std::unique_ptr<Ctl>> ctl;
    std::vector<std::pair<float, float>> peaks; const void* peaksFor = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status; int sel = 0, dragMarker = -1, lastVer = -1; bool updating = false;
};

//==============================================================================
class PadsPage : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    PadsPage(PcmSynthProcessor& p, SynthLook& laf, std::function<void(const juce::StringArray&, int)> dropFn) : proc(p), onDrop(std::move(dropFn))
    {
        setMouseClickGrabsKeyboardFocus(false);
        fixed.setButtonText("FIXED VELOCITY"); fixed.setClickingTogglesState(true); fixed.setToggleState(false, juce::dontSendNotification); addAndMakeVisible(fixed);
        fixedVel.setSliderStyle(juce::Slider::LinearBar); fixedVel.setRange(1, 127, 1); fixedVel.setValue(100); fixedVel.setLookAndFeel(&laf); fixedVel.setTextBoxIsEditable(false); addAndMakeVisible(fixedVel);
        setup.setButtonText("SET UP PADS (drum map + Program mode)"); addAndMakeVisible(setup);
        setup.onClick = [this] { proc.doDrumMap(); proc.useSamplesInProgramMode(); };
        for (auto& l : lastVel) l = 0.0f;
        startTimerHz(30);
    }
    ~PadsPage() override { stopTimer(); fixedVel.setLookAndFeel(nullptr); }

    void resized() override
    {
        setup.setBounds(4, 4, 300, 26); fixed.setBounds(320, 4, 150, 26); fixedVel.setBounds(476, 4, 160, 26);
    }
    juce::Rectangle<int> padRect(int i) const
    {
        const int col = i % 4, row = 3 - i / 4;                     // pad 1 bottom-left, like a drum machine
        const int area = std::min(getWidth() - 270, getHeight() - 44), cell = area / 4;
        return juce::Rectangle<int>(8 + col * cell, 40 + row * cell, cell - 8, cell - 8);
    }
    void paint(juce::Graphics& g) override
    {
        for (int i = 0; i < 16; ++i)
        {
            const auto r = padRect(i).toFloat(); const auto s = proc.getSlot(i);
            const bool on = proc.keyboardState.isNoteOn(1, kPadBase + i);
            g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(r.translated(0, 3), 9.0f);
            const auto base = s.has() ? juce::Colour(0xff3a3d44) : juce::Colour(0xff25272b);
            g.setGradientFill(juce::ColourGradient(base.brighter(0.2f), 0, r.getY(), base.darker(0.4f), 0, r.getBottom(), false)); g.fillRoundedRectangle(r, 9.0f);
            if (lastVel[i] > 0.01f) { g.setColour(amber.withAlpha(0.15f + 0.65f * lastVel[i])); g.fillRoundedRectangle(r, 9.0f); }
            g.setColour(on ? amber : juce::Colours::white.withAlpha(0.14f)); g.drawRoundedRectangle(r, 9.0f, on ? 2.5f : 1.0f);
            g.setColour(ink); g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
            g.drawText(s.has() ? juce::String(s.name) : "-", r.reduced(8, 6).withHeight(34), juce::Justification::topLeft, true);
            g.setColour(dim); g.setFont(juce::FontOptions(10.0f)); g.drawText(juce::String(i + 1) + "  " + noteName(kPadBase + i), r.reduced(8, 6), juce::Justification::bottomLeft);
            const float vh = (r.getHeight() - 16) * lastVel[i];                      // velocity bar
            g.setColour(amber); g.fillRoundedRectangle(r.getRight() - 12, r.getBottom() - 8 - vh, 5, std::max(0.0f, vh), 2.0f);
        }
        const int x0 = 8 + std::min(getWidth() - 270, getHeight() - 44) + 20;
        g.setColour(dim); g.setFont(juce::FontOptions(11.0f));
        g.drawFittedText("Hit a pad lower for soft, higher for hard (top = loudest). On a touch screen real finger pressure is used when the screen reports it, and several fingers can play at once.\n\n"
                         "Pads play keys C1-D#2 (36-51), so a MIDI controller or the keyboard below triggers them too. Drop a sample on a pad to load it. Each pad is one slot on the SAMPLES tab.",
                         juce::Rectangle<int>(x0, 44, getWidth() - x0 - 6, 220), juce::Justification::topLeft, 12);
    }
    void mouseDown(const juce::MouseEvent& e) override { press(e); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto it = held.find(e.source.getIndex()); const int pad = padAt(e.getPosition());
        if (it != held.end() && it->second != pad) { release(e.source.getIndex()); press(e); }
    }
    void mouseUp(const juce::MouseEvent& e) override { release(e.source.getIndex()); }
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& f, int x, int y) override { const int p = padAt({ x, y }); onDrop(f, p < 0 ? 0 : p); }

private:
    int padAt(juce::Point<int> p) const { for (int i = 0; i < 16; ++i) if (padRect(i).contains(p)) return i; return -1; }
    void press(const juce::MouseEvent& e)
    {
        const int pad = padAt(e.getPosition()); if (pad < 0) return;
        const auto r = padRect(pad); float v;
        if (fixed.getToggleState()) v = (float) fixedVel.getValue() / 127.0f;
        else if (e.isPressureValid() && e.pressure > 0.02f && e.source.getType() != juce::MouseInputSource::InputSourceType::mouse) v = 0.1f + 0.9f * e.pressure;
        else v = 0.12f + 0.88f * (1.0f - juce::jlimit(0.0f, 1.0f, (float) (e.y - r.getY()) / (float) r.getHeight()));
        v = juce::jlimit(1.0f / 127.0f, 1.0f, v);
        held[e.source.getIndex()] = pad; lastVel[pad] = v;
        proc.keyboardState.noteOn(1, kPadBase + pad, v);
    }
    void release(int src)
    {
        auto it = held.find(src); if (it == held.end()) return;
        proc.keyboardState.noteOff(1, kPadBase + it->second, 0.0f); held.erase(it);
    }
    void timerCallback() override
    {
        for (int i = 0; i < 16; ++i)
        {
            const float h = proc.padHit[(size_t) i].exchange(0.0f);
            if (h > 0.0f) lastVel[i] = h; else lastVel[i] *= proc.keyboardState.isNoteOn(1, kPadBase + i) ? 0.995f : 0.9f;
        }
        repaint();
    }
    PcmSynthProcessor& proc; std::function<void(const juce::StringArray&, int)> onDrop;
    juce::TextButton setup, fixed; juce::Slider fixedVel;
    std::map<int, int> held; float lastVel[16];
};

//==============================================================================
class PcmSynthEditor::Content : public juce::Component, private juce::Timer
{
public:
    Content(PcmSynthProcessor& pr) : proc(pr), apvts(pr.apvts), joystick(pr.apvts),
        keyboard(pr.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setLookAndFeel(&laf);
        auto knob = [&](const char* id, const char* t, juce::Colour c, std::function<juce::String(double)> f) {
            knobs.push_back(std::make_unique<Knob>(apvts, id, t, c, std::move(f), laf)); addAndMakeVisible(*knobs.back()); return knobs.back().get(); };
        auto pct = [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; };

        // mode buttons
        for (int i = 0; i < 3; ++i)
        {
            static const char* nm[3] = { "PROGRAM", "COMBI", "MULTI" };
            modeBtn[i] = std::make_unique<ModeButton>(nm[i]); addAndMakeVisible(*modeBtn[i]);
            modeBtn[i]->onClick = [this, i] { setChoice("mode", i); };
        }
        // data entry
        data.setSliderStyle(juce::Slider::LinearHorizontal); data.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        data.setRange(0, 100, 1); data.setLookAndFeel(&laf); addAndMakeVisible(data);
        data.onValueChange = [this] { if (!updating) setChoice(currentParamId(), juce::jmin(dataMax(), juce::roundToInt(data.getValue()))); };
        for (int i = 0; i < 2; ++i)
        {
            step[i].setButtonText(i == 0 ? "-" : "+"); addAndMakeVisible(step[i]);
            step[i].onClick = [this, i] { data.setValue(juce::jlimit(0.0, (double) dataMax(), data.getValue() + (i == 0 ? -1 : 1)), juce::sendNotificationSync); };
        }
        knob("master", "Master", cream, [](double v) { return juce::String(v, 1) + " dB"; });

        // parts table
        for (int i = 0; i < 8; ++i)
        {
            const juce::String s(i + 1);
            auto& r = row[(size_t) i];
            r.num.setButtonText(s); r.num.onClick = [this, i] { selPart = i + 1; };
            addAndMakeVisible(r.num);
            for (int k = 0; k < pcm::kAllPrograms; ++k) r.prog.addItem(pcm::programs()[(size_t) k].name, k + 1);
            addAndMakeVisible(r.prog);
            r.progAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "p" + s + "prog", r.prog);
            auto bar = [&](juce::Slider& sl, const juce::String& id, std::function<juce::String(double)> txt, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
                sl.setSliderStyle(juce::Slider::LinearBar); sl.setTextBoxIsEditable(false); sl.setLookAndFeel(&laf);
                addAndMakeVisible(sl);
                att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, sl);
                sl.textFromValueFunction = txt; sl.updateText();   // after the attachment, which installs its own formatter
                if (auto* p = apvts.getParameter(id)) sl.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue())); };
            bar(r.lvl, "p" + s + "lvl", pct, r.lvlAtt);
            bar(r.pan, "p" + s + "pan", [](double v) { const int p = juce::roundToInt(v * 100); return p == 0 ? juce::String("C") : (p < 0 ? "L" : "R") + juce::String(std::abs(p)); }, r.panAtt);
            auto nn = [](double v) { static const char* a[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; const int n = (int) v; return juce::String(a[n % 12]) + juce::String(n / 12 - 2); };
            bar(r.lo, "p" + s + "lo", nn, r.loAtt); bar(r.hi, "p" + s + "hi", nn, r.hiAtt);
            bar(r.tr, "p" + s + "tr", [](double v) { return (v > 0 ? "+" : "") + juce::String((int) v); }, r.trAtt);
            bar(r.ch, "p" + s + "ch", [](double v) { return "ch " + juce::String((int) v); }, r.chAtt);
        }

        // tone macros
        knob("cutoff", "Brightness", amber, [](double v) { return (v > 0 ? "+" : "") + juce::String(v, 2); });
        knob("res", "Resonance", amber, [](double v) { return juce::String(juce::roundToInt(v / 0.6 * 100)) + "%"; });
        knob("attack", "Attack", teal, [](double v) { return "x" + juce::String(v, 2); });
        knob("release", "Release", teal, [](double v) { return "x" + juce::String(v, 2); });
        knob("lfo", "LFO Depth", green, [](double v) { return "x" + juce::String(v, 2); });
        knob("bend", "Bend Range", slate, [](double v) { return juce::String((int) v) + " st"; });

        // effects
        for (int i = 0; i < 2; ++i)
        {
            const juce::String s(i + 1);
            for (int t = 0; t < pcm::FxCount; ++t) fxType[i].addItem(pcm::fxName(t), t + 1);
            addAndMakeVisible(fxType[i]);
            fxAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "fx" + s + "type", fxType[i]);
            fxKnob[i][0] = knob(("fx" + s + "a").toRawUTF8(), "A", blue, [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
            fxKnob[i][1] = knob(("fx" + s + "b").toRawUTF8(), "B", blue, [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
            fxKnob[i][2] = knob(("fx" + s + "mix").toRawUTF8(), "Mix", red, [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
        }

        for (int i = 0; i < 3; ++i)
        {
            static const char* tn[3] = { "SYNTH", "SAMPLES", "PADS" };
            tab[i].setButtonText(tn[i]); tab[i].setClickingTogglesState(false); addAndMakeVisible(tab[i]);
            tab[i].onClick = [this, i] { setTab(i); };
        }
        samples = std::make_unique<SamplesPage>(proc, laf); addChildComponent(*samples);
        pads = std::make_unique<PadsPage>(proc, laf, [this](const juce::StringArray& f, int slot) { samples->loadFiles(f, slot); });
        addChildComponent(*pads);
        addAndMakeVisible(joystick);
        keyboard.setAvailableRange(36, 96); keyboard.setKeyWidth(21.8f); keyboard.setScrollButtonsVisible(false);
        keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, amber.withAlpha(0.7f));
        keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, amber.withAlpha(0.25f));
        keyboard.setMidiChannel(1);
        addAndMakeVisible(keyboard);
        setTab(0);
        startTimerHz(30);
        timerCallback();
    }
    ~Content() override
    {
        stopTimer(); samples.reset(); pads.reset(); setLookAndFeel(nullptr); data.setLookAndFeel(nullptr);
        for (auto& r : row) for (auto* s : { &r.lvl, &r.pan, &r.lo, &r.hi, &r.tr, &r.ch }) s->setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.setGradientFill(juce::ColourGradient(panelTop, 0, 0, panelBottom, 0, (float) getHeight(), false)); g.fillAll();
        juce::Random rng(9); g.setColour(juce::Colours::white.withAlpha(0.02f));
        for (int y = 0; y < getHeight(); y += 2) if (rng.nextInt(3) == 0) g.drawHorizontalLine(y, 0.0f, (float) getWidth());
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRect(getLocalBounds(), 2);
        screw(g, 12, 12); screw(g, getWidth() - 12.0f, 12); screw(g, 12, getHeight() - 12.0f); screw(g, getWidth() - 12.0f, getHeight() - 12.0f);

        // brand
        g.setColour(ink); g.setFont(juce::FontOptions(36.0f, juce::Font::bold)); g.drawText("PCM", 30, 22, 220, 40, juce::Justification::centredLeft);
        g.setColour(amber); g.setFont(juce::FontOptions(14.0f, juce::Font::bold)); g.drawText("WORKSTATION", 32, 60, 220, 18, juce::Justification::centredLeft);
        g.setColour(dim); g.setFont(juce::FontOptions(9.5f)); g.drawText("16 voices  /  8 parts  /  100 programs  /  100 combinations", 32, 80, 240, 14, juce::Justification::centredLeft);

        // LCD
        auto lcd = juce::Rectangle<float>(270, 16, 372, 88);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(lcd.translated(0, 2), 6.0f);
        g.setGradientFill(juce::ColourGradient(lcdBg.brighter(0.08f), 0, lcd.getY(), lcdBg, 0, lcd.getBottom(), false)); g.fillRoundedRectangle(lcd, 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.14f)); g.drawRoundedRectangle(lcd, 6.0f, 1.0f);
        g.setColour(amber); g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::bold));
        g.drawText(lcd1, lcd.reduced(12, 8).removeFromTop(24), juce::Justification::centredLeft);
        g.setColour(amber.withAlpha(0.8f)); g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain));
        g.drawText(lcd2, lcd.reduced(12, 8).withTrimmedTop(26).removeFromTop(18), juce::Justification::centredLeft);
        g.drawText(lcd3, lcd.reduced(12, 8).withTrimmedTop(46).removeFromTop(18), juce::Justification::centredLeft);
        // output meter in the LCD
        const int segs = 24; const float lvl = juce::Decibels::gainToDecibels(meter, -60.0f);
        for (int i = 0; i < segs; ++i)
        {
            const float db = -60.0f + (float) (i + 1) / segs * 60.0f; const bool lit = lvl >= db - 1.2f;
            g.setColour(lit ? (db > -3 ? red : amber) : amber.withAlpha(0.12f));
            g.fillRect(lcd.getX() + 12 + i * 10.5f, lcd.getBottom() - 15, 8.0f, 6.0f);
        }

        auto head = [&](const juce::String& t, int x, int y, int w, juce::Colour c) {
            g.setColour(c); g.fillRoundedRectangle((float) x, (float) y + 2, 4.0f, 12.0f, 1.5f);
            g.setColour(ink.withAlpha(0.9f)); g.setFont(juce::FontOptions(11.0f, juce::Font::bold)); g.drawText(t, x + 10, y, w, 16, juce::Justification::centredLeft);
            g.setColour(juce::Colours::white.withAlpha(0.10f)); g.drawHorizontalLine(y + 19, (float) x, (float) (x + w)); };
        if (curTab != 0) return;
        head("PARTS", 24, 122, 640, amber);
        head("TONE", 690, 122, 266, teal);
        head("EFFECT 1", 24, 372, 452, blue);
        head("EFFECT 2", 504, 372, 452, blue);

        g.setColour(dim); g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        const int hy = 144; const char* cols[] = { "PROGRAM", "LEVEL", "PAN", "LOW", "HIGH", "TRANS", "CHAN" };
        const int cx[] = { 58, 236, 332, 402, 456, 510, 568 }, cw[] = { 170, 92, 66, 50, 50, 54, 50 };
        for (int i = 0; i < 7; ++i) g.drawText(cols[i], cx[i], hy, cw[i], 12, juce::Justification::centred);

        // selected-part highlight
        const bool multi = proc.apvts.getRawParameterValue("mode")->load() > 1.5f;
        if (multi) { g.setColour(amber.withAlpha(0.10f)); g.fillRoundedRectangle(20.0f, (float) (158 + (selPart - 1) * 26), 648.0f, 26.0f, 4.0f); }
    }

    void resized() override
    {
        for (int i = 0; i < 3; ++i) modeBtn[i]->setBounds(660 + i * 70, 22, 66, 44);
        step[0].setBounds(660, 76, 26, 24); data.setBounds(690, 74, 160, 28); step[1].setBounds(854, 76, 26, 24);
        knobs[0]->setBounds(886, 16, 80, 92);

        for (int i = 0; i < 8; ++i)
        {
            auto& r = row[(size_t) i]; const int y = 160 + i * 26;
            r.num.setBounds(28, y, 24, 22); r.prog.setBounds(58, y, 170, 22);
            r.lvl.setBounds(236, y, 92, 22); r.pan.setBounds(332, y, 66, 22); r.lo.setBounds(402, y, 50, 22);
            r.hi.setBounds(456, y, 50, 22); r.tr.setBounds(510, y, 54, 22); r.ch.setBounds(568, y, 50, 22);
        }
        // tone knobs 3 x 2
        for (int i = 0; i < 6; ++i) knobs[(size_t) (1 + i)]->setBounds(690 + (i % 3) * 90, 148 + (i / 3) * 110, 88, 106);
        // effects
        for (int i = 0; i < 2; ++i)
        {
            const int x0 = i == 0 ? 24 : 504;
            fxType[i].setBounds(x0, 398, 110, 24);
            for (int k = 0; k < 3; ++k) fxKnob[i][k]->setBounds(x0 + 130 + k * 108, 394, 100, 112);
        }
        for (int i = 0; i < 3; ++i) tab[i].setBounds(30 + i * 74, 98, 70, 20);
        samples->setBounds(20, 122, 940, 396); pads->setBounds(20, 122, 940, 396);
        joystick.setBounds(24, 520, 128, 160);
        keyboard.setBounds(170, 524, 790, 156);
    }

private:
    struct Row
    {
        juce::TextButton num; juce::ComboBox prog; juce::Slider lvl, pan, lo, hi, tr, ch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> progAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lvlAtt, panAtt, loAtt, hiAtt, trAtt, chAtt;
    };

    void setTab(int t)
    {
        curTab = t;
        for (int i = 0; i < 3; ++i) tab[i].setToggleState(i == t, juce::dontSendNotification);
        const bool s = t == 0;
        for (size_t i = 1; i < knobs.size(); ++i) knobs[i]->setVisible(s);
        for (auto& r : row) for (juce::Component* k : std::initializer_list<juce::Component*>{ &r.num, &r.prog, &r.lvl, &r.pan, &r.lo, &r.hi, &r.tr, &r.ch }) k->setVisible(s);
        for (auto& f : fxType) f.setVisible(s);
        samples->setVisible(t == 1); pads->setVisible(t == 2);
        repaint();
    }
    int mode() const { return juce::roundToInt(proc.apvts.getRawParameterValue("mode")->load()); }
    int dataMax() const { return mode() == 1 ? 99 : 100; }
    juce::String currentParamId() const { const int m = mode(); return m == 0 ? "p1prog" : m == 1 ? "combi" : "p" + juce::String(selPart) + "prog"; }
    void setChoice(const juce::String& id, int idx)
    {
        if (auto* p = apvts.getParameter(id))
        {
            p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1((float) idx)); p->endChangeGesture();
        }
    }

    void timerCallback() override
    {
        const int m = mode();
        for (int i = 0; i < 3; ++i) modeBtn[i]->setToggleState(i == m, juce::dontSendNotification);
        const auto id = currentParamId();
        const int cur = juce::roundToInt(apvts.getRawParameterValue(id)->load());
        updating = true; if (juce::roundToInt(data.getValue()) != cur) data.setValue(cur, juce::dontSendNotification); updating = false;

        const char* ml[3] = { "PROGRAM", "COMBINATION", "MULTI" };
        const auto& progs = pcm::programs();
        if (m == 0) lcd1 = "PROG  P" + juce::String(cur).paddedLeft('0', 2) + " " + juce::String(progs[(size_t) cur].name);
        else if (m == 1) lcd1 = "COMBI C" + juce::String(cur).paddedLeft('0', 2) + " " + juce::String(pcm::combinations()[(size_t) cur].name);
        else lcd1 = "MULTI part " + juce::String(selPart) + " P" + juce::String(cur).paddedLeft('0', 2) + " " + juce::String(progs[(size_t) cur].name);
        lcd1 = lcd1.substring(0, 28);
        int parts = 0; for (int i = 1; i <= 8; ++i) if (apvts.getRawParameterValue("p" + juce::String(i) + "lvl")->load() > 0.001f) ++parts;
        lcd2 = juce::String(ml[m]) + "  PARTS " + juce::String(m == 0 ? 1 : parts) + "  VOICES " + juce::String(proc.voicesActive.load()).paddedLeft('0', 2) + "/16";
        lcd3 = "FX1 " + juce::String(pcm::fxName(juce::roundToInt(apvts.getRawParameterValue("fx1type")->load()))) + "  FX2 "
             + juce::String(pcm::fxName(juce::roundToInt(apvts.getRawParameterValue("fx2type")->load())));
        const float pk = proc.outPeak.load(); meter = pk > meter ? pk : meter * 0.9f;

        static const char* lab[pcm::FxCount][2] = { { "A", "B" }, { "Size", "Damp" }, { "Time", "Feedback" }, { "Rate", "Depth" }, { "Rate", "Feedback" }, { "Drive", "Tone" }, { "Speed", "Depth" } };
        for (int i = 0; i < 2; ++i)
        {
            const int t = juce::jlimit(0, pcm::FxCount - 1, juce::roundToInt(apvts.getRawParameterValue("fx" + juce::String(i + 1) + "type")->load()));
            fxKnob[i][0]->setTitle(lab[t][0]); fxKnob[i][1]->setTitle(lab[t][1]);
        }
        for (int i = 0; i < 8; ++i)
        {
            const float a = (m == 0 && i > 0) ? 0.35f : 1.0f;
            row[(size_t) i].num.setAlpha(a); row[(size_t) i].prog.setAlpha(a);
            for (auto* s : { &row[(size_t) i].lvl, &row[(size_t) i].pan, &row[(size_t) i].lo, &row[(size_t) i].hi, &row[(size_t) i].tr }) s->setAlpha(a);
            row[(size_t) i].ch.setAlpha(m == 2 ? 1.0f : 0.35f);
        }
        repaint();
    }

    PcmSynthProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    SynthLook laf;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<ModeButton> modeBtn[3];
    juce::Slider data; juce::TextButton step[2];
    Row row[8];
    juce::ComboBox fxType[2];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fxAtt[2];
    Knob* fxKnob[2][3] = {};
    JoystickPad joystick;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton tab[3]; std::unique_ptr<SamplesPage> samples; std::unique_ptr<PadsPage> pads; int curTab = 0;
    juce::String lcd1, lcd2, lcd3;
    float meter = 0.0f; int selPart = 1; bool updating = false;
};

//==============================================================================
PcmSynthEditor::PcmSynthEditor(PcmSynthProcessor& p) : juce::AudioProcessorEditor(&p)
{
    content = std::make_unique<Content>(p);
    addAndMakeVisible(*content);
    constrainer.setFixedAspectRatio((double) kBaseW / kBaseH);
    constrainer.setSizeLimits(kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(kBaseW * 9 / 10, kBaseH * 9 / 10);
}
PcmSynthEditor::~PcmSynthEditor() { setConstrainer(nullptr); }

void PcmSynthEditor::resized()
{
    content->setBounds(0, 0, kBaseW, kBaseH);
    content->setTransform(juce::AffineTransform::scale((float) getWidth() / (float) kBaseW));
}
