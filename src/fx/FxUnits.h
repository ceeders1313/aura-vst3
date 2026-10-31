#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include "Effects.h"
#include "FxTypes.h"

// Effets d'insert des chaines FX. process() est temps reel (aucune allocation) ;
// prepare() alloue (thread non audio). p[] = valeurs reelles (voir FxTypes.h).
namespace aura::fx
{
struct FxContext { double bpm = 120.0; };

class FxUnit
{
public:
    virtual ~FxUnit() = default;
    virtual void prepare (double sr, int maxBlock) = 0;
    virtual void reset() {}
    virtual void process (float* L, float* R, int n, const float* p, const FxContext& ctx) = 0;
};

inline float dbToGain (float db) { return std::pow (10.f, db / 20.f); }
constexpr float kTwoPi = 6.2831853f;

// ---------------- SVF (Simper) ----------------
struct Svf2
{
    float ic1 = 0.f, ic2 = 0.f;
    void reset() { ic1 = ic2 = 0.f; }
    // mode : 0 LP, 1 HP, 2 BP, 3 Notch
    inline float process (float x, float g, float k, int mode)
    {
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        switch (mode)
        {
            case 1:  return x - k * v1 - v2;
            case 2:  return v1 * k;              // BP normalise (gain 1 au centre)
            case 3:  return x - k * v1;
            default: return v2;
        }
    }
};

inline float svfG (float fc, float fs) { return std::tan (3.14159265f * std::clamp (fc, 10.f, 0.49f * fs) / fs); }

// ---------------- Multi filter ----------------
class FilterFx : public FxUnit
{
public:
    void prepare (double sr, int) override { fs = float (sr); reset(); }
    void reset() override { for (auto& s : f) s.reset(); }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        const int mode = int (p[0]);
        const bool steep = mode >= 4;
        const int m = steep ? (mode == 4 ? 0 : 1) : mode;
        const float g = svfG (p[1], fs);
        const float k = 1.f / std::max (0.5f, p[2]);
        const float k2 = steep ? 1.4142f : k;          // 2e etage : Butterworth (reso sur le 1er)
        const float drive = dbToGain (p[3]), mix = p[4];
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float dry = ch[c][i];
                float y = f[c * 2].process (dry * drive, g, k, m);
                if (steep) y = f[c * 2 + 1].process (y, g, k2, m);
                if (drive > 1.001f) y = std::tanh (y);
                ch[c][i] = dry + (y - dry) * mix;
            }
    }
private:
    float fs = 48000.f;
    Svf2 f[4];
};

// ---------------- Low / High cut ----------------
class CutFx : public FxUnit
{
public:
    void prepare (double sr, int) override { fs = float (sr); reset(); }
    void reset() override { for (auto& s : f) s.reset(); }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        const float gh = svfG (p[0], fs), gl = svfG (p[1], fs);
        const bool steep = p[2] > 0.5f;
        const float k = 1.4142f;
        const bool doHp = p[0] > 21.f, doLp = p[1] < 19900.f;
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float y = ch[c][i];
                if (doHp) { y = f[c * 4].process (y, gh, k, 1);     if (steep) y = f[c * 4 + 1].process (y, gh, k, 1); }
                if (doLp) { y = f[c * 4 + 2].process (y, gl, k, 0); if (steep) y = f[c * 4 + 3].process (y, gl, k, 0); }
                ch[c][i] = y;
            }
    }
private:
    float fs = 48000.f;
    Svf2 f[8];
};

// ---------------- EQ 6 bandes (biquads RBJ) ----------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.f; }
    inline float process (float x, int c)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    // type : 0 low shelf, 1 peak, 2 high shelf
    void set (int type, float fs, float f, float gainDb, float q)
    {
        const float A = std::pow (10.f, gainDb / 40.f);
        const float w0 = kTwoPi * std::clamp (f, 10.f, 0.49f * fs) / fs;
        const float cw = std::cos (w0), sw = std::sin (w0);
        float B0, B1, B2, A0, A1, A2;
        if (type == 1)
        {
            const float alpha = sw / (2.f * std::max (0.05f, q));
            B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
            A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A;
        }
        else
        {
            const float alpha = sw / 2.f * std::sqrt (2.f);          // pente S = 1
            const float sq = 2.f * std::sqrt (A) * alpha;
            if (type == 0)
            {
                B0 = A * ((A + 1) - (A - 1) * cw + sq); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sq);
                A0 = (A + 1) + (A - 1) * cw + sq;       A1 = -2 * ((A - 1) + (A + 1) * cw);    A2 = (A + 1) + (A - 1) * cw - sq;
            }
            else
            {
                B0 = A * ((A + 1) + (A - 1) * cw + sq); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq;       A1 = 2 * ((A - 1) - (A + 1) * cw);      A2 = (A + 1) - (A - 1) * cw - sq;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    // reponse en amplitude (pour l'affichage)
    float magnitude (float f, float fs) const
    {
        const double w = 2.0 * 3.14159265358979 * f / fs;
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i;
        const double dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
        return float (std::sqrt ((nr * nr + ni * ni) / std::max (1e-12, dr * dr + di * di)));
    }
};

// p : [0]LowF [1]LowG [2..4]B2 F/G/Q [5..7]B3 [8..10]B4 [11..13]B5 [14]HighF [15]HighG [16]Output
inline void eqDesign (Biquad (&b)[6], const float* p, float fs, bool (&active)[6])
{
    b[0].set (0, fs, p[0], p[1], 0.707f);   active[0] = std::abs (p[1]) > 0.01f;
    for (int k = 0; k < 4; ++k)
    {
        b[1 + k].set (1, fs, p[2 + 3 * k], p[3 + 3 * k], p[4 + 3 * k]);
        active[1 + k] = std::abs (p[3 + 3 * k]) > 0.01f;
    }
    b[5].set (2, fs, p[14], p[15], 0.707f); active[5] = std::abs (p[15]) > 0.01f;
}

class EqFx : public FxUnit
{
public:
    void prepare (double sr, int) override { fs = float (sr); reset(); }
    void reset() override { for (auto& b : band) b.reset(); }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        bool active[6];
        eqDesign (band, p, fs, active);
        const float out = dbToGain (p[16]);
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float y = ch[c][i];
                for (int k = 0; k < 6; ++k) if (active[k]) y = band[k].process (y, c);
                ch[c][i] = y * out;
            }
    }
