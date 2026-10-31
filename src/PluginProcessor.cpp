#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "data/BankLoader.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Affichage compact des valeurs : 0.9, 0.09, 12.5, 350 (pas de zeros inutiles)
static juce::String compactValue (float v, int)
{
    const float a = std::abs (v);
    auto s = juce::String (v, a >= 100.f ? 0 : (a >= 10.f ? 1 : 2));
    if (s.containsChar ('.'))
    {
        s = s.trimCharactersAtEnd ("0");
        if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
    }
    return s == "-0" ? juce::String ("0") : s;
}

APVTS::ParameterLayout AuraProcessor::createLayout()
{
    using juce::NormalisableRange;
    using namespace aura::fx;
    using RP = std::unique_ptr<juce::RangedAudioParameter>;
    std::vector<RP> p;

    auto pid = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };
    auto flt = [&] (const juce::String& id, const juce::String& name, NormalisableRange<float> r, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            pid (id), name, r, def, juce::AudioParameterFloatAttributes().withStringFromValueFunction (compactValue)));
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
                        // couche 1 sortie par defaut sur FX 1 (reverb) : les effets s'entendent tout de suite
                        choice (id, name, juce::StringArray::fromTokens (s.choices, "|", ""),
                                lp == aura::LP_Output ? (l == 0 ? 1 : 0) : (int) s.def);
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

    // ---- chaines FX : 4 groupes + master, 6 emplacements ----
    // Defaut : FX 1 = Reverb, FX 2 = Delay synchro 1/8 D, master vide.
    const auto typeNames = fxTypeNames();
    for (int c = 0; c < kFxChains; ++c)
    {
        const juce::String cn = fxChainName (c) + " ";
        for (int s = 0; s < kFxSlots; ++s)
        {
            int defType = FxNone;
            if (s == 0 && c == 0) defType = FxReverb;
            if (s == 0 && c == 1) defType = FxDelay;
            const juce::String sn = cn + "Slot " + juce::String (s + 1);
            choice (fxTypeId (c, s), sn + " Type", typeNames, defType);
            boolean (fxOnId (c, s), sn + " On", true);
            const auto& def = fxDef (defType);
            for (int i = 0; i < kFxParams; ++i)
            {
                const float d = i < def.numParams ? fxToNorm (def.p[i], def.p[i].def) : 0.f;
                p.push_back (std::make_unique<juce::AudioParameterFloat> (
                    pid (fxParamId (c, s, i)), sn + " P" + juce::String (i + 1), NormalisableRange<float> (0.f, 1.f), d,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (compactValue)));
            }
        }
        if (c < kFxGroups) flt (fxGainId (c), cn + "Level", NormalisableRange<float> (0.f, 1.5f), 1.f);
    }

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
    using namespace aura::fx;
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

    for (int c = 0; c < kFxChains; ++c)
    {
        auto& cp = chainPtrs[c];
        for (int s = 0; s < kFxSlots; ++s)
        {
            cp.type[s] = apvts.getRawParameterValue (fxTypeId (c, s));
            cp.on[s] = apvts.getRawParameterValue (fxOnId (c, s));
            for (int i = 0; i < kFxParams; ++i) cp.p[s][i] = apvts.getRawParameterValue (fxParamId (c, s, i));
        }
        if (c < kFxGroups) cp.gain = apvts.getRawParameterValue (fxGainId (c));
    }

    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    gp.limOn = g ("limOn"); gp.limCeil = g ("limCeil"); gp.limRel = g ("limRel");
    gp.master = g ("master");

    ensureFx();
    startTimerHz (10);
}

AuraProcessor::~AuraProcessor()
{
    stopTimer();
}

