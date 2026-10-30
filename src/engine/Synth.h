#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "SampleBank.h"

namespace aura
{
constexpr float kPi = 3.14159265358979f;

struct Params
{
    int   source = 0;        // 0 Synth, 1 Sample, 2 Synth+Sample
    bool  oneShot = false;   // samples sans boucle : note-off ignore
    int   wave   = 2;        // 0 Saw, 1 Square, 2 Supersaw
    float detune = 0.25f;
    float cutoff = 8000.f;
    float res    = 0.8f;
    float atk = 0.005f, dec = 0.3f, sus = 0.7f, rel = 0.3f;
    float gain   = 0.5f;
};

inline float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// Interpolation de Hermite 4 points
inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

struct Adsr
{
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Stage::Idle;
    float level = 0.f;

    void noteOn()  { stage = Stage::Attack; }
    void noteOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset()   { stage = Stage::Idle; level = 0.f; }
    bool active() const { return stage != Stage::Idle; }

    float next (const Params& p, float fs)
    {
        switch (stage)
        {
            case Stage::Attack:
                level += 1.f / (std::max (p.atk, 0.001f) * fs);
                if (level >= 1.f) { level = 1.f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
            {
                const float c = 1.f - std::exp (-1.f / (std::max (p.dec, 0.001f) * fs));
                level += (p.sus - level) * c;
                if (std::abs (level - p.sus) < 1e-4f) stage = Stage::Sustain;
                break;
            }
            case Stage::Sustain: level = p.sus; break;
            case Stage::Release:
            {
                const float c = 1.f - std::exp (-1.f / (std::max (p.rel, 0.001f) * fs));
                level -= level * c;
                if (level < 1e-4f) { level = 0.f; stage = Stage::Idle; }
                break;
            }
            case Stage::Idle: break;
        }
        return level;
    }
};

struct Svf
{
    float ic1 = 0.f, ic2 = 0.f;
    void reset() { ic1 = ic2 = 0.f; }
    inline float lowpass (float x, float a1, float a2, float a3)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        return v2;
    }
};

class Voice
{
public:
    bool active = false, released = false;
    int note = -1;
    float vel = 0.f;
    uint64_t age = 0;

    void start (int n, float v, uint64_t a, const Params& p,
                const SampleBank::Ptr& b, const Zone* z)
    {
        note = n; vel = v; age = a; active = true; released = false;
        bank = b; zone = z;

        synthOn  = (p.source != 1);
        sampleOn = (p.source >= 1 && z != nullptr);
        holdOneShot = p.oneShot && sampleOn && ! synthOn && ! z->hasLoop();

        freq = 440.f * std::pow (2.f, (float (n) - 69.f) / 12.f);
        for (int i = 0; i < kOsc; ++i)
        {
            const float x = float (i) * 0.37f + float (n) * 0.113f;
            phase[i] = x - std::floor (x);
        }

        pos = 0.0;
        ratio = (z != nullptr) ? std::pow (2.0, double (n - z->rootKey) / 12.0) : 1.0;

        fl.reset(); fr.reset(); env.reset(); env.noteOn();
    }

    void stop()
    {
        if (holdOneShot) return;      // one-shot : le sample va jusqu'au bout
        released = true;
        env.noteOff();
    }

    float level() const { return env.level; }

