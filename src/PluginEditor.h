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
    void setTitle (const juce::String& t) { if (t != title) { title = t; repaint(); } }

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

// Piano roll d'un motif (Arp : lignes = index de note tenue ; Seq : lignes = demi-tons).
class PianoRoll : public juce::Component, private juce::Timer
{
public:
    PianoRoll (AuraProcessor&, int layer, int kind);   // kind : AuraProcessor::kPatArp / kPatSeq
    void refresh();                                     // recharge le motif depuis le processeur
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    enum Drag { DragNone, DragMove, DragResize, DragVel };

    void timerCallback() override;
    void commit();
    int lengthSteps() const;
    int minRow() const { return isArp() ? 0 : -24; }
    int maxRow() const { return isArp() ? 31 : 24; }
    int visibleRows() const { return isArp() ? 16 : 25; }
    bool isArp() const { return kind == 0; }
    juce::Rectangle<float> gridArea() const;
    juce::Rectangle<float> velArea() const;
    float rowH() const;
    float stepW() const;
    float rowY (int row) const;
    int rowAt (float y) const;
    int stepAt (float x) const;
    int noteAt (juce::Point<float> p) const;
    juce::Rectangle<float> noteRect (const aura::PatNote& n) const;
    juce::String rowLabel (int row) const;
    void setVelAt (juce::Point<float> p);

    AuraProcessor& proc;
    int layer, kind;
    aura::Pattern pat;
    std::atomic<float>* lenP;
    int topRow;
    Drag drag = DragNone;
    int dragNote = -1, grabStep = 0, grabRow = 0, lastLen = 1, lastPlay = -2, tick = 0, seenVersion = -1;
};

// ============================================================================
// Page LAYER : sections
// ============================================================================
// Synthe analogique : Saw / Square / Triangle / Sine + unison (Saw+ = Saw avec unison)
class AnalogPanel : public juce::Component, private juce::Timer
{
public:
    AnalogPanel (AuraProcessor&, int layer);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    AuraProcessor& proc;
    int layer;
    juce::TextButton waveBtn[4];
    KnobSet knobs;
    juce::Rectangle<int> viewArea;
    std::atomic<float> *waveP, *uniP, *detP;
};

class WavetablePanel : public juce::Component, private juce::Timer
{
public:
    WavetablePanel (AuraProcessor&, int layer);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    AuraProcessor& proc;
    int layer;
    KnobSet knobs;
    juce::Rectangle<int> viewArea;
    std::atomic<float>* posP;
};

class FmPanel : public juce::Component, private juce::Timer
{
public:
    FmPanel (AuraProcessor&, int layer);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    int layer;
    LabeledCombo algo;
    KnobSet knobs;
    juce::Rectangle<int> viewArea;
    std::atomic<float> *algoP, *r1P, *r2P;
};

class SamplePanel : public juce::Component, private juce::Timer
{
public:
    SamplePanel (AuraProcessor&, int layer);
    void paint (juce::Graphics&) override;
    void resized() override;
    void loadFiles (const juce::Array<juce::File>& files);

private:
    void timerCallback() override;
    void rebuild();

    AuraProcessor& proc;
    int layer;
    ToggleBox oneShot;
    juce::TextButton loadButton { "Load samples..." };
    juce::Label statusLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> viewArea;
    aura::SampleBank::Ptr bank;
    int bankVersion = -1;
    std::vector<float> mins, maxs;
    float loopS = -1.f, loopE = -1.f;
    juce::String info;
};

class OscSection : public Section, private juce::Timer
{
public:
    OscSection (AuraProcessor&, int layer);
    void resized() override;
    void loadFiles (const juce::Array<juce::File>& files) { sample.loadFiles (files); }

private:
    enum Type { TypeAnalog = 0, TypeWavetable, TypeFm, kNumTypes };
    void timerCallback() override;
    int currentSource() const;
    int currentType() const;

