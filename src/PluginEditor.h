#pragma once
#include "PluginProcessor.h"
#include <functional>
#include <memory>
#include <vector>

using APVTS_ = juce::AudioProcessorValueTreeState;

// ============================================================================
// Theme
// ============================================================================
namespace ui
{
const juce::Colour bg      (0xff111318);
const juce::Colour panel   (0xff191c23);
const juce::Colour panel2  (0xff20242d);
const juce::Colour outline (0xff2b303b);
const juce::Colour text    (0xffc9ced8);
const juce::Colour dim     (0xff7d8494);
const juce::Colour accent  (0xff4fc3f7);

inline juce::Colour layerColour (int l)
{
    static const juce::uint32 c[] = { 0xff4fc3f7, 0xff81c784, 0xffffb74d, 0xffe57399, 0xffba68c8, 0xff4db6ac };
    return juce::Colour (c[(size_t) l % 6]);
}

juce::String layerTypeText (APVTS_& s, int layer);   // "SAW", "WT", "FM", "SMP", "SAW+SMP"...
}

class AuraLook : public juce::LookAndFeel_V4
{
public:
    AuraLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// ============================================================================
// Helpers
// ============================================================================
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

// Panneau a titre (fond arrondi). Les controles sont des enfants places dans content().
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& t, juce::Colour c = ui::accent) : title (t), colour (c) {}
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (8).withTrimmedTop (22); }

protected:
    juce::String title;
    juce::Colour colour;
};

// Panneau generique de parametres (inspecteur du routage)
class ParamPanel : public Section
{
public:
    ParamPanel (APVTS_& s, const juce::String& t, juce::Colour c, int knobCols = 3);
    void addKnob (const juce::String& id, const juce::String& text) { knobs.add (id, text); ++numKnobs; }
    void addToggle (const juce::String& id, const juce::String& text);
    void addCombo (const juce::String& id, const juce::String& text, const juce::StringArray& items);
    void setAction (const juce::String& text, std::function<void()> fn);
    void resized() override;

private:
    APVTS_& apvts;
    KnobSet knobs;
    int numKnobs = 0, cols;
    std::vector<std::unique_ptr<ToggleBox>> toggles;
    std::vector<std::unique_ptr<LabeledCombo>> combos;
    juce::TextButton action;
    std::function<void()> actionFn;
};

// ============================================================================
// Visualisations
// ============================================================================
class FilterView : public juce::Component, private juce::Timer
{
public:
    FilterView (APVTS_&, int layer, juce::Colour);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    std::atomic<float>* cut;
    std::atomic<float>* res;
    juce::Colour colour;
};

class EnvView : public juce::Component, private juce::Timer
{
public:
    EnvView (APVTS_&, int layer, int lpAtk, juce::Colour);   // atk, dec, sus, rel consecutifs
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    std::atomic<float>* p[4];
    juce::Colour colour;
};

class LfoView : public juce::Component, private juce::Timer
{
public:
    LfoView (APVTS_&, int layer, int lpShape, juce::Colour);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    std::atomic<float>* shape;
    juce::Colour colour;
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

// ============================================================================
// Page LAYER : sections
// ============================================================================
class OscSection : public Section, private juce::Timer
{
public:
    OscSection (AuraProcessor&, int layer);
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

class FilterSection : public Section
{
public:
    FilterSection (AuraProcessor&, int layer);
    void resized() override;

private:
    FilterView view;
    KnobSet knobs;
};

class AmpSection : public Section
{
public:
    AmpSection (AuraProcessor&, int layer);
    void resized() override;

private:
    EnvView view;
    KnobSet knobs;
};

class EnvPane : public juce::Component
{
public:
    EnvPane (AuraProcessor&, int layer);
    void resized() override;

private:
    EnvView view;
    KnobSet knobs;
};

class LfoPane : public juce::Component
{
public:
    LfoPane (AuraProcessor&, int layer, int idx);
    void resized() override;

private:
    LfoView view;
    LabeledCombo shape;
    KnobSet knobs;
};

class MsegPane : public juce::Component
{
public:
    MsegPane (AuraProcessor&, int layer, int idx);
    void resized() override;

private:
    MsegEditor editor;
    ToggleBox loop;
    KnobSet knobs;
    juce::Label hint;
};

class SeqPane : public juce::Component
{
public:
    SeqPane (AuraProcessor&, int layer);
    void resized() override;

private:
    ToggleBox on;
    LabeledCombo rate;
    KnobSet knobs;
    StepSeqEditor editor;
    juce::Label hint;
};

class ModSourcesSection : public Section
{
public:
    ModSourcesSection (AuraProcessor&, int layer);
    void resized() override;

private:
    EnvPane env;
    LfoPane lfo1, lfo2;
    MsegPane mseg1, mseg2;
    SeqPane seq;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };   // en dernier
};

class MatrixSection : public Section
{
public:
    MatrixSection (AuraProcessor&, int layer);
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
    juce::Label hSrc, hDst, hAmt;
};

class ZoneSection : public Section
{
public:
    ZoneSection (AuraProcessor&, int layer);
    void resized() override;

private:
    ToggleBox arp;
    KnobSet knobs;
};

class LayerEditor : public juce::Component
{
public:
    LayerEditor (AuraProcessor&, int layer);
    void resized() override;
    void loadFiles (const juce::Array<juce::File>& f) { osc.loadFiles (f); }

private:
    OscSection osc;
    FilterSection filter;
    AmpSection amp;
    ModSourcesSection mods;
    MatrixSection matrix;
    ZoneSection zone;
};

// ============================================================================
// Page ROUTING : graphe de noeuds + inspecteur
// ============================================================================
class LayerNode : public juce::Component, private juce::Timer
{
public:
    LayerNode (AuraProcessor&, int layer);
    std::function<void()> onSelect, onEdit;
    bool selected = false;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onEdit) onEdit(); }

