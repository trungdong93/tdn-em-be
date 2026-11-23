#pragma once
#include "PluginProcessor.h"

// Nut van to, vong mau xanh (ong gia) / hong (em be)
struct KnobLook : public juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& slider) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (10.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = startA + pos * (endA - startA);
        const float mid = startA + 0.5f * (endA - startA);
        const juce::PathStrokeType stroke (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, startA, endA, true);
        g.setColour (juce::Colour (0xff343846));
        g.strokePath (track, stroke);

        if (std::abs (ang - mid) > 0.02f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (mid, ang), juce::jmax (mid, ang), true);
            g.setColour (ang > mid ? juce::Colour (0xffffb347) : juce::Colour (0xff7fb8ff));
            g.strokePath (val, stroke);
        }

        // Vach chia giua (can bang) o vi tri 12 gio
        {
            const float inner = r - 10.0f, outer = r + 9.0f;
            g.setColour (juce::Colours::white);
            g.drawLine (c.x + std::sin (mid) * inner, c.y - std::cos (mid) * inner,
                        c.x + std::sin (mid) * outer, c.y - std::cos (mid) * outer, 3.0f);
        }

        const float br = r - 24.0f;
        juce::ColourGradient grad (juce::Colour (0xff4f5468), c.x - br * 0.5f, c.y - br * 0.6f,
                                   juce::Colour (0xff1e2029), c.x + br * 0.6f, c.y + br * 0.7f, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (juce::Colour (0xff12131a));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 3.0f);

        // So ban cung ngay giua nut
        {
            double st = slider.getValue();
            if (std::abs (st) < 0.3) st = 0.0;
            juce::String txt;
            if (st == 0.0)
                txt = "0";
            else
            {
                const double a = std::abs (st);
                const juce::String v = (std::abs (a - std::round (a)) < 0.05) ? juce::String ((int) std::round (a))
                                                                              : juce::String (a, 1);
                txt = (st > 0 ? "+" : "-") + v;
            }
            g.setColour (st > 0 ? juce::Colour (0xffffb347) : (st < 0 ? juce::Colour (0xff7fb8ff) : juce::Colours::white));
            g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
            g.drawText (txt, juce::Rectangle<float> (c.x - br, c.y - 34.0f, br * 2.0f, 56.0f), juce::Justification::centred);
            g.setColour (juce::Colour (0xff9aa0b2));
            g.setFont (juce::FontOptions (14.0f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("bán cung")),
                        juce::Rectangle<float> (c.x - br, c.y + 22.0f, br * 2.0f, 20.0f), juce::Justification::centred);
        }

        juce::Path p;
        p.addRoundedRectangle (-4.0f, -br + 10.0f, 8.0f, br * 0.42f, 4.0f);
        g.setColour (juce::Colours::white);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};

// Nut mui ten: bam de an / hien 2 thanh chinh (mui ten xuong = dang mo, sang phai = dang an)
struct ArrowToggle : public juce::Button
{
    ArrowToggle() : juce::Button ("arrowToggle") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        g.setColour (over ? juce::Colours::white : juce::Colour (0xff9aa0b2));
        juce::Path tri;
        if (getToggleState())   // dang mo -> mui ten xuong
            tri.addTriangle (c.x - 7.0f, c.y - 4.0f, c.x + 7.0f, c.y - 4.0f, c.x, c.y + 6.0f);
        else                    // dang an -> mui ten sang phai
            tri.addTriangle (c.x - 4.0f, c.y - 7.0f, c.x - 4.0f, c.y + 7.0f, c.x + 6.0f, c.y);
        g.fillPath (tri);
    }
};

class BabyOldEditor : public juce::AudioProcessorEditor
{
public:
    explicit BabyOldEditor (BabyOldAudioProcessor& p)
        : AudioProcessorEditor (&p), attach (p.apvts, "semitones", knob),
          clarityAttach (p.apvts, "clarity", clarity),
          deessAttach (p.apvts, "deess", deess)
    {
        knob.setLookAndFeel (&look);
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
        knob.setDoubleClickReturnValue (true, 0.0);   // double-click = ve giua (giong that)
        addAndMakeVisible (knob);

        clarity.setSliderStyle (juce::Slider::LinearHorizontal);
        clarity.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
        clarity.setTextValueSuffix (" %");
        clarity.setDoubleClickReturnValue (true, 100.0);
        clarity.setColour (juce::Slider::trackColourId, juce::Colour (0xff7fb8ff));
        clarity.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff343846));
        clarity.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        clarity.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        clarity.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (clarity);

        deess.setSliderStyle (juce::Slider::LinearHorizontal);
        deess.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
        deess.setTextValueSuffix (" %");
        deess.setDoubleClickReturnValue (true, 50.0);
        deess.setColour (juce::Slider::trackColourId, juce::Colour (0xffffb347));
        deess.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff343846));
        deess.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        deess.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        deess.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (deess);

        arrow.setToggleState (true, juce::dontSendNotification);   // mac dinh: dang mo
        arrow.onClick = [this] { updateLayoutState(); };
        addAndMakeVisible (arrow);

        updateLayoutState();
    }

    ~BabyOldEditor() override { knob.setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1d24));
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("TDN Em Bé")),
                    0, 10, getWidth(), 32, juce::Justification::centred);
        g.setColour (juce::Colour (0xff9aa0b2));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("Ông già ↔ Em bé")),
                    0, 42, getWidth(), 20, juce::Justification::centred);

        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff7fb8ff));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("ÔNG GIÀ")),
                    20, 372, 140, 26, juce::Justification::left);
        g.setColour (juce::Colour (0xffffb347));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("EM BÉ")),
                    getWidth() - 160, 372, 140, 26, juce::Justification::right);

        g.setColour (juce::Colour (0xff9aa0b2));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("Tùy chỉnh giọng")),
                    44, 402, 200, 26, juce::Justification::left);

        if (arrow.getToggleState())
        {
            g.drawText (juce::String (juce::CharPointer_UTF8 ("Độ trong (100% = hết xè xè)")),
                        20, 436, getWidth() - 40, 20, juce::Justification::left);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("Khử xì (kéo lên để bớt xì, è è)")),
                        20, 494, getWidth() - 40, 20, juce::Justification::left);
        }
    }

    void resized() override
    {
        knob.setBounds (30, 68, 300, 300);
        arrow.setBounds (14, 402, 26, 26);
        clarity.setBounds (20, 458, getWidth() - 40, 28);
        deess.setBounds (20, 516, getWidth() - 40, 28);
    }

private:
    void updateLayoutState()
    {
        const bool open = arrow.getToggleState();
        clarity.setVisible (open);
        deess.setVisible (open);
        setSize (360, open ? 560 : 436);   // dong lai thi cua so thap xuong
        repaint();
    }

    KnobLook look;
    juce::Slider knob;
    juce::AudioProcessorValueTreeState::SliderAttachment attach;
    juce::Slider clarity;
    juce::AudioProcessorValueTreeState::SliderAttachment clarityAttach;
    juce::Slider deess;
    juce::AudioProcessorValueTreeState::SliderAttachment deessAttach;
    ArrowToggle arrow;
};
