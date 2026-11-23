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
    // Khu xi: 0% = tat (nhu cu), cao hon = bot xi / e e / tieng gio o dai cao nhung van giu do trong
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "deess", 1 }, "Khu xi",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    // Sang / Am: -100% = am nhat, 0 = giu nguyen, +100% = sang nhat (khoang +-6 dB giua tram va cao)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "warmth", 1 }, "Sang Am",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    // Cat duoi: 0% = tat; cao hon = cong chat hon, cat tieng duoi / nen / e e sau khi ngung noi
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate", 1 }, "Cat duoi",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
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

    // He so cho bo khu xi (phu thuoc sample rate)
    hiLpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * (float) juce::jmin (7000.0, 0.4 * sampleRate) / (float) sampleRate);
    envAtk   = 1.0f - std::exp (-1.0f / (0.0005f * (float) sampleRate));   // 0.5 ms
    envRel   = 1.0f - std::exp (-1.0f / (0.040f  * (float) sampleRate));   // 40 ms
    gainDn   = 1.0f - std::exp (-1.0f / (0.0010f * (float) sampleRate));   // giam gain nhanh 1 ms
    gainUp   = 1.0f - std::exp (-1.0f / (0.0300f * (float) sampleRate));   // hoi gain 30 ms
    hiLp = { 0.0f, 0.0f }; hiEnv = { 0.0f, 0.0f }; hiGain = { 1.0f, 1.0f };

    // He so cho cong Cat duoi
    gateDelay.prepare (latency + 4);
    gateLead = juce::jmax (0, latency - (int) (0.003 * sampleRate));   // mo cong som 3 ms de khong cut dau chu
    gateAtk  = 1.0f - std::exp (-1.0f / (0.001f * (float) sampleRate));
    gateRel  = 1.0f - std::exp (-1.0f / (0.012f * (float) sampleRate));
    gateUp   = 1.0f - std::exp (-1.0f / (0.002f * (float) sampleRate));   // mo 2 ms
    gateDown = 1.0f - std::exp (-1.0f / (0.020f * (float) sampleRate));   // dong 20 ms
    gateEnv = 0.0f; gateGain = 1.0f; gateOpenState = true;

    // He so cho bo Sang / Am
    tiltA  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 800.0f / (float) sampleRate);
    tiltSm = 1.0f - std::exp (-1.0f / (0.020f * (float) sampleRate));   // muot 20 ms, khong bi tach tach
    tiltLp = { 0.0f, 0.0f };
    {
        const float t0 = apvts.getRawParameterValue ("warmth")->load() * 0.01f * 6.0f;
        tiltGL = std::pow (10.0f, -t0 / 20.0f);
        tiltGH = std::pow (10.0f,  t0 / 20.0f);
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

    const float ds   = apvts.getRawParameterValue ("deess")->load() * 0.01f;
    const float thr  = 0.15f * std::pow (0.1f, ds);   // nguong nen xi: -16 dB (0%) -> -36 dB (100%)
    const float flr  = 0.004f * ds + 1.0e-6f;         // san tieng gio: duoi muc nay thi gate nhe

    const float tiltDb = apvts.getRawParameterValue ("warmth")->load() * 0.01f * 6.0f;
    const float tgtGL  = std::pow (10.0f, -tiltDb / 20.0f);
    const float tgtGH  = std::pow (10.0f,  tiltDb / 20.0f);

    const float gAmt       = apvts.getRawParameterValue ("gate")->load() * 0.01f;
    const bool  gateOn     = gAmt > 0.001f;
    const float gateThr    = std::pow (10.0f, (-70.0f + 35.0f * gAmt) / 20.0f);   // -70 dB -> -35 dB
    const float gateCloseT = gateThr * 0.6f;                                      // tre (hysteresis) tranh nhap nhay
    const float gateClosed = std::pow (10.0f, -(10.0f + 25.0f * gAmt) / 20.0f);   // -10 dB -> -35 dB khi cong dong

    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (numIn, (int) shifters.size());

    for (int i = 0; i < n; ++i)
    {
        const float r = ratioSmooth.getNextValue();
        const float clar = clarSmooth.getNextValue();
        tiltGL += tiltSm * (tgtGL - tiltGL);
        tiltGH += tiltSm * (tgtGH - tiltGH);

        // Cong Cat duoi: do muc VAO (truoc khi xu ly), tre cho khop voi dau ra roi moi quyet dinh dong / mo
        float lvl = 0.0f;
        for (int c = 0; c < chans; ++c)
            lvl = juce::jmax (lvl, std::abs (buffer.getReadPointer (c)[i]));
        gateEnv += (lvl > gateEnv ? gateAtk : gateRel) * (lvl - gateEnv);
        const float envAligned = gateDelay.process (gateEnv, gateLead);
        if (! gateOn)                                   gateOpenState = true;
        else if (! gateOpenState && envAligned > gateThr)     gateOpenState = true;
        else if (gateOpenState && envAligned < gateCloseT)    gateOpenState = false;
        const float gateTarget = gateOpenState ? 1.0f : gateClosed;
        gateGain += (gateTarget > gateGain ? gateUp : gateDown) * (gateTarget - gateGain);
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
            const float hi0 = xd - lod;

            // Khu xi: (1) lam diu dai cao, (2) nen dong khi xi qua to, (3) gate nhe tieng gio/e e luc im
            auto& lpS = hiLp[(size_t) c];
            lpS += hiLpCoef * (hi0 - lpS);
            const float hi = hi0 + ds * (lpS - hi0);

            auto& env = hiEnv[(size_t) c];
            const float ae = std::abs (hi);
            env += (ae > env ? envAtk : envRel) * (ae - env);

            float gt = 1.0f;
            if (ds > 0.001f)
            {
                if (env > thr)      gt = std::pow (thr / env, 0.8f * ds);
                else if (env < flr) gt = juce::jmax (0.1f, std::pow (env / flr, ds));
            }
            auto& g = hiGain[(size_t) c];
            g += (gt < g ? gainDn : gainUp) * (gt - g);

            const float y = wet + clar * hi * g;

            // Sang / Am: tach tram / cao quanh 800 Hz roi can bang lai
            auto& tl = tiltLp[(size_t) c];
            tl += tiltA * (y - tl);
            d[i] = (tl * tiltGL + (y - tl) * tiltGH) * gateGain;
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
