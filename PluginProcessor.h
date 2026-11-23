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

    // BEGIN_DSP
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

    // Doi cao do: delay-line 2 dau doc + can chinh pha (SOLA) de 2 dau cong dong pha -> giong trong, it re
    struct Shifter
    {
        std::vector<float> buf;
        int mask = 0;
        int writePos = 0;
        float p[2] = { 0.0f, 0.5f };   // pha cua 2 dau doc
        float a[2] = { 0.0f, 0.0f };   // do lech can chinh (mau)
        int S = 0;                     // pham vi tim kiem +-S mau
        int L = 0;                     // do dai doan so khop (mau)

        void prepare (int size, int searchSamples, int matchSamples)
        {
            buf.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writePos = 0;
            p[0] = 0.0f; p[1] = 0.5f;
            a[0] = a[1] = 0.0f;
            S = searchSamples;
            L = matchSamples;
        }

        float delayOf (int i, float W) const { return p[i] * W + (float) S + 2.0f + a[i]; }

        float tap (float delay) const
        {
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
            const float s = std::sin (3.14159265f * p);
            return s * s;
        }

        // Khi dau doc i quay lai dau cua so (luc do trong so = 0, khong nghe thay),
        // chon do lech sao cho doan giong moi khop pha nhat voi dau doc con lai.
        void realign (int i, float W)
        {
            const int j = 1 - i;
            const int posJ = writePos - (int) std::lround (delayOf (j, W));
            const float dnom = p[i] * W + (float) S + 2.0f;

            double best = -1.0e30;
            int bestA = 0;
            for (int c = -S; c <= S; ++c)
            {
                const int posI = writePos - (int) std::lround (dnom + (float) c);
                double dot = 0.0, e = 1.0e-9;
                for (int m = 0; m < L; ++m)
                {
                    const float xi = buf[(size_t) ((posI - m) & mask)];
                    dot += (double) xi * (double) buf[(size_t) ((posJ - m) & mask)];
                    e += (double) xi * (double) xi;
                }
                const double sc = dot / std::sqrt (e);
                if (sc > best) { best = sc; bestA = c; }
            }
            a[i] = (float) bestA;
        }

        float process (float in, float ratio, float W)
        {
            buf[(size_t) writePos] = in;

            const float out = tap (delayOf (0, W)) * win (p[0])
                            + tap (delayOf (1, W)) * win (p[1]);

            const float inc = (1.0f - ratio) / W;
            writePos = (writePos + 1) & mask;

            for (int i = 0; i < 2; ++i)
            {
                p[i] += inc;
                bool wrapped = false;
                if (p[i] >= 1.0f)      { p[i] -= 1.0f; wrapped = true; }
                else if (p[i] < 0.0f)  { p[i] += 1.0f; wrapped = true; }
                if (wrapped) realign (i, W);
            }
            return out;
        }
    };
    // END_DSP

    std::array<Shifter, 2> shifters;
    std::array<Biquad, 2> pre, post1, post2;
    juce::SmoothedValue<float> ratioSmooth;
    float windowSamples = 2048.0f;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabyOldAudioProcessor)
};
