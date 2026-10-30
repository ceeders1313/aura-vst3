#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Synth.h"

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Thread message uniquement
    void loadSamples (const juce::Array<juce::File>& files);
    juce::String getStatusText() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    void setStatus (const juce::String& s);

    aura::Synth synth;

    // Publication lock-free cote audio (try-lock) d'une nouvelle banque
    juce::SpinLock bankLock;
    aura::SampleBank::Ptr pendingBank;
    bool bankDirty = false;

    // Garde les banques en vie : leur destruction ne se fait jamais sur le thread audio
    std::vector<aura::SampleBank::Ptr> keepAlive;

    mutable juce::CriticalSection statusLock;
    juce::String statusText { "No samples loaded" };

    juce::ThreadPool loaderPool { 1 };   // declare en dernier : detruit en premier

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraProcessor)
};
