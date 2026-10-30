#pragma once
#include "PluginProcessor.h"
#include <functional>
#include <memory>
#include <vector>

// Grille de potentiometres lies a des parametres
class KnobSet
{
public:
    KnobSet (juce::Component& owner, juce::AudioProcessorValueTreeState& s) : parent (owner), apvts (s) {}
    void add (const char* id, const char* text);
    void layout (juce::Rectangle<int> area, int cols);

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    juce::Component& parent;
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<Knob>> knobs;
};

struct LabeledCombo
{
    juce::Label label;
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;

    void init (juce::Component& parent, juce::AudioProcessorValueTreeState& s, const char* id,
               const juce::String& text, const juce::StringArray& items)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredRight);
        box.addItemList (items, 1);
        parent.addAndMakeVisible (label);
        parent.addAndMakeVisible (box);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
    }
    void place (juce::Rectangle<int>& row, int labelW, int boxW)
    {
        label.setBounds (row.removeFromLeft (labelW));
        box.setBounds (row.removeFromLeft (boxW));
        row.removeFromLeft (8);
    }
};

// Editeur graphique du MSEG
class MsegEditor : public juce::Component
{
public:
    std::function<void (const aura::MsegShape&)> onChange;

    void setShape (const aura::MsegShape& s) { shape = s; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragPoint = -1; dragCurve = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (12.f); }
    juce::Point<float> toPx (float x, float y) const;
    void fromPx (juce::Point<float> p, float& x, float& y) const;
    int hitPoint (juce::Point<float> p) const;
    int hitCurve (juce::Point<float> p) const;
    void notify() { repaint(); if (onChange) onChange (shape); }

    aura::MsegShape shape;
    int dragPoint = -1, dragCurve = -1;
    float curveStart = 0.f, dragStartY = 0.f;
};

class SoundPage : public juce::Component, private juce::Timer
{
public:
    explicit SoundPage (AuraProcessor&);
    void resized() override;
    void loadFiles (const juce::Array<juce::File>& files);

private:
    void timerCallback() override;

    AuraProcessor& proc;
    LabeledCombo source, wave, fmAlgo;
    juce::ToggleButton oneShot { "One-shot" };
    juce::TextButton loadButton { "Load samples..." };
    juce::Label statusLabel;
    KnobSet knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oneShotAttach;
    std::unique_ptr<juce::FileChooser> chooser;
};

class ModPage : public juce::Component
{
public:
    explicit ModPage (AuraProcessor&);
    void resized() override;

private:
    struct Row
    {
        juce::Label num;
        juce::ComboBox src, dst;
        juce::Slider amt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aS, aD;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aA;
    };
    std::vector<std::unique_ptr<Row>> rows;
    LabeledCombo lfo1Shape, lfo2Shape;
    KnobSet knobs;
};

class MsegPage : public juce::Component
{
public:
    explicit MsegPage (AuraProcessor&);
    void resized() override;

private:
    AuraProcessor& proc;
    MsegEditor editor;
    juce::Label hint;
    juce::ToggleButton loop { "Loop" };
    KnobSet knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttach;
};

class AuraEditor : public juce::AudioProcessorEditor,
                   public juce::FileDragAndDropTarget
{
public:
    explicit AuraEditor (AuraProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    AuraProcessor& proc;
    SoundPage sound;
    ModPage mod;
    MsegPage mseg;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};
