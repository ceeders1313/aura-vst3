#pragma once
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class AuraEditor : public juce::AudioProcessorEditor
{
public:
    explicit AuraEditor (AuraProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void addKnob (const char* paramId, const char* text);

    AuraProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ComboBox wave;
    juce::Label waveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
};