void AuraProcessor::ensureFx()
{
    const juce::ScopedLock sl (fxAllocLock);
    const int cap = std::max (bufCap, 16384);
    for (int c = 0; c < aura::fx::kFxChains; ++c)
        chains[c].ensure (chainPtrs[c], currentRate, cap);
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
        l.busPrev = -1;
    }
    globalInit = false;

    bufCap = std::max (samplesPerBlock, 16384);
    layerBuf.setSize (2, bufCap);
    masterBus.setSize (2, bufCap);
    for (auto& b : groupBus) b.setSize (2, bufCap);

    {
        const juce::ScopedLock sl (fxAllocLock);
        currentRate = sampleRate;
        for (auto& c : chains) c.prepareAll (sampleRate, bufCap);
    }
    ensureFx();

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

    // ---- couches -> bus de sortie (Master ou groupe FX 1..4) ----
    using namespace aura::fx;
    masterBus.clear (0, n);
    for (auto& gb : groupBus) gb.clear (0, n);

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
                L.busPrev = -1;
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
        const float g[2] = { level * std::cos (ang) * 1.41421356f, level * std::sin (ang) * 1.41421356f };

        const int out = juce::jlimit (0, kFxGroups, int (P (LP_Output) + 0.5f));   // 0 = master, 1..4 = FX n
        if (out != L.busPrev) { L.gPrev[0] = g[0]; L.gPrev[1] = g[1]; L.busPrev = out; }
        auto& dst = (out == 0) ? masterBus : groupBus[out - 1];
        addRamp (dst.getWritePointer (0), layerBuf.getReadPointer (0), n, L.gPrev[0], g[0]);
        addRamp (dst.getWritePointer (1), layerBuf.getReadPointer (1), n, L.gPrev[1], g[1]);
        L.gPrev[0] = g[0];
        L.gPrev[1] = g[1];
    }

    // ---- groupes FX -> master ----
    FxContext ctx;
    ctx.bpm = bpm;
    const float mg = gp.master->load();
    if (! globalInit)
    {
        for (int c = 0; c < kFxGroups; ++c) groupGainPrev[c] = chainPtrs[c].gain->load();
        masterPrev = mg;
        globalInit = true;
    }
    float* mL = masterBus.getWritePointer (0);
    float* mR = masterBus.getWritePointer (1);
    for (int c = 0; c < kFxGroups; ++c)
    {
        float* gL = groupBus[c].getWritePointer (0);
        float* gR = groupBus[c].getWritePointer (1);
        chains[c].process (gL, gR, n, chainPtrs[c], ctx);       // toujours : les queues de reverb / delay continuent
        const float gg = chainPtrs[c].gain->load();
        addRamp (mL, gL, n, groupGainPrev[c], gg);
        addRamp (mR, gR, n, groupGainPrev[c], gg);
        groupGainPrev[c] = gg;
    }

    // ---- chaine master -> gain -> limiteur ----
    chains[kMasterChain].process (mL, mR, n, chainPtrs[kMasterChain], ctx);
    {
        const float step = (mg - masterPrev) / float (n);
        float gm = masterPrev;
        for (int i = 0; i < n; ++i) { gm += step; mL[i] *= gm; mR[i] *= gm; }
        masterPrev = mg;
    }

    const float ceilLin = std::pow (10.f, gp.limCeil->load() / 20.f);
    limiter.process (mL, mR, n, ceilLin, gp.limRel->load(), gp.limOn->load() > 0.5f);

    std::copy (mL, mL + n, buffer.getWritePointer (0));
    std::copy (mR, mR + n, buffer.getWritePointer (1));
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

// ---------------------------------------------------------------------------
// Noms de couches
// ---------------------------------------------------------------------------
juce::String AuraProcessor::getLayerName (int layer) const
{
    const auto n = apvts.state.getProperty ("L" + juce::String (layer + 1) + "_name").toString();
    return n.isNotEmpty() ? n : "Layer " + juce::String (layer + 1);
}

void AuraProcessor::setLayerName (int layer, const juce::String& name)
{
    apvts.state.setProperty ("L" + juce::String (layer + 1) + "_name", name.trim().substring (0, 32), nullptr);
}

