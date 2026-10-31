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
    auto flt = [&] (const juce::String& id, const juce::String& name, NormalisableRange<float> r, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (id), name, r, def));
    };
    auto choice = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    {
        p.push_back (std::make_unique<juce::AudioParameterChoice> (pid (id), name, items, def));
    };
    auto boolean = [&] (const juce::String& id, const juce::String& name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (pid (id), name, def));
    };

    // ---- parametres par couche ----
    const auto srcItems = aura::namesToArray (aura::kSrcNames, aura::kNumSrc);
    const auto dstItems = aura::namesToArray (aura::kDstNames, aura::kNumDst);

    for (int l = 0; l < aura::kLayers; ++l)
    {
        const juce::String prefix = "L" + juce::String (l + 1) + " ";
        for (int lp = 0; lp < aura::kNumLP; ++lp)
        {
            const juce::String id = aura::lpId (l, lp);
            if (lp < aura::LP_SlotBase)
            {
                const auto& s = aura::kLpSpecs[lp];
                const juce::String name = prefix + s.name;
                switch (s.kind)
                {
                    case aura::Kind::Bool:
                        boolean (id, name, lp == aura::LP_Enable ? (l == 0) : (s.def > 0.5f));
                        break;
                    case aura::Kind::Int:
                        p.push_back (std::make_unique<juce::AudioParameterInt> (
                            pid (id), name, (int) s.mn, (int) s.mx, (int) s.def));
                        break;
                    case aura::Kind::Choice:
                        choice (id, name, juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def);
                        break;
                    case aura::Kind::Float:
                        flt (id, name, NormalisableRange<float> (s.mn, s.mx, 0.f, s.skew), s.def);
                        break;
                }
            }
            else
            {
                const int j = (lp - aura::LP_SlotBase) / 3, t = (lp - aura::LP_SlotBase) % 3;
                const juce::String n = prefix + "Mod " + juce::String (j + 1);
                if (t == 0)      choice (id, n + " Source", srcItems, 0);
                else if (t == 1) choice (id, n + " Dest", dstItems, 0);
                else             flt (id, n + " Amount", NormalisableRange<float> (-1.f, 1.f), 0.f);
            }
        }
    }

    // ---- effets ----
    boolean ("satOn", "Sat On", false);
    choice ("satMode", "Sat Mode", { "Tanh", "Hard Clip", "Asymmetric" }, 0);
    flt ("satDrive", "Sat Drive", NormalisableRange<float> (0.f, 24.f), 6.f);
    flt ("satMix", "Sat Mix", NormalisableRange<float> (0.f, 1.f), 1.f);

    flt ("choRate", "Chorus Rate", NormalisableRange<float> (0.05f, 5.f, 0.f, 0.5f), 0.6f);
    flt ("choDepth", "Chorus Depth", NormalisableRange<float> (0.f, 1.f), 0.5f);
    flt ("choRet", "Chorus Return", NormalisableRange<float> (0.f, 1.f), 0.5f);

    flt ("dlyTime", "Delay Time", NormalisableRange<float> (10.f, 1500.f, 0.f, 0.5f), 350.f);
    flt ("dlyFb", "Delay Feedback", NormalisableRange<float> (0.f, 0.95f), 0.4f);
    flt ("dlyDamp", "Delay Damp", NormalisableRange<float> (0.f, 1.f), 0.4f);
    boolean ("dlyPing", "Delay PingPong", true);
    flt ("dlyRet", "Delay Return", NormalisableRange<float> (0.f, 1.f), 0.35f);

    flt ("revDecay", "Reverb Decay", NormalisableRange<float> (0.3f, 12.f, 0.f, 0.5f), 2.5f);
    flt ("revSize", "Reverb Size", NormalisableRange<float> (0.5f, 2.f), 1.f);
    flt ("revDamp", "Reverb Damp", NormalisableRange<float> (0.f, 1.f), 0.4f);
    flt ("revPre", "Reverb PreDelay", NormalisableRange<float> (0.f, 100.f), 15.f);
    flt ("revRet", "Reverb Return", NormalisableRange<float> (0.f, 1.f), 0.3f);

    boolean ("limOn", "Limiter On", true);
    flt ("limCeil", "Limiter Ceiling", NormalisableRange<float> (-12.f, 0.f), -0.3f);
    flt ("limRel", "Limiter Release", NormalisableRange<float> (10.f, 500.f, 0.f, 0.5f), 100.f);

    flt ("master", "Master", NormalisableRange<float> (0.f, 1.5f), 1.f);

    return { p.begin(), p.end() };
}