    AuraProcessor& proc;
    int layer;
    LabeledCombo source;
    juce::TextButton typeBtn[kNumTypes];
    AnalogPanel analog;
    WavetablePanel wavetable;
    FmPanel fm;
    SamplePanel sample;
    int lastSrc = -1, lastType = -1, lastAnalogWave = 0;
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

class MsegPane : public juce::Component, private juce::Timer
{
public:
    MsegPane (AuraProcessor&, int layer, int idx);
    void resized() override;

private:
    void timerCallback() override;   // recharge la forme apres un chargement de preset
    AuraProcessor& proc;
    int layer, idx, seenVersion = -1;
    MsegEditor editor;
    ToggleBox loop;
    KnobSet knobs;
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
};

// ============================================================================
// Chaines FX : panneau d'un effet + editeur de chaine (ajout / retrait / on-off / ordre)
// ============================================================================
class EqView : public juce::Component, private juce::Timer
{
public:
    EqView (APVTS_&, int chain, int slot);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    std::atomic<float>* p[aura::fx::kFxParams] {};
};

class FxSlotPanel : public juce::Component
{
public:
    FxSlotPanel (AuraProcessor&, int chain, int slot);
    int preferredHeight (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onStructureChanged;

private:
    AuraProcessor& proc;
    int chain, slot, type;
    juce::ToggleButton power;
    std::unique_ptr<APVTS_::ButtonAttachment> powerA;
    juce::TextButton up { juce::CharPointer_UTF8 ("\xe2\x96\xb2") }, down { juce::CharPointer_UTF8 ("\xe2\x96\xbc") }, remove { "X" };
    struct Knob
    {
        juce::Slider s;
        juce::Label l;
        std::unique_ptr<APVTS_::SliderAttachment> a;
    };
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<EqView> eq;
};

class FxChainEditor : public juce::Component, private juce::Timer
{
public:
    FxChainEditor (AuraProcessor&, int chain);
    void setChain (int c);
    int getChain() const { return chain; }
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;          // detecte un changement de structure (preset, automation)
    int computeSignature() const;
    void rebuild();
    void layoutContent();
    void showAddMenu();

    AuraProcessor& proc;
    int chain;
    juce::Label title;
    juce::Slider groupGain;
    juce::Label gainLabel;
    std::unique_ptr<APVTS_::SliderAttachment> groupGainA;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<FxSlotPanel>> panels;
    juce::TextButton addBtn { "+  Add effect" };
    juce::Label emptyHint;
    int signature = -1;
};

// ============================================================================
// Page ROUTING : graphe Couches -> Groupes FX -> Master FX -> OUT + inspecteur
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

// Noeud d'une chaine FX (groupe FX 1..4 ou Master) : liste des effets
class FxNode : public juce::Component, private juce::Timer
{
public:
    FxNode (AuraProcessor&, int chain);
    std::function<void()> onSelect;
    bool selected = false;

    static juce::Colour chainColour (int chain);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    AuraProcessor& proc;
    int chain;
};

class RoutingGraph : public juce::Component, private juce::Timer
{
public:
    static int chainIndex (int c) { return aura::kLayers + c; }   // index de selection d'une chaine

    explicit RoutingGraph (AuraProcessor&);
    std::function<void (int)> onSelect;      // 0..kLayers-1 couches, puis chaines FX (4 groupes + master)
    std::function<void (int)> onEditLayer;
    void setSelected (int idx);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }

    AuraProcessor& proc;
    std::vector<std::unique_ptr<LayerNode>> layerNodes;
    std::vector<std::unique_ptr<FxNode>> fxNodes;
    juce::Rectangle<int> outBox;
};

class RoutingPage : public juce::Component, private juce::Timer
{
public:
    explicit RoutingPage (AuraProcessor&);
    void resized() override;
    void select (int idx);
    std::function<void (int)> onEditLayer;

private:
    void timerCallback() override;          // titres des panneaux = noms de couches
    AuraProcessor& proc;
    RoutingGraph graph;
    std::vector<std::unique_ptr<ParamPanel>> layerPanels;
    FxChainEditor chainEditor;
    int selected = 0;
};

// ============================================================================
// Page FX : chaine selectionnee en grand + sortie (limiteur, master)
// ============================================================================
class FxPage : public juce::Component
{
public:
    explicit FxPage (AuraProcessor&);
    void resized() override;

private:
    void showChain (int c);
    juce::TextButton chainBtn[aura::fx::kFxChains];
    FxChainEditor editor;
    ParamPanel output;
};

// ============================================================================
// Page ARP / SEQ (par couche) : boutons ARP et SEQ, reglages, piano roll en bas, import/export MIDI
// ============================================================================
class SeqArpPage : public juce::Component
{
public:
    SeqArpPage (AuraProcessor&, int layer);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void showEditor (int k);
    void generate();
    void importMidi();
    void exportMidi();
    void readMidi (const juce::File& f, int k);
    void writeMidi (const juce::File& f, int k);
    double stepBeats (int k) const;
    PianoRoll& roll (int k) { return k == 0 ? arpRoll : seqRoll; }

