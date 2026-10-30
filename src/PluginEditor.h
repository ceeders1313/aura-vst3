#pragma once
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class AuraEditor : public juce::AudioProcessorEditor,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
{
public:
    explicit AuraEditor (AuraProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void addKnob (const char* paramId, const char* text);
    void timerCallback() override;
    void loadAndSwitch (const juce::Array<juce::File>& files);

    AuraProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;

    juce::ComboBox wave, source;
    juce::Label waveLabel, sourceLabel, statusLabel;
    juce::ToggleButton oneShot { "One-shot" };
    juce::TextButton loadButton { "Load samples..." };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach, sourceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oneShotAttach;
    std::unique_ptr<juce::FileChooser> chooser;
};
