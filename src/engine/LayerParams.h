#pragma once
#include <juce_core/juce_core.h>
#include "ModTypes.h"

namespace aura
{
constexpr int kLayers = 4;      // modifiable (ex: 6) : tout le reste s'adapte

enum LP
{
    LP_Enable, LP_Level, LP_Pan, LP_Transpose, LP_KeyLo, LP_KeyHi, LP_VelLo, LP_VelHi,
    LP_SendCho, LP_SendDly, LP_SendRev, LP_Arp,
    LP_Source, LP_OneShot, LP_Wave, LP_Unison, LP_Detune, LP_WtPos,
    LP_FmAlgo, LP_FmR1, LP_FmR2, LP_FmIdx, LP_FmFb,
    LP_Cutoff, LP_Res, LP_Atk, LP_Dec, LP_Sus, LP_Rel, LP_MAtk, LP_MDec, LP_MSus, LP_MRel,
    LP_Lfo1Rate, LP_Lfo1Shape, LP_Lfo2Rate, LP_Lfo2Shape,
    LP_Mseg1Time, LP_Mseg1Loop, LP_Mseg2Time, LP_Mseg2Loop,
    LP_Macro1, LP_Macro2, LP_Macro3, LP_Macro4,
    LP_SeqOn, LP_SeqRate, LP_SeqLen, LP_SeqGate, LP_SeqSwing,
    LP_Mute, LP_Solo,
    LP_SlotBase,
    LP_StepBase = LP_SlotBase + 3 * kSlots,
    kNumLP = LP_StepBase + 3 * kSeqSteps
};

enum class Kind { Float, Bool, Int, Choice };

struct LPSpec
{
    const char* id;
    const char* name;
    Kind kind;
    float mn, mx, skew, def;
    const char* choices;
};

inline const LPSpec kLpSpecs[] = {
    { "on",      "Enable",        Kind::Bool,   0, 1, 1, 1, "" },
    { "level",   "Level",         Kind::Float,  0, 1, 1, 0.5f, "" },
    { "pan",     "Pan",           Kind::Float, -1, 1, 1, 0, "" },
    { "transp",  "Transpose",     Kind::Int,  -24, 24, 1, 0, "" },
    { "keylo",   "Key Low",       Kind::Int,    0, 127, 1, 0, "" },
    { "keyhi",   "Key High",      Kind::Int,    0, 127, 1, 127, "" },
    { "vello",   "Vel Low",       Kind::Int,    1, 127, 1, 1, "" },
    { "velhi",   "Vel High",      Kind::Int,    1, 127, 1, 127, "" },
    { "sendcho", "Chorus Send",   Kind::Float,  0, 1, 1, 0, "" },
    { "senddly", "Delay Send",    Kind::Float,  0, 1, 1, 0, "" },
    { "sendrev", "Reverb Send",   Kind::Float,  0, 1, 1, 0, "" },
    { "arp",     "Arp",           Kind::Bool,   0, 1, 1, 0, "" },
    { "source",  "Source",        Kind::Choice, 0, 0, 1, 0, "Synth|Sample|Synth+Sample" },
    { "oneshot", "One-shot",      Kind::Bool,   0, 1, 1, 0, "" },
    { "wave",    "Waveform",      Kind::Choice, 0, 0, 1, 0, "Saw|Square|Wavetable|FM|Triangle|Sine" },
    { "unison",  "Unison",        Kind::Int,    1, 7, 1, 5, "" },
    { "detune",  "Detune",        Kind::Float,  0, 1, 1, 0.25f, "" },
    { "wtpos",   "WT Position",   Kind::Float,  0, 1, 1, 0, "" },
    { "fmalgo",  "FM Algorithm",  Kind::Choice, 0, 0, 1, 0, "2-Op|3-Op Stack|3-Op Parallel" },
    { "fmr1",    "FM Ratio 1",    Kind::Float,  0.25f, 16, 0.5f, 2, "" },
    { "fmr2",    "FM Ratio 2",    Kind::Float,  0.25f, 16, 0.5f, 3, "" },
    { "fmidx",   "FM Index",      Kind::Float,  0, 12, 0.6f, 2, "" },
    { "fmfb",    "FM Feedback",   Kind::Float,  0, 1, 1, 0, "" },
    { "cutoff",  "Cutoff",        Kind::Float,  20, 20000, 0.3f, 8000, "" },
    { "res",     "Resonance",     Kind::Float,  0.5f, 10, 0.5f, 0.8f, "" },
    { "atk",     "Attack",        Kind::Float,  0.001f, 5, 0.4f, 0.005f, "" },
    { "dec",     "Decay",         Kind::Float,  0.001f, 5, 0.4f, 0.3f, "" },
    { "sus",     "Sustain",       Kind::Float,  0, 1, 1, 0.7f, "" },
    { "rel",     "Release",       Kind::Float,  0.001f, 8, 0.4f, 0.3f, "" },
    { "matk",    "ModEnv Attack", Kind::Float,  0.001f, 5, 0.4f, 0.01f, "" },
    { "mdec",    "ModEnv Decay",  Kind::Float,  0.001f, 5, 0.4f, 0.4f, "" },
    { "msus",    "ModEnv Sustain",Kind::Float,  0, 1, 1, 0, "" },
    { "mrel",    "ModEnv Release",Kind::Float,  0.001f, 8, 0.4f, 0.3f, "" },
    { "lfo1rate","LFO1 Rate",     Kind::Float,  0.05f, 30, 0.4f, 2, "" },
    { "lfo1shape","LFO1 Shape",   Kind::Choice, 0, 0, 1, 0, "Sine|Triangle|Saw|Square|S&H" },
    { "lfo2rate","LFO2 Rate",     Kind::Float,  0.05f, 30, 0.4f, 0.5f, "" },
    { "lfo2shape","LFO2 Shape",   Kind::Choice, 0, 0, 1, 0, "Sine|Triangle|Saw|Square|S&H" },
    { "ms1time", "MSEG1 Time",    Kind::Float,  0.01f, 20, 0.4f, 1, "" },
    { "ms1loop", "MSEG1 Loop",    Kind::Bool,   0, 1, 1, 0, "" },
    { "ms2time", "MSEG2 Time",    Kind::Float,  0.01f, 20, 0.4f, 1, "" },
    { "ms2loop", "MSEG2 Loop",    Kind::Bool,   0, 1, 1, 0, "" },
    { "macro1",  "Macro 1",       Kind::Float,  0, 1, 1, 0, "" },
    { "macro2",  "Macro 2",       Kind::Float,  0, 1, 1, 0, "" },
    { "macro3",  "Macro 3",       Kind::Float,  0, 1, 1, 0, "" },
    { "macro4",  "Macro 4",       Kind::Float,  0, 1, 1, 0, "" },
    { "seqon",   "Seq On",        Kind::Bool,   0, 1, 1, 0, "" },
    { "seqrate", "Seq Rate",      Kind::Choice, 0, 0, 1, 3, "1/4|1/8|1/8T|1/16|1/16T|1/32" },
    { "seqlen",  "Seq Length",    Kind::Int,    1, 16, 1, 16, "" },
    { "seqgate", "Seq Gate",      Kind::Float,  0.05f, 0.95f, 1, 0.5f, "" },
    { "seqswing","Seq Swing",     Kind::Float,  0, 1, 1, 0, "" },
    { "mute",    "Mute",          Kind::Bool,   0, 1, 1, 0, "" },
    { "solo",    "Solo",          Kind::Bool,   0, 1, 1, 0, "" },
};
static_assert (sizeof (kLpSpecs) / sizeof (LPSpec) == LP_SlotBase, "kLpSpecs doit suivre l'enum LP");

inline int lpSlot (int slot, int part) { return LP_SlotBase + slot * 3 + part; }   // part: 0 src, 1 dst, 2 amount
inline int lpStep (int step, int part) { return LP_StepBase + step * 3 + part; }   // part: 0 on, 1 pitch, 2 velocity

inline juce::String lpId (int layer, int lp)
{
    juce::String s ("L");
    s << (layer + 1) << "_";
    if (lp < LP_SlotBase)
    {
        s << kLpSpecs[lp].id;
    }
    else if (lp < LP_StepBase)
    {
        const int j = (lp - LP_SlotBase) / 3, t = (lp - LP_SlotBase) % 3;
        s << (t == 0 ? kSlotSrcId[j] : (t == 1 ? kSlotDstId[j] : kSlotAmtId[j]));
    }
    else
    {
        const int j = (lp - LP_StepBase) / 3, t = (lp - LP_StepBase) % 3;
        s << "s" << (j + 1) << (t == 0 ? "on" : (t == 1 ? "pit" : "vel"));
    }
    return s;
}
} // namespace aura
