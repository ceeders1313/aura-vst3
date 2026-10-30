#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace aura::fx
{
inline int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

inline float readLinear (const std::vector<float>& b, int w, float d, int mask)
{
    const float pos = float (w) - d;
    int i0 = (int) std::floor (pos);
    const float f = pos - float (i0);
    i0 &= mask;
    const int i1 = (i0 + 1) & mask;
    return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
}

// ---------------- Saturation (ADAA 1er ordre) ----------------
class Saturator
{
public:
    void reset() { x1[0] = x1[1] = 0.f; }

    // mode : 0 tanh, 1 hard clip, 2 asymetrique
    void process (float* L, float* R, int n, int mode, float driveDb, float mix)
    {
        const float gain = std::pow (10.f, driveDb / 20.f);
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
        {
            float* d = ch[c];
            for (int i = 0; i < n; ++i)
            {
                const float dry = d[i];
                const float x = dry * gain;
                float y;
                if (mode == 1)      y = adaaClip (x, x1[c]);
                else if (mode == 2) y = adaaTanh (x + 0.3f, x1[c]) - std::tanh (0.3f);
                else                y = adaaTanh (x, x1[c]);
                d[i] = dry + (y - dry) * mix;
            }
        }
    }

private:
    static float logCosh (float x)
    {
        x = std::abs (x);
        return x > 20.f ? x - 0.6931472f : std::log (std::cosh (x));
    }
    static float adaaTanh (float x, float& s)
    {
        const float d = x - s;
        const float y = (std::abs (d) < 1e-4f) ? std::tanh (0.5f * (x + s))
                                               : (logCosh (x) - logCosh (s)) / d;
        s = x;
        return y;
    }
    static float F1 (float x) { const float a = std::abs (x); return a <= 1.f ? 0.5f * x * x : a - 0.5f; }
    static float adaaClip (float x, float& s)
    {
        const float d = x - s;
        const float y = (std::abs (d) < 1e-4f) ? std::min (1.f, std::max (-1.f, 0.5f * (x + s)))
                                               : (F1 (x) - F1 (s)) / d;
        s = x;
        return y;
    }
    float x1[2] { 0.f, 0.f };
};

// ---------------- Chorus (2 prises par canal, LFO multiphase) ----------------
class Chorus
{
public:
    void prepare (double sr)
    {
        fs = float (sr);
        const int size = nextPow2 (int (0.06 * sr) + 8);
        mask = size - 1;
        bl.assign ((size_t) size, 0.f);
        br.assign ((size_t) size, 0.f);
        wp = 0; ph = 0.f;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, float rate, float depth)
    {
        const float inc = rate / fs;
        const float base = 0.012f * fs;
        const float amp = depth * 0.008f * fs;
        for (int i = 0; i < n; ++i)
        {
            bl[(size_t) wp] = inL[i];
            br[(size_t) wp] = inR[i];
            float l = 0.f, r = 0.f;
            for (int tap = 0; tap < 2; ++tap)
            {
                const float pL = ph + 0.5f * float (tap);
                const float pR = ph + 0.25f + 0.5f * float (tap);
                l += readLinear (bl, wp, base + amp * std::sin (6.2831853f * pL), mask);
                r += readLinear (br, wp, base + amp * std::sin (6.2831853f * pR), mask);
            }
            outL[i] = 0.5f * l;
            outR[i] = 0.5f * r;
            ph += inc; if (ph >= 1.f) ph -= 1.f;
            wp = (wp + 1) & mask;
        }
    }

private:
    float fs = 44100.f, ph = 0.f;
    int mask = 0, wp = 0;
    std::vector<float> bl, br;
};

// ---------------- Delay stereo (ping-pong optionnel, feedback filtre) ----------------
class StereoDelay
{
public:
    void prepare (double sr)
    {
        fs = float (sr);
        const int size = nextPow2 (int (2.2 * sr));
        mask = size - 1;
        bl.assign ((size_t) size, 0.f);
        br.assign ((size_t) size, 0.f);
        wp = 0; cur = 0.3f * fs; lpL = lpR = 0.f;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  float timeMs, float fb, float damp, bool ping)
    {
        const float target = std::min (float (mask - 8), std::max (2.f, timeMs * 0.001f * fs));
        const float fc = 500.f + (1.f - damp) * 15000.f;
        const float a = std::exp (-6.2831853f * fc / fs);
        for (int i = 0; i < n; ++i)
        {
            cur += (target - cur) * 0.0008f;      // lissage du temps (pas de clic)
            const float dL = readLinear (bl, wp, cur, mask);
            const float dR = readLinear (br, wp, cur, mask);
            lpL = dL * (1.f - a) + lpL * a;
            lpR = dR * (1.f - a) + lpR * a;
            if (ping)
            {
                const float m = 0.5f * (inL[i] + inR[i]);
                bl[(size_t) wp] = m + lpR * fb;
                br[(size_t) wp] = lpL * fb;
            }
            else
            {
                bl[(size_t) wp] = inL[i] + lpL * fb;
                br[(size_t) wp] = inR[i] + lpR * fb;
            }
            outL[i] = dL;
            outR[i] = dR;
            wp = (wp + 1) & mask;
        }
    }

private:
    float fs = 44100.f, cur = 0.f, lpL = 0.f, lpR = 0.f;
    int mask = 0, wp = 0;
    std::vector<float> bl, br;
};

// ---------------- Reverb FDN 8 lignes (matrice de Hadamard) ----------------
class FdnReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sr)
    {
        fs = float (sr);
        const float scale = fs / 48000.f;
        for (int i = 0; i < N; ++i) baseLen[i] = kBase[i] * scale;

        lineMask = nextPow2 (int (2800.f * 2.1f * scale) + 8) - 1;
        for (auto& l : line) l.assign ((size_t) lineMask + 1, 0.f);
        for (int i = 0; i < N; ++i) { lenS[i] = baseLen[i]; lp[i] = 0.f; }

        preMask = nextPow2 (int (0.12f * fs) + 8) - 1;
        pre.assign ((size_t) preMask + 1, 0.f);
        apA.init (int (142.f * scale));
        apB.init (int (107.f * scale));
        wp = 0; pw = 0;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  float decay, float size, float damp, float preMs)
    {
        const float dampA = 0.05f + 0.9f * damp;
        float target[N], g[N];
        for (int i = 0; i < N; ++i)
        {
            target[i] = baseLen[i] * size;
            g[i] = std::pow (10.f, -3.f * target[i] / (fs * std::max (decay, 0.1f)));
        }
        const float preS = std::min (float (preMask - 2), std::max (0.f, preMs * 0.001f * fs));

        for (int s = 0; s < n; ++s)
        {
            pre[(size_t) pw] = 0.5f * (inL[s] + inR[s]);
            float in = readLinear (pre, pw, preS, preMask);
            pw = (pw + 1) & preMask;
            in = apA.process (in, 0.6f);
            in = apB.process (in, 0.6f);

            float d[N];
            for (int i = 0; i < N; ++i)
            {
                lenS[i] += (target[i] - lenS[i]) * 0.0005f;
                const float x = readLinear (line[(size_t) i], wp, lenS[i], lineMask);
                lp[i] = x * (1.f - dampA) + lp[i] * dampA;
                d[i] = lp[i];
            }

            float oL = 0.f, oR = 0.f;
            for (int i = 0; i < N; ++i)
            {
                oL += ((i & 1) ? -1.f : 1.f) * d[i];
                oR += (((i >> 1) & 1) ? -1.f : 1.f) * d[i];
            }

            float m[N];
            for (int i = 0; i < N; ++i) m[i] = d[i] * g[i];
            fwht8 (m);
            for (int i = 0; i < N; ++i)
            {
                const float inj = in * ((i & 1) ? -0.3f : 0.3f);
                line[(size_t) i][(size_t) wp] = m[i] * 0.35355339f + inj;
            }
            wp = (wp + 1) & lineMask;

            outL[s] = oL * 0.25f;
            outR[s] = oR * 0.25f;
        }
    }

