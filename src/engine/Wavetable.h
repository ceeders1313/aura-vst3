#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace aura
{
// Tables mipmappees : 11 niveaux, le niveau l garde kMaxHarm >> l harmoniques (pas d'aliasing).
// Generees par synthese additive au demarrage (pas de fichier externe).
class WavetableBank
{
public:
    static constexpr int kSize = 2048;
    static constexpr int kFrames = 8;
    static constexpr int kLevels = 11;
    static constexpr int kMaxHarm = 1024;

    WavetableBank()
    {
        sinTab.resize (size_t (kSize) + 2);
        for (int i = 0; i < kSize; ++i)
            sinTab[size_t (i)] = float (std::sin (2.0 * 3.14159265358979323846 * double (i) / double (kSize)));
        sinTab[size_t (kSize)] = sinTab[0];
        sinTab[size_t (kSize) + 1] = sinTab[1];
        buildTables();
    }

    // dt = increment de phase par echantillon (f / fs)
    int levelFor (float dt) const noexcept
    {
        int l = 0, h = kMaxHarm;
        while (l < kLevels - 1 && float (h) * dt > 0.5f) { h >>= 1; ++l; }
        return l;
    }

    // ph dans [0,1[, framePos dans [0, kFrames-1]
    float read (int level, float framePos, float ph) const noexcept
    {
        int f0 = (int) framePos;
        if (f0 > kFrames - 2) f0 = kFrames - 2;
        if (f0 < 0) f0 = 0;
        const float ft = std::min (1.f, std::max (0.f, framePos - float (f0)));

        const float* a = &data[(size_t (level) * kFrames + size_t (f0)) * kSize];
        const float* b = a + kSize;

        const float x = ph * float (kSize);
        int i0 = (int) x;
        const float fr = x - float (i0);
        i0 &= (kSize - 1);
        const int i1 = (i0 + 1) & (kSize - 1);

        const float va = a[i0] + (a[i1] - a[i0]) * fr;
        const float vb = b[i0] + (b[i1] - b[i0]) * fr;
        return va + (vb - va) * ft;
    }

    // sinus par table + interpolation lineaire, phase en cycles (toute valeur)
    float sinCycles (float ph) const noexcept
    {
        ph -= std::floor (ph);
        const float x = ph * float (kSize);
        int i = (int) x;
        if (i > kSize) i = kSize;
        const float f = x - float (i);
        return sinTab[size_t (i)] + (sinTab[size_t (i) + 1] - sinTab[size_t (i)]) * f;
    }

private:
    static void fillSpectrum (int frame, std::vector<float>& amp)
    {
        std::fill (amp.begin(), amp.end(), 0.f);
        const double pi = 3.14159265358979323846;
        for (int h = 1; h <= kMaxHarm; ++h)
        {
            const float fh = float (h);
            float a = 0.f;
            switch (frame)
            {
                case 0: a = (h == 1) ? 1.f : 0.f; break;                                           // sinus
                case 1: if (h & 1) a = (((h - 1) / 2) % 2 ? -1.f : 1.f) / (fh * fh); break;        // triangle
                case 2: a = 1.f / fh; break;                                                       // dent de scie
                case 3: if (h & 1) a = 1.f / fh; break;                                            // carre
                case 4: a = float (std::sin (pi * double (h) * 0.25)) / fh; break;                 // impulsion 25 %
                case 5: a = (1.f / fh) * (1.f + 4.f * std::exp (-((fh - 8.f) / 2.f) * ((fh - 8.f) / 2.f))); break; // formant
                case 6:                                                                            // orgue
                    if (h == 1) a = 1.f; else if (h == 2) a = 0.8f; else if (h == 3) a = 0.6f;
                    else if (h == 4) a = 0.5f; else if (h == 6) a = 0.4f; else if (h == 8) a = 0.3f;
                    break;
                case 7: if (h <= 32) a = ((h % 3 == 0) ? -1.f : 1.f) / std::sqrt (fh); break;      // buzz numerique
                default: break;
            }
            amp[size_t (h)] = a;
        }
    }

    void buildTables()
    {
        data.assign (size_t (kLevels) * kFrames * kSize, 0.f);
        std::vector<float> amp (size_t (kMaxHarm) + 1);

        for (int f = 0; f < kFrames; ++f)
        {
            fillSpectrum (f, amp);
            float scale = 1.f;

            for (int l = 0; l < kLevels; ++l)
            {
                const int H = kMaxHarm >> l;
                float* out = &data[(size_t (l) * kFrames + size_t (f)) * kSize];

                for (int n = 0; n < kSize; ++n)
                {
                    float acc = 0.f;
                    for (int h = 1; h <= H; ++h)
                    {
                        const float a = amp[size_t (h)];
                        if (a != 0.f) acc += a * sinTab[size_t ((h * n) & (kSize - 1))];
                    }
                    out[n] = acc;
                }

                if (l == 0)
                {
                    float peak = 0.f;
                    for (int n = 0; n < kSize; ++n) peak = std::max (peak, std::abs (out[n]));
                    scale = (peak > 1e-6f) ? 0.9f / peak : 1.f;
                }
                for (int n = 0; n < kSize; ++n) out[n] *= scale;
            }
        }
    }

    std::vector<float> data;
    std::vector<float> sinTab;
};
} // namespace aura
