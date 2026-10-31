#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "engine/Synth.h"
#include "engine/LayerParams.h"
#include "engine/PatternPlayer.h"
#include "data/PresetLibrary.h"
#include "fx/Effects.h"
#include "fx/FxChain.h"

class AuraProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    AuraProcessor();
    ~AuraProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AURA"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15.0; }   // reverb jusqu'a 12 s + delay

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Thread message uniquement
    void loadSamples (int layer, const juce::Array<juce::File>& files);
    juce::String getStatusText (int layer) const;
    aura::MsegShape getMseg (int layer, int idx) const;
    void setMseg (int layer, int idx, const aura::MsegShape& s);
    juce::String getLayerName (int layer) const;
    void setLayerName (int layer, const juce::String& name);

    // Motifs Arp (k = 0) / Seq (k = 1) de chaque couche
    enum { kPatArp = 0, kPatSeq = 1 };
    aura::Pattern getPattern (int layer, int k) const;
    void setPattern (int layer, int k, const aura::Pattern& p);
    int getPlayStep (int layer, int k) const { return layers[(size_t) layer].player[k & 1].currentStep(); }   // tout thread

    // Chaines FX (thread message) : ajout / retrait / deplacement d'effets
    int addFx (int chain, int type);                 // retourne l'emplacement ou -1 si plein
    void removeFx (int chain, int slot);             // compacte la chaine
    void moveFx (int chain, int slot, int delta);
    int fxType (int chain, int slot) const;
    void ensureFx();                                 // cree les instances d'effets manquantes (appele par timer)

    // Presets (thread message)
    struct PresetMeta { juce::String name, category, tags, author; };
    bool savePreset (const juce::File& file, const PresetMeta& meta, bool copySamples, juce::String& error);
    bool loadPreset (const juce::File& file, juce::String& error);
    juce::String getPresetName() const;
    juce::File getPresetFile() const;
    int getStateVersion() const { return stateVersion.load(); }   // incremente a chaque chargement d'etat
    aura::PresetLibrary library;

    // Pour les visuels de l'UI (thread message)
    const aura::WavetableBank& getWavetables() const { return wavetables; }
    aura::SampleBank::Ptr getUiBank (int layer, int& version) const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    struct LayerRt
    {
        aura::Synth synth;

        juce::SpinLock bankLock;
        aura::SampleBank::Ptr pendingBank;
        bool bankDirty = false;
        std::vector<aura::SampleBank::Ptr> keepAlive;

        mutable juce::SpinLock msegLock;
        aura::MsegShape mseg[2];
        bool msegDirty[2] { false, false };

        mutable juce::CriticalSection statusLock;
        juce::String status { "No samples loaded" };
        aura::SampleBank::Ptr uiBank;     // derniere banque chargee (sous statusLock), pour les visuels
        int uiBankVersion = 0;

        // Arp (0) puis Seq (1) : lecteurs de motif, buffers MIDI, motifs (copie UI sous verrou + copie audio)
        aura::PatternPlayer player[2];
        juce::MidiBuffer playMidi[2];
        mutable juce::SpinLock patLock;
        aura::Pattern pat[2];
        bool patDirty[2] { true, true };
        aura::Pattern patRt[2];

        // gains L/R du bloc precedent (rampe anti-zipper) et bus de sortie precedent
        float gPrev[2] {};
        int busPrev = -1;

        bool wasOn = false;
    };

    void timerCallback() override { ensureFx(); }
    void applyState (const juce::ValueTree& newState);
    void clearSamples (int layer);
    std::atomic<int> stateVersion { 0 };

    void setStatus (int layer, const juce::String& s);
    void fillParams (int layer, aura::Params& p) const;
    void fillPlay (int layer, int k, aura::PlayParams& s) const;
    void copySlot (int chain, int from, int to);
    void setFxDefaults (int chain, int slot, int type);

    struct GlobalPtrs { std::atomic<float> *limOn, *limCeil, *limRel, *master; } gp {};

    std::atomic<float>* lpPtr[aura::kLayers][aura::kNumLP] {};

    aura::WavetableBank wavetables;
    std::array<LayerRt, aura::kLayers> layers;

    // FX : 4 groupes + master
    aura::fx::ChainPtrs chainPtrs[aura::fx::kFxChains];
    aura::fx::FxChain chains[aura::fx::kFxChains];
    juce::CriticalSection fxAllocLock;
    double currentRate = 48000.0;
    float groupGainPrev[aura::fx::kFxGroups] {};

    aura::fx::Limiter limiter;
    float masterPrev = 1.f;
    bool globalInit = false;

    int bufCap = 0;
    juce::AudioBuffer<float> layerBuf, masterBus;
    juce::AudioBuffer<float> groupBus[aura::fx::kFxGroups];

    juce::ThreadPool loaderPool { 1 };   // declare en dernier : detruit en premier

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraProcessor)
};
