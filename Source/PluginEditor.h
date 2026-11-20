#pragma once
#include "PluginProcessor.h"

class ViceVoxAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ViceVoxAudioProcessorEditor (ViceVoxAudioProcessor&);
    ~ViceVoxAudioProcessorEditor() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void setMode (int newMode);
    void drawPalm (juce::Graphics&, float x, float baseY, float h, float lean);

    ViceVoxAudioProcessor& proc;
    juce::Slider reverbKnob, delayKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aRev, aDel, aOut;
    juce::Rectangle<int> btnUnder, btnFlint;
    float phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViceVoxAudioProcessorEditor)
};