private:
    void timerCallback() override { if (isShowing()) repaint(); }

    AuraProcessor& proc;
    int layer;
    juce::ToggleButton power;
    juce::Slider vol, pan;
    std::unique_ptr<APVTS_::ButtonAttachment> powerA;
    std::unique_ptr<APVTS_::SliderAttachment> volA, panA;
    std::atomic<float> *enP, *arpP, *seqP;
};

class FxNode : public juce::Component
{
public:
    FxNode (APVTS_&, const juce::String& title, const juce::String& retId, juce::Colour);
    std::function<void()> onSelect;
    bool selected = false;
    juce::Colour colour;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); }

private:
    juce::String title;
    juce::Slider ret;
    std::unique_ptr<APVTS_::SliderAttachment> retA;
};

class MasterNode : public juce::Component, private juce::Timer
{
public:
    explicit MasterNode (APVTS_&);
    std::function<void()> onSelect;
    bool selected = false;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    juce::Slider gain;
    std::unique_ptr<APVTS_::SliderAttachment> gainA;
    std::atomic<float> *satOn, *limOn;
};

class RoutingGraph : public juce::Component, private juce::Timer
{
public:
    static constexpr int kFx = 3;
    static int masterIndex() { return aura::kLayers + kFx; }   // index de selection du master

    explicit RoutingGraph (AuraProcessor&);
    std::function<void (int)> onSelect;      // 0..kLayers-1 couches, puis FX, puis master
    std::function<void (int)> onEditLayer;
    void setSelected (int idx);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }

    AuraProcessor& proc;
    std::vector<std::unique_ptr<LayerNode>> layerNodes;
    std::vector<std::unique_ptr<FxNode>> fxNodes;
    MasterNode master;
    juce::Rectangle<int> outBox;
};

class RoutingPage : public juce::Component
{
public:
    explicit RoutingPage (AuraProcessor&);
    void resized() override;
    void select (int idx);
    std::function<void (int)> onEditLayer;

private:
    RoutingGraph graph;
    std::vector<std::unique_ptr<ParamPanel>> panels;
    int selected = 0;
};

// ============================================================================
// Page ARP
// ============================================================================
class ArpPage : public Section
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

// ============================================================================
// Bandeau des couches
// ============================================================================
class LayerTab : public juce::Component, private juce::Timer
{
public:
    LayerTab (AuraProcessor&, int layer);
    std::function<void()> onSelect;
    bool selected = false;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    AuraProcessor& proc;
    int layer;
    juce::ToggleButton power;
    std::unique_ptr<APVTS_::ButtonAttachment> powerA;
};

// ============================================================================
// Editeur principal
// ============================================================================
class AuraEditor : public juce::AudioProcessorEditor,
                   public juce::FileDragAndDropTarget
{
public:
    explicit AuraEditor (AuraProcessor&);
    ~AuraEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    enum Page { PageLayer = 0, PageRouting, PageArp, kNumPages };
    void showPage (int p);
    void selectLayer (int l);

    AuraLook look;   // declare en premier : detruit en dernier
    AuraProcessor& proc;

    juce::TextButton nav[kNumPages];
    juce::Slider output;
    juce::Label outputLabel;
    std::unique_ptr<APVTS_::SliderAttachment> outputA;

    std::vector<std::unique_ptr<LayerTab>> tabs;
    std::vector<std::unique_ptr<LayerEditor>> layerEditors;
    RoutingPage routing;
    ArpPage arpPage;

    int page = PageLayer, currentLayer = 0;
};