private:
    struct Allpass
    {
        std::vector<float> b;
        int mask = 0, w = 0, len = 1;
        void init (int l)
        {
            len = std::max (1, l);
            const int size = nextPow2 (len + 4);
            mask = size - 1;
            b.assign ((size_t) size, 0.f);
            w = 0;
        }
        float process (float x, float g)
        {
            const float d = b[(size_t) ((w - len) & mask)];
            const float v = x - g * d;
            b[(size_t) w] = v;
            w = (w + 1) & mask;
            return d + g * v;
        }
    };

    static void fwht8 (float* x)
    {
        for (int len = 1; len < 8; len <<= 1)
            for (int i = 0; i < 8; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float a = x[j], b = x[j + len];
                    x[j] = a + b;
                    x[j + len] = a - b;
                }
    }

    static constexpr float kBase[N] = { 1051.f, 1237.f, 1423.f, 1627.f, 1861.f, 2083.f, 2311.f, 2539.f };

    float fs = 48000.f;
    float baseLen[N] {}, lenS[N] {}, lp[N] {};
    std::array<std::vector<float>, N> line;
    std::vector<float> pre;
    Allpass apA, apB;
    int lineMask = 0, preMask = 0, wp = 0, pw = 0;
};

// ---------------- Limiteur look-ahead (pic echantillon) ----------------
class Limiter
{
public:
    void prepare (double sr)
    {
        fs = float (sr);
        la = std::max (16, int (0.0015 * sr));
        dl.assign ((size_t) la, 0.f);
        dr.assign ((size_t) la, 0.f);
        req.assign ((size_t) la + 1, 1.f);
        wp = 0; rw = 0; g = 1.f;
    }

    int latency() const { return la; }

    void process (float* L, float* R, int n, float ceilLin, float relMs, bool on)
    {
        const float atk = std::exp (-1.f / (0.2f * float (la)));
        const float rel = std::exp (-1.f / (std::max (relMs, 1.f) * 0.001f * fs));
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R[i];
            const float peak = std::max (std::abs (xl), std::abs (xr));
            req[(size_t) rw] = (on && peak > ceilLin) ? ceilLin / peak : 1.f;
            rw = (rw + 1) % (la + 1);

            float mn = 1.f;
            for (float v : req) mn = std::min (mn, v);
            g = (mn < g) ? mn + (g - mn) * atk : mn + (g - mn) * rel;

            const float ol = dl[(size_t) wp], orr = dr[(size_t) wp];
            dl[(size_t) wp] = xl;
            dr[(size_t) wp] = xr;
            wp = (wp + 1) % la;

            float yl = ol * g, yr = orr * g;
            if (on)
            {
                yl = std::min (ceilLin, std::max (-ceilLin, yl));
                yr = std::min (ceilLin, std::max (-ceilLin, yr));
            }
            L[i] = yl;
            R[i] = yr;
        }
    }

private:
    float fs = 48000.f, g = 1.f;
    int la = 72, wp = 0, rw = 0;
    std::vector<float> dl, dr, req;
};
} // namespace aura::fx