private:
    float fs = 48000.f;
    Biquad band[6];
};

// ---------------- Compresseur ----------------
class CompFx : public FxUnit
{
public:
    void prepare (double sr, int) override { fs = float (sr); reset(); }
    void reset() override { env = 0.f; }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        const float thr = p[0], ratio = std::max (1.f, p[1]);
        const float atk = std::exp (-1.f / (std::max (0.05f, p[2]) * 0.001f * fs));
        const float rel = std::exp (-1.f / (std::max (1.f, p[3]) * 0.001f * fs));
        const float makeup = dbToGain (p[4]), mix = p[5];
        for (int i = 0; i < n; ++i)
        {
            const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
            const float lvlDb = 20.f * std::log10 (std::max (lvl, 1e-6f));
            const float over = lvlDb - thr;
            const float target = over > 0.f ? over * (1.f - 1.f / ratio) : 0.f;    // reduction en dB
            env = target > env ? target + (env - target) * atk : target + (env - target) * rel;
            const float g = dbToGain (-env) * makeup;
            L[i] = L[i] * (1.f - mix) + L[i] * g * mix;
            R[i] = R[i] * (1.f - mix) + R[i] * g * mix;
        }
    }
    float gainReductionDb() const { return env; }
private:
    float fs = 48000.f, env = 0.f;
};

// ---------------- Saturation (ADAA) ----------------
class SatFx : public FxUnit
{
public:
    void prepare (double, int) override { sat.reset(); }
    void reset() override { sat.reset(); }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        sat.process (L, R, n, int (p[0]), p[1], p[3]);
        const float g = dbToGain (p[2]);
        for (int i = 0; i < n; ++i) { L[i] *= g; R[i] *= g; }
    }
private:
    Saturator sat;
};

// ---------------- Bitcrusher ----------------
class CrushFx : public FxUnit
{
public:
    void prepare (double, int) override { reset(); }
    void reset() override { hold[0] = hold[1] = 0.f; cnt = 0.f; }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        const float levels = std::pow (2.f, std::clamp (p[0], 2.f, 16.f) - 1.f);
        const float ds = std::max (1.f, p[1]), mix = p[2];
        for (int i = 0; i < n; ++i)
        {
            cnt += 1.f;
            if (cnt >= ds)
            {
                cnt -= ds;
                hold[0] = std::round (L[i] * levels) / levels;
                hold[1] = std::round (R[i] * levels) / levels;
            }
            L[i] += (hold[0] - L[i]) * mix;
            R[i] += (hold[1] - R[i]) * mix;
        }
    }
private:
    float hold[2] {}, cnt = 0.f;
};

// ---------------- Chorus (wrapper) ----------------
class ChorusFx : public FxUnit
{
public:
    void prepare (double sr, int maxBlock) override
    {
        sr0 = sr;
        cho.prepare (sr);
        wl.assign ((size_t) maxBlock, 0.f);
        wr.assign ((size_t) maxBlock, 0.f);
    }
    void reset() override { cho.prepare (sr0); }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        n = std::min (n, (int) wl.size());
        cho.process (L, R, wl.data(), wr.data(), n, p[0], p[1]);
        const float mix = p[2];
        for (int i = 0; i < n; ++i) { L[i] += (wl[(size_t) i] - L[i]) * mix; R[i] += (wr[(size_t) i] - R[i]) * mix; }
    }
private:
    Chorus cho;
    double sr0 = 48000.0;
    std::vector<float> wl, wr;
};

