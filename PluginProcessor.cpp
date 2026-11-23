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

void BabyOldAudioProcessor::updateFilters (float r)
{
    const double sr = currentSampleRate;

    // Truoc khi doi cao do: cat bot tan so cao de khong bi alias khi len giong
    const double fcPre = 0.45 * sr / juce::jmax (1.0f, r);

    // Sau khi doi cao do: cang len giong em be cang cat tieng xi (am s, x, ch)
    const double t = juce::jlimit (0.0, 1.0, (double) (r - 1.0f) / 0.5);
    const double fcPost = std::exp (std::log (0.45 * sr) * (1.0 - t) + std::log (10000.0) * t);

    for (size_t c = 0; c < 2; ++c)
    {
        pre[c].setLowpass (sr, fcPre);
        post1[c].setLowpass (sr, fcPost);
        post2[c].setLowpass (sr, fcPost);
    }
}

void BabyOldAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    windowSamples = (float) (0.030 * sampleRate);   // cua so 30ms
    const int S = (int) (0.005 * sampleRate);       // tim khop pha +-5ms
    const int L = (int) (0.010 * sampleRate);       // so khop tren 10ms
    const int size = juce::nextPowerOfTwo ((int) windowSamples + 2 * S + L + 16);
    for (auto& s : shifters) s.prepare (size, S, L);
    for (auto& f : pre)   f.reset();
    for (auto& f : post1) f.reset();
    for (auto& f : post2) f.reset();

    setLatencySamples ((int) (windowSamples * 0.5f) + S + 2);   // DAW tu bu tre (PDC)

    float st0 = apvts.getRawParameterValue ("semitones")->load();
    if (std::abs (st0) < 0.3f) st0 = 0.0f;   // vung chet o giua: de ve dung giong goc
    const float r0 = std::pow (2.0f, st0 / 12.0f);
    ratioSmooth.reset (sampleRate, 0.05);
    ratioSmooth.setCurrentAndTargetValue (r0);
    updateFilters (r0);
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
    updateFilters (ratioSmooth.getCurrentValue());

    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (numIn, (int) shifters.size());

    for (int i = 0; i < n; ++i)
    {
        const float r = ratioSmooth.getNextValue();
        for (int c = 0; c < chans; ++c)
        {
            float* d = buffer.getWritePointer (c);
            float x = pre[(size_t) c].process (d[i]);
            x = shifters[(size_t) c].process (x, r, windowSamples);
            x = post1[(size_t) c].process (x);
            x = post2[(size_t) c].process (x);
            d[i] = x;
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
