#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout ViceVoxAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                   StringArray { "Off", "Underground", "Flint" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "reverb", 1 }, "Reverb",
                                                  NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "delay", 1 }, "Delay",
                                                  NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output",
                                                  NormalisableRange<float> (-12.0f, 6.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

ViceVoxAudioProcessor::ViceVoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool ViceVoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void ViceVoxAudioProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;

    for (int c = 0; c < 2; ++c)
    {
        // 3 EQ - vocal preset: cut rumble + mud, add presence and air
        hpf[c].setHighPass (sr, 90.0, 0.707);
        mud[c].setPeak (sr, 320.0, 1.0, -2.5);
        pres[c].setPeak (sr, 3500.0, 0.9, 2.5);
        air[c].setHighShelf (sr, 10500.0, 0.7, 3.0);
        // 5 De-esser band
        sib[c].setBandPass (sr, 7000.0, 1.4);

        hpf[c].reset(); mud[c].reset(); pres[c].reset(); air[c].reset(); sib[c].reset();
    }

    compEnv = dsEnv = 0.0f;
    compAtk = (float) std::exp (-1.0 / (0.006 * sr));
    compRel = (float) std::exp (-1.0 / (0.090 * sr));
    dsAtk   = (float) std::exp (-1.0 / (0.0005 * sr));
    dsRel   = (float) std::exp (-1.0 / (0.040 * sr));

    tuner.prepare (sr);

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f; rp.damping = 0.55f; rp.wetLevel = 0.33f;
    rp.dryLevel = 0.0f; rp.width = 1.0f; rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.setSampleRate (sr);
    reverb.reset();
    revBuf.setSize (2, juce::jmax (block, 512), false, true, false);

    int sz = 1;
    while (sz < (int) sr) sz <<= 1;
    dMask = sz - 1;
    for (auto& b : dBuf) b.assign ((size_t) sz, 0.0f);
    dW = 0;
    dLen[0] = (int) (0.27 * sr);
    dLen[1] = (int) (0.33 * sr);
    dLp[0] = dLp[1] = 0.0f;
    dLpCoef = 1.0f - (float) std::exp (-2.0 * vv::kPi * 4000.0 / sr);

    lastGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load());
}

void ViceVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    if (buf.getNumChannels() < 2 || n == 0) return;

    const int mode = (int) apvts.getRawParameterValue ("mode")->load();   // 0 off, 1 Underground, 2 Flint
    if (mode == 0)
    {
        level.store (level.load() * 0.9f);
        return;                                                           // bypass
    }

    const float revSend = apvts.getRawParameterValue ("reverb")->load() * 0.01f * 0.35f;
    const float dlySend = apvts.getRawParameterValue ("delay")->load() * 0.01f * 0.30f;
    const float outGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load());
    const float makeup  = juce::Decibels::decibelsToGain (compMakeupDb);

    float* L = buf.getWritePointer (0);
    float* R = buf.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        float x[2] = { L[i], R[i] };

        // 2 Auto-Tune (Underground only)
        if (mode == 1) tuner.process (x[0], x[1]);

        // 3 EQ
        for (int c = 0; c < 2; ++c)
        {
            float v = x[c];
            v = hpf[c].process (v);
            v = mud[c].process (v);
            v = pres[c].process (v);
            v = air[c].process (v);
            x[c] = v;
        }

        // 4 Compressor
        {
            const float lvl = std::max (std::abs (x[0]), std::abs (x[1]));
            const float co = lvl > compEnv ? compAtk : compRel;
            compEnv = co * compEnv + (1.0f - co) * lvl;
            const float envDb = juce::Decibels::gainToDecibels (compEnv, -100.0f);
            const float over = envDb - compThresh;
            const float gr = over > 0.0f ? over * (1.0f - 1.0f / compRatio) : 0.0f;
            const float g = juce::Decibels::decibelsToGain (-gr) * makeup;
            x[0] *= g; x[1] *= g;
        }

        // 5 De-esser
        {
            const float s0 = sib[0].process (x[0]);
            const float s1 = sib[1].process (x[1]);
            const float sl = std::max (std::abs (s0), std::abs (s1));
            const float co = sl > dsEnv ? dsAtk : dsRel;
            dsEnv = co * dsEnv + (1.0f - co) * sl;
            const float dsDb = juce::Decibels::gainToDecibels (dsEnv, -100.0f);
            const float red = dsDb > dsThresh ? std::min ((dsDb - dsThresh) * 0.75f, 10.0f) : 0.0f;
            const float gDs = juce::Decibels::decibelsToGain (-red);
            x[0] -= s0 * (1.0f - gDs);
            x[1] -= s1 * (1.0f - gDs);
        }

        // 6 Saturation
        for (int c = 0; c < 2; ++c)
        {
            const float sat = std::tanh (x[c] * satDrive) / satDrive;
            x[c] = x[c] * (1.0f - satMix) + sat * satMix;
        }

        L[i] = x[0];
        R[i] = x[1];
    }

    // 7 Reverb + Delay on sends (Underground only)
    if (mode == 1)
    {
        if (revBuf.getNumSamples() < n) revBuf.setSize (2, n, false, true, true);
        float* rl = revBuf.getWritePointer (0);
        float* rr = revBuf.getWritePointer (1);
        for (int i = 0; i < n; ++i) { rl[i] = L[i] * revSend; rr[i] = R[i] * revSend; }
        reverb.processStereo (rl, rr, n);
        for (int i = 0; i < n; ++i) { L[i] += rl[i]; R[i] += rr[i]; }

        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float d = dBuf[c][(size_t) ((dW - dLen[c]) & dMask)];
                dLp[c] += dLpCoef * (d - dLp[c]);
                dBuf[c][(size_t) dW] = ch[c][i] * dlySend + dLp[c] * 0.32f;
                ch[c][i] += dLp[c];
            }
            dW = (dW + 1) & dMask;
        }
    }

    buf.applyGainRamp (0, n, lastGain, outGain);
    lastGain = outGain;

    const float pk = std::max (buf.getMagnitude (0, 0, n), buf.getMagnitude (1, 0, n));
    level.store (std::max (pk, level.load() * 0.92f));
}

juce::AudioProcessorEditor* ViceVoxAudioProcessor::createEditor() { return new ViceVoxAudioProcessorEditor (*this); }

void ViceVoxAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}

void ViceVoxAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ViceVoxAudioProcessor(); }
