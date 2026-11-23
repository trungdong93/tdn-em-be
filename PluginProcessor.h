#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include <cmath>

class BabyOldAudioProcessor : public juce::AudioProcessor
{
public:
    BabyOldAudioProcessor();
    ~BabyOldAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TDN Em Be"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters (float ratio);

    static constexpr float maxSemitones = 8.0f;   // nut van toi da +-8 ban cung

    // Bo loc low-pass bac 2 (RBJ)
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;

        void setLowpass (double sr, double fc, double q = 0.70710678)
        {
            fc = juce::jlimit (20.0, 0.49 * sr, fc);
            const double w0 = 2.0 * juce::MathConstants<double>::pi * fc / sr;
            const double cw = std::cos (w0);
            const double alpha = std::sin (w0) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = (float) (((1.0 - cw) * 0.5) / a0);
            b1 = (float) ((1.0 - cw) / a0);
            b2 = b0;
            a1 = (float) ((-2.0 * cw) / a0);
            a2 = (float) ((1.0 - alpha) / a0);
        }

        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() { z1 = z2 = 0.0f; }
    };

    // Doi cao do bang delay-line 2 dau doc, noi suy cubic
    struct Shifter
    {
        std::vector<float> buf;
        int mask = 0;
        int writePos = 0;
        float phase = 0.0f;

        void prepare (int size)
        {
            buf.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writePos = 0;
            phase = 0.0f;
        }

        float tap (float p, float W) const
        {
            const float delay = p * W + 2.0f;
            const float rp = (float) writePos - delay;
            const int i = (int) std::floor (rp);
            const float f = rp - (float) i;

            const float y0 = buf[(size_t) ((i - 1) & mask)];
            const float y1 = buf[(size_t) (i & mask)];
            const float y2 = buf[(size_t) ((i + 1) & mask)];
            const float y3 = buf[(size_t) ((i + 2) & mask)];

            const float c0 = y1;
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + c0;
        }

        static float win (float p)
        {
            const float s = std::sin (juce::MathConstants<float>::pi * p);
            return s * s;
        }

        float process (float in, float ratio, float W)
        {
            buf[(size_t) writePos] = in;

            const float p0 = phase;
            float p1 = phase + 0.5f;
            if (p1 >= 1.0f) p1 -= 1.0f;

            const float out = tap (p0, W) * win (p0) + tap (p1, W) * win (p1);

            phase += (1.0f - ratio) / W;
            phase -= std::floor (phase);

            writePos = (writePos + 1) & mask;
            return out;
        }
    };

    std::array<Shifter, 2> shifters;
    std::array<Biquad, 2> pre, post1, post2;
    juce::SmoothedValue<float> ratioSmooth;
    float windowSamples = 2048.0f;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyOldAudioProcessor)
};