// ---------------- Phaser (6 passe-tout du 1er ordre) ----------------
class PhaserFx : public FxUnit
{
public:
    void prepare (double sr, int) override { fs = float (sr); reset(); }
    void reset() override { for (auto& s : st) for (auto& v : s) v = 0.f; fb[0] = fb[1] = 0.f; ph = 0.f; }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        const float inc = p[0] / fs, depth = p[1], feed = p[2], mix = p[3];
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (ph + 0.25f * float (c)));
                const float fc = 200.f * std::pow (20.f, lfo * depth);          // 200 Hz .. 4 kHz
                const float t = std::tan (3.14159265f * std::min (fc, 0.45f * fs) / fs);
                const float a = (t - 1.f) / (t + 1.f);
                const float dry = ch[c][i];
                float x = dry + fb[c] * feed;
                for (int k = 0; k < 6; ++k)
                {
                    const float y = a * x + st[c][k];
                    st[c][k] = x - a * y;
                    x = y;
                }
                fb[c] = x;
                ch[c][i] = dry * (1.f - mix) + 0.5f * (dry + x) * mix;
            }
            ph += inc; if (ph >= 1.f) ph -= 1.f;
        }
    }
private:
    float fs = 48000.f, ph = 0.f;
    float st[2][6] {}, fb[2] {};
};

// ---------------- Delay (wrapper, synchro tempo) ----------------
class DelayFx : public FxUnit
{
public:
    void prepare (double sr, int maxBlock) override
    {
        sr0 = sr;
        dly.prepare (sr);
        wl.assign ((size_t) maxBlock, 0.f);
        wr.assign ((size_t) maxBlock, 0.f);
    }
    void reset() override { dly.prepare (sr0); }
    void process (float* L, float* R, int n, const float* p, const FxContext& ctx) override
    {
        static constexpr float kBeats[12] = { 0.f, 0.125f, 0.25f, 0.375f, 1.f / 3.f, 0.5f, 0.75f, 2.f / 3.f, 1.f, 1.5f, 2.f, 4.f };
        const int sync = std::clamp (int (p[0]), 0, 11);
        const float ms = sync == 0 ? p[1] : float (kBeats[sync] * 60000.0 / std::max (20.0, ctx.bpm));
        n = std::min (n, (int) wl.size());
        dly.process (L, R, wl.data(), wr.data(), n, ms, p[2], p[3], p[4] > 0.5f);
        const float mix = p[5];
        for (int i = 0; i < n; ++i) { L[i] += (wl[(size_t) i] - L[i]) * mix; R[i] += (wr[(size_t) i] - R[i]) * mix; }
    }
private:
    StereoDelay dly;
    double sr0 = 48000.0;
    std::vector<float> wl, wr;
};

// ---------------- Reverb (wrapper) ----------------
class ReverbFx : public FxUnit
{
public:
    void prepare (double sr, int maxBlock) override
    {
        sr0 = sr;
        rev.prepare (sr);
        wl.assign ((size_t) maxBlock, 0.f);
        wr.assign ((size_t) maxBlock, 0.f);
    }
    void reset() override { rev.prepare (sr0); }
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        n = std::min (n, (int) wl.size());
        rev.process (L, R, wl.data(), wr.data(), n, p[0], p[1], p[2], p[3]);
        const float mix = p[4];
        // le signal sec reste a niveau plein jusqu'a mix = 0.5, puis descend (wet seul a 1)
        const float dryG = std::min (1.f, 2.f * (1.f - mix)), wetG = std::min (1.f, 2.f * mix) * 2.f;
        for (int i = 0; i < n; ++i) { L[i] = L[i] * dryG + wl[(size_t) i] * wetG; R[i] = R[i] * dryG + wr[(size_t) i] * wetG; }
    }
private:
    FdnReverb rev;
    double sr0 = 48000.0;
    std::vector<float> wl, wr;
};

// ---------------- Stereo / Gain ----------------
class UtilityFx : public FxUnit
{
public:
    void prepare (double, int) override {}
    void process (float* L, float* R, int n, const float* p, const FxContext&) override
    {
        const float g = dbToGain (p[0]), w = p[1], pan = std::clamp (p[2], -1.f, 1.f);
        const float gl = g * std::min (1.f, 1.f - pan), gr = g * std::min (1.f, 1.f + pan);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * w;
            L[i] = (m + s) * gl;
            R[i] = (m - s) * gr;
        }
    }
};

inline std::unique_ptr<FxUnit> createFx (int type)
{
    switch (type)
    {
        case FxFilter:  return std::make_unique<FilterFx>();
        case FxCut:     return std::make_unique<CutFx>();
        case FxEq6:     return std::make_unique<EqFx>();
        case FxComp:    return std::make_unique<CompFx>();
        case FxSat:     return std::make_unique<SatFx>();
        case FxCrush:   return std::make_unique<CrushFx>();
        case FxChorus:  return std::make_unique<ChorusFx>();
        case FxPhaser:  return std::make_unique<PhaserFx>();
        case FxDelay:   return std::make_unique<DelayFx>();
        case FxReverb:  return std::make_unique<ReverbFx>();
        case FxUtility: return std::make_unique<UtilityFx>();
        default:        return nullptr;
    }
}
} // namespace aura::fx