    AuraProcessor& proc;
    int layer;
    juce::TextButton arpOn { "ARP" }, seqOn { "SEQ" };
    std::unique_ptr<APVTS_::ButtonAttachment> arpOnA, seqOnA;
    ParamPanel arpPanel, seqPanel;
    Section patternBox;
    juce::TextButton editTab[2];
    juce::ComboBox genType, genSpan;
    juce::TextButton genBtn { "Generate" }, clearBtn { "Clear" },
                     importBtn { "Import MIDI..." }, exportBtn { "Export MIDI..." };
    juce::Label genLabel;
    PianoRoll arpRoll, seqRoll;
    int editing = 0;
    std::unique_ptr<juce::FileChooser> chooser;
};

// ============================================================================
// Page LIB : browser de presets
// ============================================================================
class BrowserPage : public juce::Component,
                    private juce::ChangeListener,
                    private juce::ListBoxModel,
                    private juce::Timer
{
public:
    explicit BrowserPage (AuraProcessor&);
    ~BrowserPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    void loadRelative (int delta);      // fleches precedent / suivant de la barre du haut
    void focusSave();

private:
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { loadRow (row); }
    void selectedRowsChanged (int) override { updateInfo(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void refilter();
    void setCategory (const juce::String& c);
    void loadRow (int row);
    void updateInfo();
    void doSave();
    void doDelete();
    void toggleFavorite();
    int currentIndex() const;

    AuraProcessor& proc;
    std::vector<aura::PresetInfo> all, shown;
    juce::String category { "All" };

    std::vector<std::unique_ptr<juce::TextButton>> catButtons;
    juce::TextEditor search;
    juce::ListBox list;
    juce::Label info, status;
    juce::TextButton loadBtn { "Load" }, favBtn { "Favorite" }, deleteBtn { "Delete" },
                     rescanBtn { "Rescan" }, folderBtn { "Open folder" };
    bool confirmDelete = false;

    juce::Label saveTitle;
    juce::TextEditor saveName, saveTags, saveAuthor;
    juce::ComboBox saveCat;
    juce::ToggleButton copySamples { "Copy used samples into the AURA library (portable preset)" };
    juce::TextButton saveBtn { "Save preset" };
};

// Barre preset du haut : precedent / nom / suivant / sauver
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (AuraProcessor&);
    std::function<void (int)> onStep;       // -1 / +1
    std::function<void()> onOpen, onSave;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override { if (onOpen) onOpen(); }

private:
    void timerCallback() override;
    AuraProcessor& proc;
    juce::TextButton prev { "<" }, next { ">" }, save { "Save" };
    juce::String name;
};

// ============================================================================
// Colonne des couches (gauche) : on/off, nom, type, Mute, Solo
// ============================================================================
class LayerRow : public juce::Component, private juce::Timer
{
public:
    LayerRow (AuraProcessor&, int layer);
    std::function<void()> onSelect;
    bool selected = false;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); }

private:
    void timerCallback() override;
    AuraProcessor& proc;
    int layer;
    juce::ToggleButton power;
    juce::Label name;                    // double-clic pour renommer
    juce::TextButton mute { "M" }, solo { "S" };
    juce::Slider level;
    std::unique_ptr<APVTS_::ButtonAttachment> powerA, muteA, soloA;
    std::unique_ptr<APVTS_::SliderAttachment> levelA;
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

    enum Page { PageLib = 0, PageLayer, PageSeq, PageRouting, PageFx, kNumPages };
    void showPage (int p);
    void selectLayer (int l);
    void selectRoutingNode (int idx) { routing.select (idx); }

private:

    AuraLook look;   // declare en premier : detruit en dernier
    AuraProcessor& proc;

    juce::TextButton nav[kNumPages];
    PresetBar presetBar;
    BrowserPage browser;
    juce::Slider output;
    juce::Label outputLabel;
    std::unique_ptr<APVTS_::SliderAttachment> outputA;

    std::vector<std::unique_ptr<LayerRow>> rows;
    std::vector<std::unique_ptr<LayerEditor>> layerEditors;
    std::vector<std::unique_ptr<SeqArpPage>> seqPages;
    RoutingPage routing;
    FxPage fxPage;

    int page = PageLayer, currentLayer = 0;
    juce::TooltipWindow tooltips { this, 600 };
};
