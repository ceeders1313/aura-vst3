#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "data/BankLoader.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout AuraProcessor::createLayout()
{
    using juce::NormalisableRange;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        id ("source"), "Source", juce::StringArray { "Synth", "Sample", "Synth+Sample" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (id ("oneshot"), "One-shot", false));
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

    // Recuperation d'une nouvelle banque sans jamais bloquer
    {
        const juce::SpinLock::ScopedTryLockType lock (bankLock);
        if (lock.isLocked() && bankDirty)
        {
            synth.setBank (pendingBank);
            bankDirty = false;
        }
    }

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    aura::Params p;
    p.source  = int (get ("source") + 0.5f);
    p.oneShot = get ("oneshot") > 0.5f;
    p.wave    = int (get ("wave") + 0.5f);
    p.detune  = get ("detune");
    p.cutoff  = get ("cutoff");
    p.res     = get ("res");
    p.atk     = get ("atk");
    p.dec     = get ("dec");
    p.sus     = get ("sus");
    p.rel     = get ("rel");
    p.gain    = get ("gain");
    synth.setParams (p);

    if (buffer.getNumChannels() >= 2)
        synth.process (buffer.getWritePointer (0), buffer.getWritePointer (1),
                       buffer.getNumSamples(), midi);
}

void AuraProcessor::setStatus (const juce::String& s)
{
    const juce::ScopedLock sl (statusLock);
    statusText = s;
}

juce::String AuraProcessor::getStatusText() const
{
    const juce::ScopedLock sl (statusLock);
    return statusText;
}

void AuraProcessor::loadSamples (const juce::Array<juce::File>& files)
{
    if (files.isEmpty()) return;

    juce::StringArray paths;
    for (const auto& f : files) paths.add (f.getFullPathName());
    apvts.state.setProperty ("samplePaths", paths.joinIntoString ("|"), nullptr);

    const juce::String name = (files.size() == 1)
        ? files[0].getFileNameWithoutExtension()
        : files[0].getParentDirectory().getFileName();

    setStatus ("Loading...");

    loaderPool.addJob ([this, files, name]
    {
        juce::String report;
        auto bank = aura::BankLoader::build (files, name, report);
        if (bank == nullptr) { setStatus (report); return; }

        // Libere les anciennes banques que plus personne n'utilise (hors thread audio)
        keepAlive.erase (std::remove_if (keepAlive.begin(), keepAlive.end(),
                         [] (const aura::SampleBank::Ptr& b) { return b->getReferenceCount() == 1; }),
                         keepAlive.end());
        keepAlive.push_back (bank);

        {
            const juce::SpinLock::ScopedLockType lock (bankLock);
            pendingBank = bank;
            bankDirty = true;
        }
        setStatus (name + ": " + report);
    });
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
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            const juce::String saved = apvts.state.getProperty ("samplePaths").toString();
            if (saved.isNotEmpty())
            {
                juce::Array<juce::File> files;
                for (const auto& s : juce::StringArray::fromTokens (saved, "|", ""))
                {
                    juce::File f (s);
                    if (f.existsAsFile()) files.add (f);
                }
                if (! files.isEmpty()) loadSamples (files);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