AuraProcessor::AuraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int l = 0; l < aura::kLayers; ++l)
    {
        for (int lp = 0; lp < aura::kNumLP; ++lp)
            lpPtr[l][lp] = apvts.getRawParameterValue (aura::lpId (l, lp));
        auto& L = layers[(size_t) l];
        L.synth.setWavetables (&wavetables);
        L.player[kPatArp].setKind (aura::PatternPlayer::Kind::Arp);
        L.player[kPatSeq].setKind (aura::PatternPlayer::Kind::Seq);
        L.pat[kPatArp] = L.patRt[kPatArp] = aura::Pattern::defaultArp();
        L.pat[kPatSeq] = L.patRt[kPatSeq] = aura::Pattern::defaultSeq();
    }

    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    gp.satOn = g ("satOn");   gp.satMode = g ("satMode"); gp.satDrive = g ("satDrive"); gp.satMix = g ("satMix");
    gp.choRate = g ("choRate"); gp.choDepth = g ("choDepth"); gp.choRet = g ("choRet");
    gp.dlyTime = g ("dlyTime"); gp.dlyFb = g ("dlyFb"); gp.dlyDamp = g ("dlyDamp");
    gp.dlyPing = g ("dlyPing"); gp.dlyRet = g ("dlyRet");
    gp.revDecay = g ("revDecay"); gp.revSize = g ("revSize"); gp.revDamp = g ("revDamp");
    gp.revPre = g ("revPre"); gp.revRet = g ("revRet");
    gp.limOn = g ("limOn"); gp.limCeil = g ("limCeil"); gp.limRel = g ("limRel");
    gp.master = g ("master");
}

void AuraProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (auto& l : layers)
    {
        l.synth.prepare (sampleRate);
        for (int k = 0; k < 2; ++k)
        {
            l.player[k].prepare (sampleRate);
            l.playMidi[k].ensureSize (16384);
        }
        l.gInit = false;
    }
    globalInit = false;

    bufCap = std::max (samplesPerBlock, 16384);
    layerBuf.setSize (2, bufCap);
    dryBus.setSize (2, bufCap);
    choBus.setSize (2, bufCap);
    dlyBus.setSize (2, bufCap);
    revBus.setSize (2, bufCap);
    fxOut.setSize (2, bufCap);

    saturator.reset();
    chorus.prepare (sampleRate);
    delayFx.prepare (sampleRate);
    reverb.prepare (sampleRate);
    limiter.prepare (sampleRate);
    setLatencySamples (limiter.latency());
}

