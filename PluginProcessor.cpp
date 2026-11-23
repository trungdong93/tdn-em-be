#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout BabyOldAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Don vi: ban cung. -12 = ong gia (thap 1 quang tam), 0 = giong that, +12 = em be (cao 1 quang tam)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "semitones", 1 }, "Pitch",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("st")));
    // Do trong: 100% = giu nguyen toan bo dai cao cua giong goc (trong nhat, het xe xe)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "clarity", 1 }, "Trong",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    return layout;
}

BabyOldAudioProcessor::BabyOldAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool BabyOldAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void BabyOldAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    const int half = juce::jmax (8, (int) std::lround (0.015 * sampleRate));
    windowSamples = (float) (2 * half);              // cua so ~30ms, CHAN de tre la so nguyen
    const int S = (int) (0.005 * sampleRate);        // tim khop pha +-5ms
    const int L = (int) (0.010 * sampleRate);        // so khop tren 10ms
    const int size = juce::nextPowerOfTwo ((int) windowSamples + 2 * S + L + 16);
    for (auto& s : shifters) s.prepare (size, S, L);

    latency = half + S + 2;                          // = do tre cua shifter khi giong goc
    setLatencySamples (latency);                     // DAW tu bu tre (PDC)

    const double fc = juce::jmin (4500.0, 0.225 * sampleRate);
    for (size_t c = 0; c < 2; ++c)
    {
        lp1[c].setLowpass (sampleRate, fc);
        lp2[c].setLowpass (sampleRate, fc);
        lp1[c].reset();
        lp2[c].reset();
        dryFull[c].prepare (latency + 4);
        dryLow[c].prepare (latency + 4);
    }

    float st0 = apvts.getRawParameterValue ("semitones")->load();
    if (std::abs (st0) < 0.3f) st0 = 0.0f;   // vung chet o giua: de ve dung giong goc
    ratioSmooth.reset (sampleRate, 0.05);
    ratioSmooth.setCurrentAndTargetValue (std::pow (2.0f, st0 / 12.0f));
    clarSmooth.reset (sampleRate, 0.03);
    clarSmooth.setCurrentAndTargetValue (apvts.getRawParameterValue ("clarity")->load() * 0.01f);
}

void BabyOldAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    for (int c = numIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    float st = apvts.getRawParameterValue ("semitones")->load();
    if (std::abs (st) < 0.3f) st = 0.0f;   // vung chet o giua: de ve dung giong goc
    ratioSmooth.setTargetValue (std::pow (2.0f, st / 12.0f));
    clarSmooth.setTargetValue (apvts.getRawParameterValue ("clarity")->load() * 0.01f);

    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (numIn, (int) shifters.size());

    for (int i = 0; i < n; ++i)
    {
        const float r = ratioSmooth.getNextValue();
        const float clar = clarSmooth.getNextValue();
        for (int c = 0; c < chans; ++c)
        {
            float* d = buffer.getWritePointer (c);
            const float x = d[i];

            // Dai tram: chi phan nay moi bi doi cao do
            const float lo  = lp2[(size_t) c].process (lp1[(size_t) c].process (x));
            const float wet = shifters[(size_t) c].process (lo, r, windowSamples);

            // Dai cao: lay tu giong goc (tre bang shifter) -> tieng xi, hoi tho trong va tu nhien
            const float xd  = dryFull[(size_t) c].process (x,  latency);
            const float lod = dryLow [(size_t) c].process (lo, latency);
            const float hi  = xd - lod;

            d[i] = wet + clar * hi;
        }
    }
}

juce::AudioProcessorEditor* BabyOldAudioProcessor::createEditor()
{
    return new BabyOldEditor (*this);
}

void BabyOldAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BabyOldAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BabyOldAudioProcessor();
}
