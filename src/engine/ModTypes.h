#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

namespace aura
{
constexpr int kSlots = 8;
constexpr int kSeqSteps = 16;

enum Src { SrcNone = 0, SrcModEnv, SrcLfo1, SrcLfo2, SrcMseg, SrcMseg2, SrcVelocity, SrcKey,
           SrcModWheel, SrcRandom, SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, kNumSrc };

enum Dst { DstNone = 0, DstPitch, DstCutoff, DstRes, DstWtPos, DstFmIndex, DstDetune, DstLevel, kNumDst };

inline const char* const kSrcNames[kNumSrc] = {
    "None", "Mod Env", "LFO 1", "LFO 2", "MSEG 1", "MSEG 2", "Velocity", "Key",
    "Mod Wheel", "Random", "Macro 1", "Macro 2", "Macro 3", "Macro 4" };

inline const char* const kDstNames[kNumDst] = {
    "None", "Pitch", "Cutoff", "Resonance", "WT Position", "FM Index", "Detune", "Level" };

inline const char* const kSlotSrcId[kSlots] = { "m1src", "m2src", "m3src", "m4src", "m5src", "m6src", "m7src", "m8src" };
inline const char* const kSlotDstId[kSlots] = { "m1dst", "m2dst", "m3dst", "m4dst", "m5dst", "m6dst", "m7dst", "m8dst" };
inline const char* const kSlotAmtId[kSlots] = { "m1amt", "m2amt", "m3amt", "m4amt", "m5amt", "m6amt", "m7amt", "m8amt" };
inline const char* const kMacroId[4] = { "macro1", "macro2", "macro3", "macro4" };

inline juce::StringArray namesToArray (const char* const* a, int n)
{
    juce::StringArray s;
    for (int i = 0; i < n; ++i) s.add (a[i]);
    return s;
}

struct ModSlot
{
    int src = 0, dst = 0;
    float amount = 0.f;
};

// Forme MSEG : points (x,y) normalises, tension par segment. POD copiable : echange sans allocation.
struct MsegShape
{
    static constexpr int kMax = 16;
    int n = 4;
    float x[kMax]     = { 0.f, 0.1f, 0.5f, 1.f };
    float y[kMax]     = { 0.f, 1.f, 0.5f, 0.f };
    float curve[kMax] = {};     // curve[i] : segment i -> i+1, [-1, 1]

    float eval (float t) const noexcept
    {
        t = std::min (1.f, std::max (0.f, t));
        int i = 0;
        while (i < n - 2 && t >= x[i + 1]) ++i;

        const float x0 = x[i], x1 = x[i + 1];
        float u = (x1 > x0) ? (t - x0) / (x1 - x0) : 1.f;
        const float c = curve[i];
        if (std::abs (c) > 0.001f)
        {
            const float k = c * 6.f;
            u = (std::exp (k * u) - 1.f) / (std::exp (k) - 1.f);
        }
        return y[i] + (y[i + 1] - y[i]) * u;
    }

    juce::String toString() const
    {
        juce::String s (n);
        for (int i = 0; i < n; ++i)
            s << "|" << juce::String (x[i], 4) << "," << juce::String (y[i], 4) << "," << juce::String (curve[i], 4);
        return s;
    }

    static MsegShape fromString (const juce::String& str)
    {
        MsegShape def;
        const auto parts = juce::StringArray::fromTokens (str, "|", "");
        if (parts.size() < 3) return def;
        const int cnt = parts[0].getIntValue();
        if (cnt < 2 || cnt > kMax || parts.size() != cnt + 1) return def;

        MsegShape o;
        o.n = cnt;
        for (int i = 0; i < cnt; ++i)
        {
            const auto f = juce::StringArray::fromTokens (parts[i + 1], ",", "");
            if (f.size() != 3) return def;
            o.x[i]     = f[0].getFloatValue();
            o.y[i]     = std::min (1.f, std::max (0.f, f[1].getFloatValue()));
            o.curve[i] = std::min (1.f, std::max (-1.f, f[2].getFloatValue()));
        }
        o.x[0] = 0.f;
        o.x[cnt - 1] = 1.f;
        for (int i = 1; i < cnt - 1; ++i)
            o.x[i] = std::min (1.f - 1e-4f, std::max (o.x[i - 1] + 1e-4f, o.x[i]));
        return o;
    }
};
} // namespace aura
