#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout BabyOldAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // -1 = ong gia (thap 1 quang tam), 0 = giong that, +1 = em be (cao 1 quang tam)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "morph", 1 }, "Baby - Old",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
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
    windowSamples = (float) (0.040 * sampleRate);   // cua so 40ms
    const int size = juce::nextPowerOfTwo ((int) windowSamples + 8);
    for (auto& s : shifters) s.prepare (size);

    setLatencySamples ((int) (windowSamples * 0.5f) + 1);   // DAW tu bu tre (PDC)

    ratioSmooth.reset (sampleRate, 0.05);
    ratioSmooth.setCurrentAndTargetValue (std::pow (2.0f, apvts.getRawParameterValue ("morph")->load()));
}

void BabyOldAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    for (int c = numIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const float morph = apvts.getRawParameterValue ("morph")->load();
    ratioSmooth.setTargetValue (std::pow (2.0f, morph));   // 2^morph: 0.5x .. 2x

    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (numIn, (int) shifters.size());

    for (int i = 0; i < n; ++i)
    {
        const float r = ratioSmooth.getNextValue();
        for (int c = 0; c < chans; ++c)
        {
            float* d = buffer.getWritePointer (c);
            d[i] = shifters[(size_t) c].process (d[i], r, windowSamples);
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