bool AuraProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AuraProcessor::fillParams (int l, aura::Params& p) const
{
    using namespace aura;
    auto P = [&] (int id) { return lpPtr[l][id]->load(); };

    p.source   = int (P (LP_Source) + 0.5f);
    p.oneShot  = P (LP_OneShot) > 0.5f;
    p.wave     = int (P (LP_Wave) + 0.5f);
    p.unison   = int (P (LP_Unison) + 0.5f);
    p.detune   = P (LP_Detune);
    p.wtPos    = P (LP_WtPos);
    p.fmAlgo   = int (P (LP_FmAlgo) + 0.5f);
    p.fmRatio1 = P (LP_FmR1);
    p.fmRatio2 = P (LP_FmR2);
    p.fmIndex  = P (LP_FmIdx);
    p.fmFeedback = P (LP_FmFb);
    p.cutoff   = P (LP_Cutoff);
    p.res      = P (LP_Res);
    p.atk = P (LP_Atk);   p.dec = P (LP_Dec);   p.sus = P (LP_Sus);   p.rel = P (LP_Rel);
    p.matk = P (LP_MAtk); p.mdec = P (LP_MDec); p.msus = P (LP_MSus); p.mrel = P (LP_MRel);
    p.lfoRate[0] = P (LP_Lfo1Rate); p.lfoShape[0] = int (P (LP_Lfo1Shape) + 0.5f);
    p.lfoRate[1] = P (LP_Lfo2Rate); p.lfoShape[1] = int (P (LP_Lfo2Shape) + 0.5f);
    p.msegTime[0] = P (LP_Mseg1Time); p.msegLoop[0] = P (LP_Mseg1Loop) > 0.5f;
    p.msegTime[1] = P (LP_Mseg2Time); p.msegLoop[1] = P (LP_Mseg2Loop) > 0.5f;
    for (int i = 0; i < 4; ++i) p.macro[i] = P (LP_Macro1 + i);
    for (int i = 0; i < kSlots; ++i)
    {
        p.slots[i].src    = int (P (lpSlot (i, 0)) + 0.5f);
        p.slots[i].dst    = int (P (lpSlot (i, 1)) + 0.5f);
        p.slots[i].amount = P (lpSlot (i, 2));
    }
    p.keyLo = int (P (LP_KeyLo) + 0.5f);
    p.keyHi = int (P (LP_KeyHi) + 0.5f);
    p.velLo = int (P (LP_VelLo) + 0.5f);
    p.velHi = int (P (LP_VelHi) + 0.5f);
    p.transpose = int (P (LP_Transpose) + (P (LP_Transpose) >= 0.f ? 0.5f : -0.5f));
    p.gain = 1.f;
}

void AuraProcessor::fillPlay (int l, int k, aura::PlayParams& s) const
{
    using namespace aura;
    auto P = [&] (int id) { return lpPtr[l][id]->load(); };

    const bool arp = (k == kPatArp);
    s.on     = P (arp ? LP_Arp : LP_SeqOn) > 0.5f;
    s.rate   = int (P (arp ? LP_ArpRate : LP_SeqRate) + 0.5f);
    s.length = int (P (arp ? LP_ArpLen : LP_SeqLen) + 0.5f);
    s.gate   = P (arp ? LP_ArpGate : LP_SeqGate);
    s.swing  = P (arp ? LP_ArpSwing : LP_SeqSwing);
    s.latch  = arp && P (LP_ArpLatch) > 0.5f;
    s.keyLo  = int (P (LP_KeyLo) + 0.5f);
    s.keyHi  = int (P (LP_KeyHi) + 0.5f);
    s.velLo  = int (P (LP_VelLo) + 0.5f);
    s.velHi  = int (P (LP_VelHi) + 0.5f);
}

static void addScaled (float* dst, const float* src, int n, float g)
{
    for (int i = 0; i < n; ++i) dst[i] += src[i] * g;
}

// Ajout avec rampe lineaire de gain sur le bloc (anti-zipper)
static void addRamp (float* dst, const float* src, int n, float g0, float g1)
{
    if (g0 == g1)
    {
        if (g1 != 0.f) addScaled (dst, src, n, g1);
        return;
    }
    const float step = (g1 - g0) / float (n);
    float g = g0;
    for (int i = 0; i < n; ++i) { g += step; dst[i] += src[i] * g; }
}

void AuraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    using namespace aura;
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int n = buffer.getNumSamples();
    if (n <= 0 || n > bufCap || buffer.getNumChannels() < 2) return;

    // ---- transport + arpegiateur ----
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();
        if (pos.hasValue())
        {
            bpm = pos->getBpm().orFallback (120.0);
            playing = pos->getIsPlaying();
            auto q = pos->getPpqPosition();
            if (q.hasValue()) { ppq = *q; havePpq = true; }
        }
    }

    // ---- couches ----
    dryBus.clear(); choBus.clear(); dlyBus.clear(); revBus.clear();

    bool anySolo = false;
    for (int l = 0; l < kLayers; ++l)
        if (lpPtr[l][LP_Enable]->load() > 0.5f && lpPtr[l][LP_Solo]->load() > 0.5f) anySolo = true;

    for (int l = 0; l < kLayers; ++l)
    {
        auto& L = layers[(size_t) l];
        auto P = [&] (int id) { return lpPtr[l][id]->load(); };

        if (P (LP_Enable) < 0.5f)
        {
            if (L.wasOn)
            {
                L.synth.killAll();
                L.player[0].reset();
                L.player[1].reset();
                L.wasOn = false;
                L.gInit = false;
            }
            continue;
        }
        L.wasOn = true;

        {
            const juce::SpinLock::ScopedTryLockType lock (L.bankLock);
            if (lock.isLocked() && L.bankDirty) { L.synth.setBank (L.pendingBank); L.bankDirty = false; }
        }
        {
            const juce::SpinLock::ScopedTryLockType lock (L.msegLock);
            if (lock.isLocked())
                for (int k = 0; k < 2; ++k)
                    if (L.msegDirty[k]) { L.synth.setMseg (k, L.mseg[k]); L.msegDirty[k] = false; }
        }

        {
            const juce::SpinLock::ScopedTryLockType lock (L.patLock);
            if (lock.isLocked())
                for (int k = 0; k < 2; ++k)
                    if (L.patDirty[k]) { L.patRt[k] = L.pat[k]; L.patDirty[k] = false; }
        }

        // MIDI : clavier -> Arp (si actif) -> Seq (si actif) -> synthe.
        // Le premier etage actif applique la zone touches / velocite de la couche.
        const juce::MidiBuffer* cur = &midi;
        bool anyPlayer = false;
        for (int k = 0; k < 2; ++k)
        {
            PlayParams pp;
            fillPlay (l, k, pp);
            pp.filterZone = ! anyPlayer;
            L.playMidi[k].clear();
            if (L.player[k].process (*cur, L.playMidi[k], n, pp, L.patRt[k], bpm, playing, ppq, havePpq))
                cur = &L.playMidi[k];
            anyPlayer = anyPlayer || pp.on;
        }

        Params prm;
        fillParams (l, prm);
        prm.zoneBypass = anyPlayer;
        L.synth.setParams (prm);

        layerBuf.clear (0, n);
        L.synth.process (layerBuf.getWritePointer (0), layerBuf.getWritePointer (1), n, *cur);

        // mute / solo : la couche continue de jouer (timing seq/arp conserve), seul le gain tombe a 0 (rampe)
        const bool audible = P (LP_Mute) < 0.5f && (! anySolo || P (LP_Solo) > 0.5f);
        const float level = audible ? P (LP_Level) : 0.f;
        const float pan = juce::jlimit (-1.f, 1.f, P (LP_Pan));
        const float ang = (pan + 1.f) * 0.25f * 3.14159265f;
        const float cl = std::cos (ang) * 1.41421356f, cr = std::sin (ang) * 1.41421356f;
        const float sCho = P (LP_SendCho) * 0.7f, sDly = P (LP_SendDly) * 0.7f, sRev = P (LP_SendRev) * 0.7f;

        const float g[8] = { level * cl, level * cr,
                             level * sCho * cl, level * sCho * cr,
                             level * sDly * cl, level * sDly * cr,
                             level * sRev * cl, level * sRev * cr };
        if (! L.gInit) { std::copy (g, g + 8, L.gPrev); L.gInit = true; }

        const float* sl = layerBuf.getReadPointer (0);
        const float* sr = layerBuf.getReadPointer (1);
        juce::AudioBuffer<float>* bus[4] = { &dryBus, &choBus, &dlyBus, &revBus };
        for (int b = 0; b < 4; ++b)
        {
            addRamp (bus[b]->getWritePointer (0), sl, n, L.gPrev[2 * b],     g[2 * b]);
            addRamp (bus[b]->getWritePointer (1), sr, n, L.gPrev[2 * b + 1], g[2 * b + 1]);
        }
        std::copy (g, g + 8, L.gPrev);
    }

    float* dL = dryBus.getWritePointer (0);
    float* dR = dryBus.getWritePointer (1);
    float* fL = fxOut.getWritePointer (0);
    float* fR = fxOut.getWritePointer (1);

    // ---- bus d'effets (retours avec rampe) ----
    const float ret[3] = { gp.choRet->load(), gp.dlyRet->load(), gp.revRet->load() };
    const float mg = gp.master->load();
    if (! globalInit) { std::copy (ret, ret + 3, retPrev); masterPrev = mg; globalInit = true; }

    chorus.process (choBus.getReadPointer (0), choBus.getReadPointer (1), fL, fR, n,
                    gp.choRate->load(), gp.choDepth->load());
    addRamp (dL, fL, n, retPrev[0], ret[0]); addRamp (dR, fR, n, retPrev[0], ret[0]);

    delayFx.process (dlyBus.getReadPointer (0), dlyBus.getReadPointer (1), fL, fR, n,
                     gp.dlyTime->load(), gp.dlyFb->load(), gp.dlyDamp->load(), gp.dlyPing->load() > 0.5f);
    addRamp (dL, fL, n, retPrev[1], ret[1]); addRamp (dR, fR, n, retPrev[1], ret[1]);

    reverb.process (revBus.getReadPointer (0), revBus.getReadPointer (1), fL, fR, n,
                    gp.revDecay->load(), gp.revSize->load(), gp.revDamp->load(), gp.revPre->load());
    addRamp (dL, fL, n, retPrev[2], ret[2]); addRamp (dR, fR, n, retPrev[2], ret[2]);
    std::copy (ret, ret + 3, retPrev);

    // ---- master : saturation -> gain -> limiteur ----
    if (gp.satOn->load() > 0.5f)
        saturator.process (dL, dR, n, int (gp.satMode->load() + 0.5f), gp.satDrive->load(), gp.satMix->load());

    {
        const float step = (mg - masterPrev) / float (n);
        float gm = masterPrev;
        for (int i = 0; i < n; ++i) { gm += step; dL[i] *= gm; dR[i] *= gm; }
        masterPrev = mg;
    }

    const float ceilLin = std::pow (10.f, gp.limCeil->load() / 20.f);
    limiter.process (dL, dR, n, ceilLin, gp.limRel->load(), gp.limOn->load() > 0.5f);

    std::copy (dL, dL + n, buffer.getWritePointer (0));
    std::copy (dR, dR + n, buffer.getWritePointer (1));
}

