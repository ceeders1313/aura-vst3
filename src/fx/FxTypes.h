#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

// Catalogue des effets : chaque type expose jusqu'a kFxParams parametres normalises (0..1)
// stockes dans l'APVTS (fx<c>s<s>p<i>). La table ci-dessous donne nom, plage, courbe et unite.
namespace aura::fx
{
constexpr int kFxParams = 18;     // max de parametres par effet (EQ 6 bandes = 17)
constexpr int kFxSlots  = 6;      // emplacements par chaine
constexpr int kFxGroups = 4;      // groupes FX 1..4 (assignables aux couches)
constexpr int kFxChains = kFxGroups + 1;   // + chaine Master (index kFxGroups)
constexpr int kMasterChain = kFxGroups;

enum FxType
{
    FxNone = 0, FxFilter, FxCut, FxEq6, FxComp, FxSat, FxCrush, FxChorus, FxPhaser, FxDelay, FxReverb, FxUtility,
    kNumFxTypes
};

enum class Unit { None, Hz, dB, ms, s, Pct, Ratio, Choice, Bits, X };

struct FxParamSpec
{
    const char* name;
    float mn, mx, skew, def;
    Unit unit;
    const char* choices;    // "A|B|C" si Unit::Choice
};

struct FxDef
{
    const char* name;
    const char* category;
    int numParams;
    FxParamSpec p[kFxParams];
};

inline const FxDef& fxDef (int type)
{
    static const FxDef defs[kNumFxTypes] = {
        { "None", "", 0, {} },
        { "Multi Filter", "Filter", 5, {
            { "Mode", 0, 5, 1, 0, Unit::Choice, "LP 12|HP 12|BP 12|Notch|LP 24|HP 24" },
            { "Cutoff", 20, 20000, 0.25f, 2000, Unit::Hz, "" },
            { "Reso", 0.5f, 12, 0.5f, 0.9f, Unit::None, "" },
            { "Drive", 0, 24, 1, 0, Unit::dB, "" },
            { "Mix", 0, 1, 1, 1, Unit::Pct, "" } } },
        { "Low / High Cut", "Filter", 3, {
            { "Low cut", 20, 2000, 0.3f, 20, Unit::Hz, "" },
            { "High cut", 1000, 20000, 0.4f, 20000, Unit::Hz, "" },
            { "Slope", 0, 1, 1, 0, Unit::Choice, "12 dB|24 dB" } } },
        { "EQ 6-band", "EQ", 17, {
            { "Low F", 20, 500, 0.4f, 80, Unit::Hz, "" },        { "Low G", -18, 18, 1, 0, Unit::dB, "" },
            { "B2 F", 40, 2000, 0.35f, 250, Unit::Hz, "" },      { "B2 G", -18, 18, 1, 0, Unit::dB, "" },  { "B2 Q", 0.2f, 10, 0.4f, 1, Unit::None, "" },
            { "B3 F", 100, 5000, 0.35f, 800, Unit::Hz, "" },     { "B3 G", -18, 18, 1, 0, Unit::dB, "" },  { "B3 Q", 0.2f, 10, 0.4f, 1, Unit::None, "" },
            { "B4 F", 300, 10000, 0.35f, 2500, Unit::Hz, "" },   { "B4 G", -18, 18, 1, 0, Unit::dB, "" },  { "B4 Q", 0.2f, 10, 0.4f, 1, Unit::None, "" },
            { "B5 F", 1000, 18000, 0.35f, 6000, Unit::Hz, "" },  { "B5 G", -18, 18, 1, 0, Unit::dB, "" },  { "B5 Q", 0.2f, 10, 0.4f, 1, Unit::None, "" },
            { "High F", 2000, 20000, 0.4f, 10000, Unit::Hz, "" },{ "High G", -18, 18, 1, 0, Unit::dB, "" },
            { "Output", -18, 18, 1, 0, Unit::dB, "" } } },
        { "Compressor", "Dynamics", 6, {
            { "Threshold", -60, 0, 1, -18, Unit::dB, "" },
            { "Ratio", 1, 20, 0.4f, 4, Unit::Ratio, "" },
            { "Attack", 0.1f, 100, 0.35f, 10, Unit::ms, "" },
            { "Release", 10, 1000, 0.35f, 120, Unit::ms, "" },
            { "Makeup", 0, 24, 1, 0, Unit::dB, "" },
            { "Mix", 0, 1, 1, 1, Unit::Pct, "" } } },
        { "Saturation", "Distortion", 4, {
            { "Mode", 0, 2, 1, 0, Unit::Choice, "Tanh|Hard clip|Asymmetric" },
            { "Drive", 0, 36, 1, 9, Unit::dB, "" },
            { "Output", -24, 6, 1, -3, Unit::dB, "" },
            { "Mix", 0, 1, 1, 1, Unit::Pct, "" } } },
        { "Bitcrusher", "Distortion", 3, {
            { "Bits", 2, 16, 1, 8, Unit::Bits, "" },
            { "Downsample", 1, 32, 0.4f, 4, Unit::X, "" },
            { "Mix", 0, 1, 1, 1, Unit::Pct, "" } } },
        { "Chorus", "Modulation", 3, {
            { "Rate", 0.05f, 5, 0.5f, 0.6f, Unit::Hz, "" },
            { "Depth", 0, 1, 1, 0.5f, Unit::Pct, "" },
            { "Mix", 0, 1, 1, 0.5f, Unit::Pct, "" } } },
        { "Phaser", "Modulation", 4, {
            { "Rate", 0.02f, 5, 0.4f, 0.3f, Unit::Hz, "" },
            { "Depth", 0, 1, 1, 0.7f, Unit::Pct, "" },
            { "Feedback", 0, 0.9f, 1, 0.5f, Unit::Pct, "" },
            { "Mix", 0, 1, 1, 0.5f, Unit::Pct, "" } } },
        { "Delay", "Delay", 6, {
            { "Sync", 0, 11, 1, 6, Unit::Choice, "Free|1/32|1/16|1/16 D|1/8 T|1/8|1/8 D|1/4 T|1/4|1/4 D|1/2|1 bar" },
            { "Time", 10, 2000, 0.4f, 350, Unit::ms, "" },
            { "Feedback", 0, 0.95f, 1, 0.4f, Unit::Pct, "" },
            { "Damping", 0, 1, 1, 0.4f, Unit::Pct, "" },
            { "Ping-pong", 0, 1, 1, 1, Unit::Choice, "Off|On" },
            { "Mix", 0, 1, 1, 0.3f, Unit::Pct, "" } } },
        { "Reverb", "Reverb", 5, {
            { "Decay", 0.3f, 12, 0.5f, 2.5f, Unit::s, "" },
            { "Size", 0.5f, 2, 1, 1, Unit::X, "" },
            { "Damping", 0, 1, 1, 0.4f, Unit::Pct, "" },
            { "Pre-delay", 0, 100, 1, 15, Unit::ms, "" },
            { "Mix", 0, 1, 1, 0.3f, Unit::Pct, "" } } },
        { "Stereo / Gain", "Utility", 3, {
            { "Gain", -24, 12, 1, 0, Unit::dB, "" },
            { "Width", 0, 2, 1, 1, Unit::Pct, "" },
            { "Pan", -1, 1, 1, 0, Unit::None, "" } } },
    };
    return defs[std::clamp (type, 0, kNumFxTypes - 1)];
}

inline juce::StringArray fxTypeNames()
{
    juce::StringArray s;
    for (int t = 0; t < kNumFxTypes; ++t) s.add (fxDef (t).name);
    return s;
}

inline int choiceCount (const FxParamSpec& s)
{
    return juce::StringArray::fromTokens (s.choices, "|", "").size();
}

// normalise (0..1) -> valeur reelle (sans allocation : utilisable cote audio)
inline float fxToReal (const FxParamSpec& s, float norm)
{
    norm = std::clamp (norm, 0.f, 1.f);
    if (s.unit == Unit::Choice) return std::round (norm * (s.mx - s.mn)) + s.mn;
    const float t = (s.skew != 1.f) ? std::exp (std::log (std::max (norm, 1e-9f)) / s.skew) : norm;
    return s.mn + (s.mx - s.mn) * t;
}

inline float fxToNorm (const FxParamSpec& s, float real)
{
    if (s.mx <= s.mn) return 0.f;
    const float t = std::clamp ((real - s.mn) / (s.mx - s.mn), 0.f, 1.f);
    if (s.unit == Unit::Choice) return t;
    return (s.skew != 1.f) ? std::pow (t, s.skew) : t;
}

inline juce::String fxFormat (const FxParamSpec& s, float real)
{
    auto num = [] (float v)      // 0.9 / 0.09 / 12.5 / 350 : pas de zeros inutiles
    {
        const float a = std::abs (v);
        auto s = juce::String (v, a >= 100.f ? 0 : (a >= 10.f ? 1 : 2));
        if (s.containsChar ('.'))
        {
            s = s.trimCharactersAtEnd ("0");
            if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
        }
        return s;
    };
    switch (s.unit)
    {
        case Unit::Hz:    return real >= 1000.f ? num (real / 1000.f) + " kHz" : num (real) + " Hz";
        case Unit::dB:    return num (real) + " dB";
        case Unit::ms:    return num (real) + " ms";
        case Unit::s:     return num (real) + " s";
        case Unit::Pct:   return juce::String (juce::roundToInt (real * 100.f)) + " %";
        case Unit::Ratio: return num (real) + ":1";
        case Unit::Bits:  return juce::String (juce::roundToInt (real)) + " bit";
        case Unit::X:     return "x" + num (real);
        case Unit::Choice:
        {
            const auto items = juce::StringArray::fromTokens (s.choices, "|", "");
            return items[juce::jlimit (0, items.size() - 1, juce::roundToInt (real - s.mn))];
        }
        case Unit::None:
        default:          return num (real);
    }
}

// identifiants APVTS
inline juce::String fxTypeId (int c, int s)        { return "fx" + juce::String (c) + "s" + juce::String (s) + "type"; }
inline juce::String fxOnId (int c, int s)          { return "fx" + juce::String (c) + "s" + juce::String (s) + "on"; }
inline juce::String fxParamId (int c, int s, int i){ return "fx" + juce::String (c) + "s" + juce::String (s) + "p" + juce::String (i); }
inline juce::String fxGainId (int c)               { return "fx" + juce::String (c) + "gain"; }
inline juce::String fxChainName (int c)            { return c == kMasterChain ? juce::String ("Master FX") : "FX " + juce::String (c + 1); }
} // namespace aura::fx
