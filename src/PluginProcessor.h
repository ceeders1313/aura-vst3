#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "engine/Synth.h"
#include "engine/LayerParams.h"
#include "engine/Arp.h"
#include "engine/StepSeq.h"
#include "fx/Effects.h"

class AuraProcessor : public juce::AudioProcessor
{
public:
    AuraProcessor();
    ~AuraProcessor() override = default;

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
    int getSeqStep (int layer) const { return layers[(size_t) layer].seq.currentStep(); }   // tout thread

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

        aura::StepSeq seq;
        juce::MidiBuffer seqMidi;

        // gains du bloc precedent (rampes anti-zipper) : dry L/R, chorus L/R, delay L/R, reverb L/R
        float gPrev[8] {};
        bool gInit = false;

        bool wasOn = false;
    };

    void setStatus (int layer, const juce::String& s);
    void fillParams (int layer, aura::Params& p) const;
    void fillSeq (int layer, aura::SeqParams& s) const;

    struct GlobalPtrs
    {
        std::atomic<float> *satOn, *satMode, *satDrive, *satMix,
                           *choRate, *choDepth, *choRet,
                           *dlyTime, *dlyFb, *dlyDamp, *dlyPing, *dlyRet,
                           *revDecay, *revSize, *revDamp, *revPre, *revRet,
                           *limOn, *limCeil, *limRel, *master,
                           *arpOn, *arpMode, *arpRate, *arpOct, *arpGate, *arpSwing, *arpLatch;
    } gp {};

    std::atomic<float>* lpPtr[aura::kLayers][aura::kNumLP] {};

    aura::WavetableBank wavetables;
    std::array<LayerRt, aura::kLayers> layers;

    aura::Arp arp;
    juce::MidiBuffer arpMidi;

    aura::fx::Saturator saturator;
    aura::fx::Chorus chorus;
    aura::fx::StereoDelay delayFx;
    aura::fx::FdnReverb reverb;
    aura::fx::Limiter limiter;

    float retPrev[3] {}, masterPrev = 1.f;
    bool globalInit = false;

    int bufCap = 0;
    juce::AudioBuffer<float> layerBuf, dryBus, choBus, dlyBus, revBus, fxOut;

    juce::ThreadPool loaderPool { 1 };   // declare en dernier : detruit en premier

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraProcessor)
};