aura::MsegShape AuraProcessor::getMseg (int layer, int idx) const
{
    const auto& L = layers[(size_t) layer];
    const juce::SpinLock::ScopedLockType lock (L.msegLock);
    return L.mseg[idx & 1];
}

void AuraProcessor::setMseg (int layer, int idx, const aura::MsegShape& s)
{
    auto& L = layers[(size_t) layer];
    {
        const juce::SpinLock::ScopedLockType lock (L.msegLock);
        L.mseg[idx & 1] = s;
        L.msegDirty[idx & 1] = true;
    }
    apvts.state.setProperty ("L" + juce::String (layer + 1) + "_mseg" + juce::String ((idx & 1) + 1),
                             s.toString(), nullptr);
}

void AuraProcessor::setStatus (int layer, const juce::String& s)
{
    auto& L = layers[(size_t) layer];
    const juce::ScopedLock sl (L.statusLock);
    L.status = s;
}

juce::String AuraProcessor::getStatusText (int layer) const
{
    const auto& L = layers[(size_t) layer];
    const juce::ScopedLock sl (L.statusLock);
    return L.status;
}

static juce::String patPropId (int layer, int k)
{
    return "L" + juce::String (layer + 1) + (k == 0 ? "_arpPat" : "_seqPat");
}

aura::Pattern AuraProcessor::getPattern (int layer, int k) const
{
    const auto& L = layers[(size_t) layer];
    const juce::SpinLock::ScopedLockType lock (L.patLock);
    return L.pat[k & 1];
}

