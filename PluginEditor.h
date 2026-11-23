#pragma once
#include "PluginProcessor.h"

// Nut van to, vong mau xanh (ong gia) / hong (em be)
struct KnobLook : public juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider&) override
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
            g.setColour (ang > mid ? juce::Colour (0xffff9ac8) : juce::Colour (0xff7fb8ff));
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

        juce::Path p;
        p.addRoundedRectangle (-4.0f, -br + 10.0f, 8.0f, br * 0.42f, 4.0f);
        g.setColour (juce::Colours::white);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};

class BabyOldEditor : public juce::AudioProcessorEditor
{
public:
    explicit BabyOldEditor (BabyOldAudioProcessor& p)
        : AudioProcessorEditor (&p), attach (p.apvts, "morph", knob)
    {
        knob.setLookAndFeel (&look);
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
        knob.setDoubleClickReturnValue (true, 0.0);   // double-click = ve giua (giong that)
        addAndMakeVisible (knob);
        setSize (360, 420);
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
                    20, getHeight() - 50, 140, 26, juce::Justification::left);
        g.setColour (juce::Colour (0xffff9ac8));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("EM BÉ")),
                    getWidth() - 160, getHeight() - 50, 140, 26, juce::Justification::right);
    }

    void resized() override { knob.setBounds (30, 68, 300, 300); }

private:
    KnobLook look;
    juce::Slider knob;
    juce::AudioProcessorValueTreeState::SliderAttachment attach;
};
