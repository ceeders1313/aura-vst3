#pragma once
#include "PluginProcessor.h"
#include <functional>
#include <memory>
#include <vector>

using APVTS_ = juce::AudioProcessorValueTreeState;

class KnobSet
{
public:
    KnobSet (juce::Component& owner, APVTS_& s) : parent (owner), apvts (s) {}
    void add (const juce::String& id, const juce::String& text);
    void layout (juce::Rectangle<int> area, int cols);

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS_::SliderAttachment> attach;
    };
    juce::Component& parent;
    APVTS_& apvts;
    std::vector<std::unique_ptr<Knob>> knobs;
};

struct LabeledCombo
{
    juce::Label label;
    juce::ComboBox box;
    std::unique_ptr<APVTS_::ComboBoxAttachment> attach;

    void init (juce::Component& parent, APVTS_& s, const juce::String& id,
               const juce::String& text, const juce::StringArray& items)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredRight);
        box.addItemList (items, 1);
        parent.addAndMakeVisible (label);
        parent.addAndMakeVisible (box);
        attach = std::make_unique<APVTS_::ComboBoxAttachment> (s, id, box);
    }
    void place (juce::Rectangle<int>& row, int labelW, int boxW)
    {
        label.setBounds (row.removeFromLeft (labelW));
        box.setBounds (row.removeFromLeft (boxW));
        row.removeFromLeft (8);
    }
};

struct ToggleBox
{
    juce::ToggleButton button;
    std::unique_ptr<APVTS_::ButtonAttachment> attach;

    void init (juce::Component& parent, APVTS_& s, const juce::String& id, const juce::String& text)
    {
        button.setButtonText (text);
        parent.addAndMakeVisible (button);
        attach = std::make_unique<APVTS_::ButtonAttachment> (s, id, button);
    }
};

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
    SoundPage (AuraProcessor&, int layer);
    void resized() override;
    void loadFiles (const juce::Array<juce::File>& files);

private:
    void timerCallback() override;

    AuraProcessor& proc;
    int layer;
    LabeledCombo source, wave, fmAlgo;
    ToggleBox oneShot;
    juce::TextButton loadButton { "Load samples..." };
    juce::Label statusLabel;
    KnobSet knobs;
    std::unique_ptr<juce::FileChooser> chooser;
};

class ModPage : public juce::Component
{
public:
    ModPage (AuraProcessor&, int layer);
    void resized() override;

private:
    struct Row
    {
        juce::Label num;
        juce::ComboBox src, dst;
        juce::Slider amt;
        std::unique_ptr<APVTS_::ComboBoxAttachment> aS, aD;
        std::unique_ptr<APVTS_::SliderAttachment> aA;
    };
    std::vector<std::unique_ptr<Row>> rows;
    LabeledCombo lfo1Shape, lfo2Shape;
    KnobSet knobs;
};

class MsegPage : public juce::Component
{
public:
    MsegPage (AuraProcessor&, int layer);
    void resized() override;

private:
    AuraProcessor& proc;
    int layer;
    MsegEditor editor[2];
    ToggleBox loop[2];
    KnobSet knobsA, knobsB;
    juce::Label hint;
};

// Grille du sequenceur : 3 lignes (Gate / Velocity / Pitch) x 16 pas, edition a la souris.
class StepSeqEditor : public juce::Component, private juce::Timer
{
public:
    StepSeqEditor (AuraProcessor&, int layer);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragLane = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum Lane { LaneGate = 0, LaneVel, LanePitch, kNumLanes };

    void timerCallback() override;
    juce::Rectangle<float> laneRect (int lane) const;
    int laneAt (juce::Point<float> p) const;
    int stepAt (float x) const;
    float get (int step, int part) const;
    void set (int step, int part, float value);
    void editAt (juce::Point<float> p);

    AuraProcessor& proc;
    int layer;
    juce::RangedAudioParameter* prm[aura::kSeqSteps][3] {};
    juce::RangedAudioParameter* lenParam = nullptr;
    int dragLane = -1, lastStep = -2, tick = 0;
    bool paintOn = true;
};

class SeqPage : public juce::Component
{
public:
    SeqPage (AuraProcessor&, int layer);
    void resized() override;

private:
    ToggleBox on;
    LabeledCombo rate;
    KnobSet knobs;
    StepSeqEditor editor;
    juce::Label hint;
};

class LayerPage : public juce::Component
{
public:
    LayerPage (AuraProcessor&, int layer);
    void resized() override;
    void loadFiles (const juce::Array<juce::File>& f) { sound.loadFiles (f); }

private:
    ToggleBox enable;
    SoundPage sound;
    ModPage mod;
    MsegPage mseg;
    SeqPage seq;
    juce::TabbedComponent inner { juce::TabbedButtonBar::TabsAtTop };
};

class RoutingPage : public juce::Component
{
public:
    explicit RoutingPage (AuraProcessor&);
    void resized() override;

private:
    struct MiniKnob
    {
        juce::Slider s;
        std::unique_ptr<APVTS_::SliderAttachment> a;
    };
    struct Row
    {
        juce::Label name;
        ToggleBox en, arp;
        std::vector<std::unique_ptr<MiniKnob>> knobs;
    };
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<std::unique_ptr<juce::Label>> headers;
};

class FxPage : public juce::Component
{
public:
    explicit FxPage (AuraProcessor&);
    void resized() override;

private:
    struct Group
    {
        juce::Label title;
        KnobSet knobs;
        Group (juce::Component& o, APVTS_& s, const char* text) : knobs (o, s)
        {
            title.setText (text, juce::dontSendNotification);
            title.setFont (juce::FontOptions (16.f, juce::Font::bold));
            title.setColour (juce::Label::textColourId, juce::Colour (0xff4fc3f7));
            o.addAndMakeVisible (title);
        }
    };

    Group sat, cho, dly, rev, lim, mst;
    ToggleBox satOn, dlyPing, limOn;
    LabeledCombo satMode;
};

class ArpPage : public juce::Component
{
public:
    explicit ArpPage (AuraProcessor&);
    void resized() override;

private:
    ToggleBox on, latch;
    LabeledCombo mode, rate;
    KnobSet knobs;
    juce::Label hint;
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
    std::vector<std::unique_ptr<LayerPage>> layerPages;
    RoutingPage routing;
    FxPage fx;
    ArpPage arpPage;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };   // en dernier : detruit en premier
};
