#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "data/BankLoader.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout AuraProcessor::createLayout()
{
    using juce::NormalisableRange;
    using RP = std::unique_ptr<juce::RangedAudioParameter>;
    std::vector<RP> p;

    auto pid = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };
    auto flt = [&] (const char* id, const char* name, NormalisableRange<float> r, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (id), name, r, def));
    };
    auto choice = [&] (const char* id, const char* name, juce::StringArray items, int def)
    {
        p.push_back (std::make_unique<juce::AudioParameterChoice> (pid (id), name, items, def));
    };

    choice ("source", "Source", { "Synth", "Sample", "Synth+Sample" }, 0);
    p.push_back (std::make_unique<juce::AudioParameterBool> (pid ("oneshot"), "One-shot", false));

    choice ("wave", "Waveform", { "Saw", "Square", "Wavetable", "FM" }, 0);
    p.push_back (std::make_unique<juce::AudioParameterInt> (pid ("unison"), "Unison", 1, 7, 5));
    flt ("detune", "Detune", NormalisableRange<float> (0.f, 1.f), 0.25f);
    flt ("wtpos",  "WT Position", NormalisableRange<float> (0.f, 1.f), 0.f);

    choice ("fmalgo", "FM Algorithm", { "2-Op", "3-Op Stack", "3-Op Parallel" }, 0);
    flt ("fmr1",  "FM Ratio 1", NormalisableRange<float> (0.25f, 16.f, 0.f, 0.5f), 2.f);
    flt ("fmr2",  "FM Ratio 2", NormalisableRange<float> (0.25f, 16.f, 0.f, 0.5f), 3.f);
    flt ("fmidx", "FM Index", NormalisableRange<float> (0.f, 12.f, 0.f, 0.6f), 2.f);
    flt ("fmfb",  "FM Feedback", NormalisableRange<float> (0.f, 1.f), 0.f);

    flt ("cutoff", "Cutoff", NormalisableRange<float> (20.f, 20000.f, 0.f, 0.3f), 8000.f);
    flt ("res",    "Resonance", NormalisableRange<float> (0.5f, 10.f, 0.f, 0.5f), 0.8f);

    flt ("atk", "Attack",  NormalisableRange<float> (0.001f, 5.f, 0.f, 0.4f), 0.005f);
    flt ("dec", "Decay",   NormalisableRange<float> (0.001f, 5.f, 0.f, 0.4f), 0.3f);
    flt ("sus", "Sustain", NormalisableRange<float> (0.f, 1.f), 0.7f);
    flt ("rel", "Release", NormalisableRange<float> (0.001f, 8.f, 0.f, 0.4f), 0.3f);

    flt ("matk", "ModEnv Attack",  NormalisableRange<float> (0.001f, 5.f, 0.f, 0.4f), 0.01f);
    flt ("mdec", "ModEnv Decay",   NormalisableRange<float> (0.001f, 5.f, 0.f, 0.4f), 0.4f);
    flt ("msus", "ModEnv Sustain", NormalisableRange<float> (0.f, 1.f), 0.f);
    flt ("mrel", "ModEnv Release", NormalisableRange<float> (0.001f, 8.f, 0.f, 0.4f), 0.3f);

    const juce::StringArray shapes { "Sine", "Triangle", "Saw", "Square", "S&H" };
    flt ("lfo1rate", "LFO1 Rate", NormalisableRange<float> (0.05f, 30.f, 0.f, 0.4f), 2.f);
    choice ("lfo1shape", "LFO1 Shape", shapes, 0);
    flt ("lfo2rate", "LFO2 Rate", NormalisableRange<float> (0.05f, 30.f, 0.f, 0.4f), 0.5f);
    choice ("lfo2shape", "LFO2 Shape", shapes, 0);

    flt ("msegtime", "MSEG Time", NormalisableRange<float> (0.01f, 20.f, 0.f, 0.4f), 1.f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (pid ("msegloop"), "MSEG Loop", false));

    for (int i = 0; i < 4; ++i)
        flt (aura::kMacroId[i], (juce::String ("Macro ") + juce::String (i + 1)).toRawUTF8(),
             NormalisableRange<float> (0.f, 1.f), 0.f);

    for (int i = 0; i < aura::kSlots; ++i)
    {
        const juce::String n (i + 1);
        choice (aura::kSlotSrcId[i], ("Mod " + n + " Source").toRawUTF8(),
                aura::namesToArray (aura::kSrcNames, aura::kNumSrc), 0);
        choice (aura::kSlotDstId[i], ("Mod " + n + " Dest").toRawUTF8(),
                aura::namesToArray (aura::kDstNames, aura::kNumDst), 0);
        flt (aura::kSlotAmtId[i], ("Mod " + n + " Amount").toRawUTF8(),
             NormalisableRange<float> (-1.f, 1.f), 0.f);
    }

    flt ("gain", "Gain", NormalisableRange<float> (0.f, 1.f), 0.5f);

    return { p.begin(), p.end() };
}

