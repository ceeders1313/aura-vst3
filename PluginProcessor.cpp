#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout AuraProcessor::createLayout()
{
    using juce::NormalisableRange;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        id ("wave"), "Waveform", juce::StringArray { "Saw", "Square", "Supersaw" }, 2));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("detune"), "Detune", NormalisableRange<float> (0.f, 1.f), 0.25f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("cutoff"), "Cutoff", NormalisableRange<float> (20.f, 20000.f, 0.f, 0.3f), 8000.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("res"), "Resonance", NormalisableRange<float> (0.5f, 10.f, 0.f, 0.5f), 0.8f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("atk"), "Attack", NormalisableRange<float> (0.001f, 5.f, 0.f, 0.4f), 0.005f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("dec"), "Decay", NormalisableRange<float> (0.001f, 5.f, 0.f, 0.4f), 0.3f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("sus"), "Sustain", NormalisableRange<float> (0.f, 1.f), 0.7f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("rel"), "Release", NormalisableRange<float> (0.001f, 8.f, 0.f, 0.4f), 0.3f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        id ("gain"), "Gain", NormalisableRange<float> (0.f, 1.f), 0.5f));

    return { p.begin(), p.end() };
}

AuraProcessor::AuraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

void AuraProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
}

bool AuraProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AuraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    aura::Params p;
    p.wave   = int (get ("wave") + 0.5f);
    p.detune = get ("detune");
    p.cutoff = get ("cutoff");
    p.res    = get ("res");
    p.atk    = get ("atk");
    p.dec    = get ("dec");
    p.sus    = get ("sus");
    p.rel    = get ("rel");
    p.gain   = get ("gain");
    synth.setParams (p);

    if (buffer.getNumChannels() >= 2)
        synth.process (buffer.getWritePointer (0), buffer.getWritePointer (1),
                       buffer.getNumSamples(), midi);
}

juce::AudioProcessorEditor* AuraProcessor::createEditor() { return new AuraEditor (*this); }

void AuraProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void AuraProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
