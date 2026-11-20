#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include "Dsp.h"

class ViceVoxAudioProcessor : public juce::AudioProcessor
{
public:
    ViceVoxAudioProcessor();
    ~ViceVoxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vice Vox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> level { 0.0f };   // output peak for the UI

private:
    double sampleRate = 44100.0;

    // 2 Auto-Tune
    vv::PitchCorrector tuner;

    // 3 EQ
    vv::Biquad hpf[2], mud[2], pres[2], air[2];

    // 4 Compressor (linked stereo)
    float compEnv = 0.0f, compAtk = 0.0f, compRel = 0.0f;
    static constexpr float compThresh = -20.0f, compRatio = 4.0f, compMakeupDb = 4.0f;

    // 5 De-esser
    vv::Biquad sib[2];
    float dsEnv = 0.0f, dsAtk = 0.0f, dsRel = 0.0f;
    static constexpr float dsThresh = -30.0f;

    // 6 Saturation
    static constexpr float satDrive = 2.0f, satMix = 0.35f;

    // 7 Reverb + Delay (sends)
    juce::Reverb reverb;
    juce::AudioBuffer<float> revBuf;
    std::vector<float> dBuf[2];
    int dMask = 0, dW = 0, dLen[2] { 0, 0 };
    float dLp[2] { 0.0f, 0.0f }, dLpCoef = 0.3f;

    float lastGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViceVoxAudioProcessor)
};