void AuraProcessor::setPattern (int layer, int k, const aura::Pattern& p)
{
    auto& L = layers[(size_t) layer];
    {
        const juce::SpinLock::ScopedLockType lock (L.patLock);
        L.pat[k & 1] = p;
        L.patDirty[k & 1] = true;
    }
    apvts.state.setProperty (patPropId (layer, k & 1), p.toString(), nullptr);
}

aura::SampleBank::Ptr AuraProcessor::getUiBank (int layer, int& version) const
{
    const auto& L = layers[(size_t) layer];
    const juce::ScopedLock sl (L.statusLock);
    version = L.uiBankVersion;
    return L.uiBank;
}

void AuraProcessor::loadSamples (int layer, const juce::Array<juce::File>& files)
{
    if (files.isEmpty()) return;

    juce::StringArray paths;
    for (const auto& f : files) paths.add (f.getFullPathName());
    apvts.state.setProperty ("L" + juce::String (layer + 1) + "_samplePaths",
                             paths.joinIntoString ("|"), nullptr);

    const juce::String name = (files.size() == 1)
        ? files[0].getFileNameWithoutExtension()
        : files[0].getParentDirectory().getFileName();

    setStatus (layer, "Loading...");

    loaderPool.addJob ([this, layer, files, name]
    {
        auto& L = layers[(size_t) layer];
        juce::String report;
        auto bank = aura::BankLoader::build (files, name, report);
        if (bank == nullptr) { setStatus (layer, report); return; }

        L.keepAlive.erase (std::remove_if (L.keepAlive.begin(), L.keepAlive.end(),
                           [] (const aura::SampleBank::Ptr& b) { return b->getReferenceCount() == 1; }),
                           L.keepAlive.end());
        L.keepAlive.push_back (bank);

        {
            const juce::SpinLock::ScopedLockType lock (L.bankLock);
            L.pendingBank = bank;
            L.bankDirty = true;
        }
        {
            const juce::ScopedLock sl (L.statusLock);
            L.uiBank = bank;
            ++L.uiBankVersion;
        }
        setStatus (layer, name + ": " + report);
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
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    for (int l = 0; l < aura::kLayers; ++l)
    {
        const juce::String pre = "L" + juce::String (l + 1) + "_";
        for (int k = 0; k < 2; ++k)
        {
            const juce::String ms = apvts.state.getProperty (pre + "mseg" + juce::String (k + 1)).toString();
            setMseg (l, k, ms.isNotEmpty() ? aura::MsegShape::fromString (ms) : aura::MsegShape());
        }
        for (int k = 0; k < 2; ++k)
        {
            const auto def = (k == kPatArp) ? aura::Pattern::defaultArp() : aura::Pattern::defaultSeq();
            const juce::String ps = apvts.state.getProperty (patPropId (l, k)).toString();
            setPattern (l, k, ps.isNotEmpty() ? aura::Pattern::fromString (ps, def) : def);
        }

        const juce::String saved = apvts.state.getProperty (pre + "samplePaths").toString();
        if (saved.isNotEmpty())
        {
            juce::Array<juce::File> files;
            for (const auto& s : juce::StringArray::fromTokens (saved, "|", ""))
            {
                juce::File f (s);
                if (f.existsAsFile()) files.add (f);
            }
            if (! files.isEmpty()) loadSamples (l, files);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