// ---------------------------------------------------------------------------
// Chaines FX : edition (thread message, via les parametres -> automatisable et sauve dans les presets)
// ---------------------------------------------------------------------------
static void setNorm (juce::AudioProcessorValueTreeState& s, const juce::String& id, float norm)
{
    if (auto* p = s.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

static float getNorm (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    auto* p = s.getParameter (id);
    return p != nullptr ? p->getValue() : 0.f;
}

int AuraProcessor::fxType (int chain, int slot) const
{
    return juce::jlimit (0, aura::fx::kNumFxTypes - 1, juce::roundToInt (chainPtrs[chain].type[slot]->load()));
}

void AuraProcessor::setFxDefaults (int chain, int slot, int type)
{
    using namespace aura::fx;
    if (auto* tp = apvts.getParameter (fxTypeId (chain, slot))) setNorm (apvts, fxTypeId (chain, slot), tp->convertTo0to1 ((float) type));
    setNorm (apvts, fxOnId (chain, slot), 1.f);
    const auto& def = fxDef (type);
    for (int i = 0; i < kFxParams; ++i)
        setNorm (apvts, fxParamId (chain, slot, i), i < def.numParams ? fxToNorm (def.p[i], def.p[i].def) : 0.f);
}

void AuraProcessor::copySlot (int chain, int from, int to)
{
    using namespace aura::fx;
    setNorm (apvts, fxTypeId (chain, to), getNorm (apvts, fxTypeId (chain, from)));
    setNorm (apvts, fxOnId (chain, to), getNorm (apvts, fxOnId (chain, from)));
    for (int i = 0; i < kFxParams; ++i)
        setNorm (apvts, fxParamId (chain, to, i), getNorm (apvts, fxParamId (chain, from, i)));
}

int AuraProcessor::addFx (int chain, int type)
{
    using namespace aura::fx;
    for (int s = 0; s < kFxSlots; ++s)
    {
        if (fxType (chain, s) != FxNone) continue;
        setFxDefaults (chain, s, type);
        ensureFx();
        return s;
    }
    return -1;
}

void AuraProcessor::removeFx (int chain, int slot)
{
    using namespace aura::fx;
    for (int s = slot; s < kFxSlots - 1; ++s) copySlot (chain, s + 1, s);
    setFxDefaults (chain, kFxSlots - 1, FxNone);
}

void AuraProcessor::moveFx (int chain, int slot, int delta)
{
    using namespace aura::fx;
    const int other = slot + delta;
    if (other < 0 || other >= kFxSlots || fxType (chain, other) == FxNone) return;
    float t[2 + kFxParams];
    t[0] = getNorm (apvts, fxTypeId (chain, slot));
    t[1] = getNorm (apvts, fxOnId (chain, slot));
    for (int i = 0; i < kFxParams; ++i) t[2 + i] = getNorm (apvts, fxParamId (chain, slot, i));
    copySlot (chain, other, slot);
    setNorm (apvts, fxTypeId (chain, other), t[0]);
    setNorm (apvts, fxOnId (chain, other), t[1]);
    for (int i = 0; i < kFxParams; ++i) setNorm (apvts, fxParamId (chain, other, i), t[2 + i]);
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
    applyState (juce::ValueTree::fromXml (*xml));
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
static const juce::String kSamplesToken ("$SAMPLES/");

static juce::String samplePathsId (int l) { return "L" + juce::String (l + 1) + "_samplePaths"; }

juce::String AuraProcessor::getPresetName() const
{
    const auto n = apvts.state.getProperty ("presetName").toString();
    return n.isNotEmpty() ? n : juce::String ("Init");
}

juce::File AuraProcessor::getPresetFile() const
{
    const auto p = apvts.state.getProperty ("presetFile").toString();
    return p.isNotEmpty() ? juce::File (p) : juce::File();
}

void AuraProcessor::clearSamples (int layer)
{
    auto& L = layers[(size_t) layer];
    {
        const juce::SpinLock::ScopedLockType lock (L.bankLock);
        L.pendingBank = nullptr;        // la banque reste vivante via keepAlive : jamais detruite cote audio
        L.bankDirty = true;
    }
    {
        const juce::ScopedLock sl (L.statusLock);
        L.uiBank = nullptr;
        ++L.uiBankVersion;
    }
    setStatus (layer, "No samples loaded");
}

bool AuraProcessor::savePreset (const juce::File& file, const PresetMeta& meta, bool copySamples, juce::String& error)
{
    using aura::PresetLibrary;
    auto state = apvts.copyState();
    state.setProperty ("presetName", meta.name, nullptr);
    state.removeProperty ("presetFile", nullptr);

    const auto samplesRoot = PresetLibrary::samplesDir();
    juce::StringArray layerInfo;
    for (int l = 0; l < aura::kLayers; ++l)
    {
        if (lpPtr[l][aura::LP_Enable]->load() < 0.5f) continue;
        const int src = juce::roundToInt (lpPtr[l][aura::LP_Source]->load());
        const int wave = juce::roundToInt (lpPtr[l][aura::LP_Wave]->load());
        static const char* const waves[] = { "Saw", "Square", "Wavetable", "FM", "Triangle", "Sine" };
        const juce::String synth (waves[juce::jlimit (0, 5, wave)]);
        layerInfo.add ("L" + juce::String (l + 1) + ":" + (src == 1 ? juce::String ("Sample")
                                                          : (src == 2 ? synth + "+Sample" : synth)));

        const auto saved = state.getProperty (samplePathsId (l)).toString();
        if (saved.isEmpty()) continue;
        juce::StringArray out;
        for (const auto& s : juce::StringArray::fromTokens (saved, "|", ""))
        {
            juce::File f (s);
            if (! f.existsAsFile()) continue;
            if (copySamples && ! f.isAChildOf (samplesRoot))
            {
                const auto dir = samplesRoot.getChildFile (juce::File::createLegalFileName (meta.name))
                                            .getChildFile ("L" + juce::String (l + 1));
                if (! dir.createDirectory()) { error = "Cannot create " + dir.getFullPathName(); return false; }
                const auto dst = dir.getChildFile (f.getFileName());
                if (! dst.existsAsFile() && ! f.copyFileTo (dst)) { error = "Cannot copy " + f.getFileName(); return false; }
                f = dst;
            }
            out.add (f.isAChildOf (samplesRoot)
                         ? kSamplesToken + f.getRelativePathFrom (samplesRoot).replaceCharacter ('\\', '/')
                         : f.getFullPathName());
        }
        state.setProperty (samplePathsId (l), out.joinIntoString ("|"), nullptr);
    }

    juce::XmlElement root ("AuraPreset");
    root.setAttribute ("formatVersion", PresetLibrary::kFormatVersion);
    root.setAttribute ("name", meta.name);
    root.setAttribute ("category", meta.category);
    root.setAttribute ("tags", meta.tags);
    root.setAttribute ("author", meta.author);
    root.setAttribute ("favorite", 0);
    root.setAttribute ("layers", layerInfo.joinIntoString ("  "));
    if (auto xml = state.createXml()) root.addChildElement (xml.release());

    file.getParentDirectory().createDirectory();
    if (! root.writeTo (file)) { error = "Cannot write " + file.getFullPathName(); return false; }

    apvts.state.setProperty ("presetName", meta.name, nullptr);
    apvts.state.setProperty ("presetFile", file.getFullPathName(), nullptr);
    library.rescanAsync();
    return true;
}

bool AuraProcessor::loadPreset (const juce::File& file, juce::String& error)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AuraPreset")) { error = "Not an AURA preset"; return false; }
    if (xml->getIntAttribute ("formatVersion", 1) > aura::PresetLibrary::kFormatVersion)
    {
        error = "Preset made with a newer AURA version";
        return false;
    }
    auto* st = xml->getChildByName (apvts.state.getType());
    if (st == nullptr) { error = "Preset has no state"; return false; }

    auto tree = juce::ValueTree::fromXml (*st);
    tree.setProperty ("presetName", xml->getStringAttribute ("name", file.getFileNameWithoutExtension()), nullptr);
    tree.setProperty ("presetFile", file.getFullPathName(), nullptr);
    applyState (tree);
    return true;
}

void AuraProcessor::applyState (const juce::ValueTree& newState)
{
    apvts.replaceState (newState);

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

        // chemins de samples : absolus ou relatifs a la bibliotheque ($SAMPLES/...)
        juce::Array<juce::File> files;
        const juce::String saved = apvts.state.getProperty (pre + "samplePaths").toString();
        for (const auto& s : juce::StringArray::fromTokens (saved, "|", ""))
        {
            const juce::File f = s.startsWith (kSamplesToken)
                ? aura::PresetLibrary::samplesDir().getChildFile (s.substring (kSamplesToken.length())
                                                                    .replaceCharacter ('/', juce::File::getSeparatorChar()))
                : juce::File (s);
            if (f.existsAsFile()) files.add (f);
        }
        if (! files.isEmpty()) loadSamples (l, files);
        else                   clearSamples (l);
    }
    ensureFx();
    ++stateVersion;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
