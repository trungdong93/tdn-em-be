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

    // Bo doi cao do dang delay-line 2 dau doc (giong resample) -> pitch va formant doi cung nhau
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
            const float delay = p * W + 1.0f;
            const float rp = (float) writePos - delay;
            const int i = (int) std::floor (rp);
            const float f = rp - (float) i;
            return buf[(size_t) (i & mask)] * (1.0f - f)
                 + buf[(size_t) ((i + 1) & mask)] * f;
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
    juce::SmoothedValue<float> ratioSmooth;
    float windowSamples = 2048.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyOldAudioProcessor)
};
