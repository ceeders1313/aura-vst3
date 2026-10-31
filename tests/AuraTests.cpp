// Tests automatiques d'AURA (hors DAW) : rendu audio reel du processeur + captures de l'interface.
// Build local : cmake -S . -B build-tests -DAURA_BUILD_TESTS=ON && cmake --build build-tests --config Release --target AuraTests
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <functional>
#include <vector>

using namespace aura;
using namespace aura::fx;

static int failures = 0, passes = 0;

static void check (bool ok, const juce::String& name, const juce::String& detail = {})
{
    std::printf ("[%s] %s %s\n", ok ? "PASS" : "FAIL", name.toRawUTF8(), detail.toRawUTF8());
    (ok ? passes : failures)++;
}

struct TestPlayHead : public juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        return p;
    }
};

static void setReal (AuraProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void setFxParam (AuraProcessor& p, int c, int s, int i, float real)
{
    const auto& def = fxDef (p.fxType (c, s));
    p.apvts.getParameter (fxParamId (c, s, i))->setValueNotifyingHost (fxToNorm (def.p[i], real));
}

struct Ev { double t; juce::MidiMessage m; };

// Rend 'seconds' de son ; retourne le canal gauche + droit moyenne
static std::vector<float> render (AuraProcessor& p, double seconds, const std::vector<Ev>& evs,
                                  TestPlayHead* ph = nullptr, double fs = 48000.0, int block = 512)
{
    const int total = int (seconds * fs);
    std::vector<float> out;
    out.reserve ((size_t) total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    int pos = 0;
    while (pos < total)
    {
        const int n = std::min (block, total - pos);
        buf.setSize (2, n, false, false, true);
        midi.clear();
        for (const auto& e : evs)
        {
            const int s = int (e.t * fs);
            if (s >= pos && s < pos + n) midi.addEvent (e.m, s - pos);
        }
        p.processBlock (buf, midi);
        for (int i = 0; i < n; ++i) out.push_back (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
        if (ph != nullptr) ph->ppq += double (n) / fs * ph->bpm / 60.0;
        pos += n;
    }
    return out;
}

static double rms (const std::vector<float>& v, double t0, double t1, double fs = 48000.0)
{
    const size_t a = (size_t) (t0 * fs), b = std::min (v.size(), (size_t) (t1 * fs));
    if (b <= a) return 0.0;
    double s = 0.0;
    for (size_t i = a; i < b; ++i) s += double (v[i]) * double (v[i]);
    return std::sqrt (s / double (b - a));
}

static bool finite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}

static std::unique_ptr<AuraProcessor> makeProc()
{
    auto p = std::make_unique<AuraProcessor>();
    p->prepareToPlay (48000.0, 512);
    p->ensureFx();
    return p;
}

static std::vector<Ev> shortNote (double len, int note = 60)
{
    return { { 0.0, juce::MidiMessage::noteOn (1, note, (juce::uint8) 110) },
             { len, juce::MidiMessage::noteOff (1, note) } };
}

// --------------------------------------------------------------------------
static void testBasicSound()
{
    auto p = makeProc();
    setReal (*p, lpId (0, LP_Output), 0);                 // sec
    auto out = render (*p, 0.6, shortNote (0.5));
    const double r = rms (out, 0.1, 0.4);
    check (r > 0.02, "Synth produit du son", "rms=" + juce::String (r, 4));
    check (finite (out), "Pas de NaN / inf");
}

static void testReverbAndDelay()
{
    // reference seche : couche 1 -> Master, release courte
    auto tail = [] (int output, double t0, double t1, double noteLen)
    {
        auto p = makeProc();
        setReal (*p, lpId (0, LP_Rel), 0.02f);
        setReal (*p, lpId (0, LP_Output), (float) output);
        auto out = render (*p, t1 + 0.1, shortNote (noteLen));
        return rms (out, t0, t1);
    };
    const double dry = tail (0, 0.8, 1.6, 0.3);
    const double rev = tail (1, 0.8, 1.6, 0.3);                  // FX 1 = Reverb par defaut
    check (rev > 1e-3 && rev > 20.0 * std::max (dry, 1e-7), "Reverb audible (queue apres la note)",
           "dry=" + juce::String (dry, 6) + " reverb=" + juce::String (rev, 5));

    const double dryD = tail (0, 0.36, 0.42, 0.05);
    const double dly = tail (2, 0.36, 0.42, 0.05);               // FX 2 = Delay 1/8 D = 375 ms a 120 BPM
    check (dly > 1e-3 && dly > 20.0 * std::max (dryD, 1e-7), "Delay audible (echo a 375 ms)",
           "dry=" + juce::String (dryD, 6) + " delay=" + juce::String (dly, 5));
}

static void testEveryEffectOnMaster()
{
    for (int t = 1; t < kNumFxTypes; ++t)
    {
        auto p = makeProc();
        setReal (*p, lpId (0, LP_Output), 0);
        auto ref = render (*p, 0.5, shortNote (0.4));
        auto p2 = makeProc();
        setReal (*p2, lpId (0, LP_Output), 0);
        const int s = p2->addFx (kMasterChain, t);
        // reglages franchement audibles
        if (t == FxEq6) setFxParam (*p2, kMasterChain, s, 6, 12.f);
        if (t == FxFilter) setFxParam (*p2, kMasterChain, s, 1, 500.f);
        if (t == FxCut) setFxParam (*p2, kMasterChain, s, 0, 1000.f);
        if (t == FxComp) setFxParam (*p2, kMasterChain, s, 0, -40.f);
        if (t == FxUtility) setFxParam (*p2, kMasterChain, s, 0, -12.f);
        auto out = render (*p2, 0.5, shortNote (0.4));
        double diff = 0.0;
        for (size_t i = 0; i < out.size(); ++i) diff += std::abs (double (out[i]) - double (ref[i]));
        diff /= double (out.size());
        const double level = rms (out, 0.05, 0.4);
        check (s >= 0 && finite (out) && level > 1e-4 && diff > 1e-4,
               juce::String ("Effet ") + fxDef (t).name + " actif et stable",
               "rms=" + juce::String (level, 4) + " diff=" + juce::String (diff, 5));
    }
}

static void testEqAndFilterResponse()
{
    const double fs = 48000.0;
    auto sine = [&] (float f, int n) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = 0.1f * std::sin (6.2831853f * f * float (i) / float (fs)); return v; };

    EqFx eq;
    eq.prepare (fs, 4096);
    float pe[kFxParams];
    const auto& de = fxDef (FxEq6);
    for (int i = 0; i < de.numParams; ++i) pe[i] = de.p[i].def;
    pe[5] = 1000.f; pe[6] = 12.f; pe[7] = 1.f;               // bande 3 : +12 dB a 1 kHz
    auto l = sine (1000.f, 4096 * 8), r = l;
    FxContext ctx;
    for (int b = 0; b < 8; ++b) eq.process (l.data() + b * 4096, r.data() + b * 4096, 4096, pe, ctx);
    std::vector<float> in = sine (1000.f, 4096 * 8);
    const double gain = rms (l, 0.3, 0.68, fs) / rms (in, 0.3, 0.68, fs);
    check (std::abs (20.0 * std::log10 (gain) - 12.0) < 0.5, "EQ : +12 dB a 1 kHz", juce::String (20.0 * std::log10 (gain), 2) + " dB");

    FilterFx flt;
    flt.prepare (fs, 4096);
    float pf[kFxParams] { 1.f, 2000.f, 0.707f, 0.f, 1.f };   // HP 12 a 2 kHz
    auto l2 = sine (100.f, 4096 * 8), r2 = l2;
    for (int b = 0; b < 8; ++b) flt.process (l2.data() + b * 4096, r2.data() + b * 4096, 4096, pf, ctx);
    const double att = 20.0 * std::log10 (rms (l2, 0.3, 0.68, fs) / rms (in, 0.3, 0.68, fs));
    check (att < -40.0, "Filtre HP 2 kHz attenue 100 Hz", juce::String (att, 1) + " dB");
}

static int countNoteOns (PatternPlayer& pl, const PlayParams& pp, const Pattern& pat, TestPlayHead& ph, double seconds, bool first)
{
    const double fs = 48000.0;
    const int block = 512;
    int count = 0;
    juce::MidiBuffer in, out;
    for (int pos = 0; pos < int (seconds * fs); pos += block)
    {
        in.clear(); out.clear();
        if (first && pos == 0) in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        pl.process (in, out, block, pp, pat, ph.bpm, ph.playing, ph.ppq, true);
        for (const auto m : out) if (m.getMessage().isNoteOn()) ++count;
        ph.ppq += double (block) / fs * ph.bpm / 60.0;
    }
    return count;
}

static void testSeqRateChange()
{
    for (int playing = 0; playing < 2; ++playing)
    {
        PatternPlayer pl (PatternPlayer::Kind::Seq);
        pl.prepare (48000.0);
        Pattern pat;
        for (int i = 0; i < 16; ++i) pat.add (i, 1, 0, 100);
        PlayParams pp;
        pp.on = true;
        pp.length = 16;
        pp.rate = 3;                       // 1/16
        TestPlayHead ph;
        ph.playing = playing == 1;
        const int a = countNoteOns (pl, pp, pat, ph, 1.0, true);       // 2 temps a 120 BPM : 8 doubles-croches
        pp.rate = 1;                       // 1/8
        const int b = countNoteOns (pl, pp, pat, ph, 1.0, false);      // attendu : 4 croches, immediatement
        const juce::String mode = playing ? "synchro hote" : "horloge libre";
        check (a >= 7 && a <= 9, "Seq 1/16 (" + mode + ")", juce::String (a) + " notes / 1 s");
        check (b >= 3 && b <= 5, "Seq Rate 1/16 -> 1/8 pris en compte tout de suite (" + mode + ")", juce::String (b) + " notes / 1 s");
    }
}

static void testMute()
{
    auto p = makeProc();
    setReal (*p, lpId (0, LP_Output), 0);
    setReal (*p, lpId (0, LP_Mute), 1);
    auto out = render (*p, 0.5, shortNote (0.4));
    check (rms (out, 0.2, 0.4) < 1e-5, "Mute coupe la couche");
}

static void testPresetRoundTrip()
{
    auto p = makeProc();
    setReal (*p, lpId (0, LP_Cutoff), 1234.f);
    p->setLayerName (0, "Big Bass");
    const int s = p->addFx (kMasterChain, FxEq6);
    Pattern pat;
    pat.add (0, 2, 7, 90);
    pat.add (4, 1, -5, 60);
    p->setPattern (0, AuraProcessor::kPatSeq, pat);

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aura_test.aurapreset");
    juce::String err;
    AuraProcessor::PresetMeta meta { "Test preset", "Bass", "test", "tests" };
    const bool saved = p->savePreset (file, meta, false, err);
    check (saved, "Sauvegarde preset", err);

    auto q = makeProc();
    const bool loaded = q->loadPreset (file, err);
    const float cut = q->apvts.getRawParameterValue (lpId (0, LP_Cutoff))->load();
    const auto qp = q->getPattern (0, AuraProcessor::kPatSeq);
    check (loaded && std::abs (cut - 1234.f) < 1.f && q->fxType (kMasterChain, s) == FxEq6
           && qp.n == 2 && qp.notes[1].row == -5 && q->getLayerName (0) == "Big Bass" && q->getPresetName() == "Test preset",
           "Rechargement preset (params, FX, motif, nom de couche)", err);
    file.deleteFile();
}

static void testFxChainEditing()
{
    auto p = makeProc();
    p->addFx (kMasterChain, FxComp);
    p->addFx (kMasterChain, FxDelay);
    p->addFx (kMasterChain, FxReverb);
    p->moveFx (kMasterChain, 2, -1);
    const bool moved = p->fxType (kMasterChain, 1) == FxReverb && p->fxType (kMasterChain, 2) == FxDelay;
    p->removeFx (kMasterChain, 0);
    const bool removed = p->fxType (kMasterChain, 0) == FxReverb && p->fxType (kMasterChain, 1) == FxDelay
                         && p->fxType (kMasterChain, 2) == FxNone;
    check (moved && removed, "Chaine FX : ajout / deplacement / suppression");
}

// --------------------------------------------------------------------------
static void snapshots (const juce::File& dir)
{
    auto p = makeProc();
    p->addFx (0, FxEq6);
    p->setLayerName (1, "Pad");
    setReal (*p, lpId (1, LP_Enable), 1);
    setReal (*p, lpId (1, LP_Output), 2);
    std::unique_ptr<AuraEditor> ed (dynamic_cast<AuraEditor*> (p->createEditor()));
    if (ed == nullptr) { check (false, "Creation de l'editeur"); return; }
    check (true, "Creation de l'editeur");
    static const char* const names[] = { "lib", "layer", "arpseq", "routing", "fx" };
    dir.createDirectory();
    for (int i = 0; i < 5; ++i)
    {
        ed->showPage (i);
        if (i == 3) ed->selectRoutingNode (kLayers + 0);       // inspecteur : chaine FX 1
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
        juce::PNGImageFormat png;
        const auto f = dir.getChildFile (juce::String ("aura_") + names[i] + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        check (ok, juce::String ("Capture ") + names[i], f.getFullPathName());
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    testBasicSound();
    testReverbAndDelay();
    testEveryEffectOnMaster();
    testEqAndFilterResponse();
    testSeqRateChange();
    testMute();
    testPresetRoundTrip();
    testFxChainEditing();
    if (argc > 1) snapshots (juce::File (juce::String (argv[1])));
    std::printf ("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