    void render (float* L, float* R, int n, const Params& p, float fs)
    {
        static constexpr float offs[kOsc] = { -1.f, -0.6f, -0.25f, 0.f, 0.25f, 0.6f, 1.f };

        const int   count = (p.wave == 2) ? kOsc : 1;
        const int   first = (p.wave == 2) ? 0 : 3;
        const float norm  = (p.wave == 2) ? 0.33f : 0.7f;

        float dt[kOsc] {}, gl[kOsc] {}, gr[kOsc] {};
        if (synthOn)
        {
            for (int k = 0; k < count; ++k)
            {
                const int i = first + k;
                const float o = (p.wave == 2) ? offs[i] : 0.f;
                dt[i] = freq * std::pow (2.f, o * p.detune * 0.5f / 12.f) / fs;
                const float pan = (p.wave == 2) ? o * 0.8f : 0.f;
                gl[i] = std::sqrt (0.5f * (1.f - pan));
                gr[i] = std::sqrt (0.5f * (1.f + pan));
            }
        }

        const float* dl = nullptr;
        const float* dr = nullptr;
        int len = 0;
        double inc = 0.0;
        if (sampleOn)
        {
            dl  = zone->data.getReadPointer (0);
            dr  = zone->data.getReadPointer (1);
            len = zone->data.getNumSamples();
            inc = zone->srcRate / double (fs) * ratio;
        }

        const float fc = juce::jlimit (20.f, 0.45f * fs, p.cutoff);
        const float g  = std::tan (kPi * fc / fs);
        const float k  = 1.f / std::max (p.res, 0.3f);
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        for (int s = 0; s < n; ++s)
        {
            float l = 0.f, r = 0.f;

            if (synthOn)
            {
                float sl = 0.f, sr = 0.f;
                for (int c = 0; c < count; ++c)
                {
                    const int i = first + c;
                    float ph = phase[i];
                    float v;
                    if (p.wave == 1)
                    {
                        v = ph < 0.5f ? 1.f : -1.f;
                        v += polyBlep (ph, dt[i]);
                        float p2 = ph + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
                        v -= polyBlep (p2, dt[i]);
                    }
                    else
                    {
                        v = 2.f * ph - 1.f - polyBlep (ph, dt[i]);
                    }
                    sl += v * gl[i];
                    sr += v * gr[i];
                    ph += dt[i]; if (ph >= 1.f) ph -= 1.f;
                    phase[i] = ph;
                }
                l += sl * norm;
                r += sr * norm;
            }

            if (sampleOn)
            {
                const int i0 = (int) pos;
                const float t = float (pos - double (i0));
                l += hermite (fetch (dl, i0 - 1, len), fetch (dl, i0, len),
                              fetch (dl, i0 + 1, len), fetch (dl, i0 + 2, len), t) * zone->gain;
                r += hermite (fetch (dr, i0 - 1, len), fetch (dr, i0, len),
                              fetch (dr, i0 + 1, len), fetch (dr, i0 + 2, len), t) * zone->gain;

                pos += inc;
                if (zone->hasLoop())
                {
                    const double loopLen = double (zone->loopEnd - zone->loopStart);
                    while (pos >= double (zone->loopEnd)) pos -= loopLen;
                }
                else if (pos >= double (len))
                {
                    sampleOn = false;
                    holdOneShot = false;
                    if (! synthOn) stop();
                }
            }

            l = fl.lowpass (l, a1, a2, a3);
            r = fr.lowpass (r, a1, a2, a3);

            const float e = env.next (p, fs) * vel;
            L[s] += l * e;
            R[s] += r * e;

            if (! env.active()) { active = false; break; }
        }

        if (! active) { bank = nullptr; zone = nullptr; }
    }

private:
    static constexpr int kOsc = 7;

    inline float fetch (const float* d, int i, int len) const noexcept
    {
        if (zone->hasLoop() && i >= zone->loopEnd)
            i = zone->loopStart + (i - zone->loopEnd) % (zone->loopEnd - zone->loopStart);
        if (i < 0 || i >= len) return 0.f;
        return d[i];
    }

    float freq = 440.f;
    float phase[kOsc] {};
    bool synthOn = true, sampleOn = false, holdOneShot = false;
    double pos = 0.0, ratio = 1.0;
    SampleBank::Ptr bank;
    const Zone* zone = nullptr;
    Adsr env;
    Svf fl, fr;
};

class Synth
{
public:
    static constexpr int kVoices = 64;

    void prepare (double sampleRate)
    {
        fs = float (sampleRate);
        for (auto& v : voices) v.active = false;
    }

    void setParams (const Params& p) { params = p; }
    void setBank (const SampleBank::Ptr& b) { bank = b; }

    void process (float* L, float* R, int numSamples, const juce::MidiBuffer& midi)
    {
        int pos = 0;
        for (const auto meta : midi)
        {
            const int t = juce::jlimit (pos, numSamples, meta.samplePosition);
            renderVoices (L + pos, R + pos, t - pos);
            pos = t;
            handle (meta.getMessage());
        }
        renderVoices (L + pos, R + pos, numSamples - pos);

        const float g = params.gain;
        for (int i = 0; i < numSamples; ++i) { L[i] *= g; R[i] *= g; }
    }

private:
    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())         noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())   noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) if (v.active) v.stop();
    }

    void noteOn (int note, float vel)
    {
        // Selection de zone : touche + velocite, puis round-robin par touche
        const Zone* zone = nullptr;
        if (params.source >= 1 && bank != nullptr)
        {
            const Zone* matches[16];
            const int vel127 = juce::jlimit (1, 127, int (vel * 127.f + 0.5f));
            const int c = bank->match (note, vel127, matches, 16);
            if (c > 0)
                zone = matches[rrCounter[size_t (note)]++ % uint32_t (c)];
        }
        if (params.source == 1 && zone == nullptr) return;   // rien a jouer

        Voice* target = nullptr;
        for (auto& v : voices) if (! v.active) { target = &v; break; }

        if (target == nullptr)
        {
            float best = -1e9f;
            for (auto& v : voices)
            {
                const float score = (v.released ? 1000.f : 0.f)
                                  + (1.f - v.level()) * 500.f
                                  + float (counter - v.age) * 0.01f;
                if (score > best) { best = score; target = &v; }
            }
        }
        target->start (note, vel, ++counter, params, bank, zone);
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && ! v.released && v.note == note) v.stop();
    }

    void renderVoices (float* L, float* R, int n)
    {
        if (n <= 0) return;
        for (auto& v : voices)
            if (v.active) v.render (L, R, n, params, fs);
    }

    std::array<Voice, kVoices> voices;
    std::array<uint32_t, 128> rrCounter {};
    SampleBank::Ptr bank;
    Params params;
    float fs = 44100.f;
    uint64_t counter = 0;
};
} // namespace aura