AuraProcessor::AuraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    synth.setWavetables (&wavetables);
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

    {
        const juce::SpinLock::ScopedTryLockType lock (bankLock);
        if (lock.isLocked() && bankDirty) { synth.setBank (pendingBank); bankDirty = false; }
    }
    {
        const juce::SpinLock::ScopedTryLockType lock (msegLock);
        if (lock.isLocked() && msegDirty) { synth.setMseg (msegShape); msegDirty = false; }
    }

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    aura::Params p;
    p.source   = int (get ("source") + 0.5f);
    p.oneShot  = get ("oneshot") > 0.5f;
    p.wave     = int (get ("wave") + 0.5f);
    p.unison   = int (get ("unison") + 0.5f);
    p.detune   = get ("detune");
    p.wtPos    = get ("wtpos");
    p.fmAlgo   = int (get ("fmalgo") + 0.5f);
    p.fmRatio1 = get ("fmr1");
    p.fmRatio2 = get ("fmr2");
    p.fmIndex  = get ("fmidx");
    p.fmFeedback = get ("fmfb");
    p.cutoff   = get ("cutoff");
    p.res      = get ("res");
    p.atk = get ("atk"); p.dec = get ("dec"); p.sus = get ("sus"); p.rel = get ("rel");
    p.matk = get ("matk"); p.mdec = get ("mdec"); p.msus = get ("msus"); p.mrel = get ("mrel");
    p.lfoRate[0]  = get ("lfo1rate");  p.lfoShape[0] = int (get ("lfo1shape") + 0.5f);
    p.lfoRate[1]  = get ("lfo2rate");  p.lfoShape[1] = int (get ("lfo2shape") + 0.5f);
    p.msegTime = get ("msegtime");
    p.msegLoop = get ("msegloop") > 0.5f;
    for (int i = 0; i < 4; ++i) p.macro[i] = get (aura::kMacroId[i]);
    for (int i = 0; i < aura::kSlots; ++i)
    {
        p.slots[i].src    = int (get (aura::kSlotSrcId[i]) + 0.5f);
        p.slots[i].dst    = int (get (aura::kSlotDstId[i]) + 0.5f);
        p.slots[i].amount = get (aura::kSlotAmtId[i]);
    }
    p.gain = get ("gain");
    synth.setParams (p);

    if (buffer.getNumChannels() >= 2)
        synth.process (buffer.getWritePointer (0), buffer.getWritePointer (1),
                       buffer.getNumSamples(), midi);
}

aura::MsegShape AuraProcessor::getMseg() const
{
    const juce::SpinLock::ScopedLockType lock (msegLock);
    return msegShape;
}

void AuraProcessor::setMseg (const aura::MsegShape& s)
{
    {
        const juce::SpinLock::ScopedLockType lock (msegLock);
        msegShape = s;
        msegDirty = true;
    }
    apvts.state.setProperty ("mseg", s.toString(), nullptr);
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

            const juce::String ms = apvts.state.getProperty ("mseg").toString();
            setMseg (ms.isNotEmpty() ? aura::MsegShape::fromString (ms) : aura::MsegShape());

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
