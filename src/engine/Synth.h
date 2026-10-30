#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "SampleBank.h"
#include "ModTypes.h"
#include "Wavetable.h"

namespace aura
{
constexpr float kPi = 3.14159265358979f;

struct Params
{
    int   source = 0;          // 0 Synth, 1 Sample, 2 Synth+Sample
    bool  oneShot = false;

    int   wave = 0;            // 0 Saw, 1 Square, 2 Wavetable, 3 FM
    int   unison = 5;
    float detune = 0.25f;
    float wtPos = 0.f;

    int   fmAlgo = 0;
    float fmRatio1 = 2.f, fmRatio2 = 3.f, fmIndex = 2.f, fmFeedback = 0.f;

    float cutoff = 8000.f, res = 0.8f;
    float atk = 0.005f, dec = 0.3f, sus = 0.7f, rel = 0.3f;
    float matk = 0.01f, mdec = 0.4f, msus = 0.f, mrel = 0.3f;

    float lfoRate[2] = { 2.f, 0.5f };
    int   lfoShape[2] = { 0, 0 };
    float msegTime[2] = { 1.f, 1.f };
    bool  msegLoop[2] = { false, false };
    float macro[4] = { 0.f, 0.f, 0.f, 0.f };

    ModSlot slots[kSlots];

    // zone de la couche
    int keyLo = 0, keyHi = 127, velLo = 1, velHi = 127, transpose = 0;

    float gain = 1.f;
    float modWheel = 0.f;
    float bend = 0.f;
};

struct RenderCtx
{
    const WavetableBank* wt = nullptr;
    const MsegShape* mseg = nullptr;     // tableau de 2 formes
};

inline float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

inline float lfoValue (int shape, float ph, float sh)
{
    switch (shape)
    {
        case 0: return std::sin (2.f * kPi * ph);
        case 1: return 1.f - 4.f * std::abs (ph - 0.5f);
        case 2: return 2.f * ph - 1.f;
        case 3: return ph < 0.5f ? 1.f : -1.f;
        default: return sh;
    }
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

    float next (float a, float d, float s, float r, float fs)
    {
        switch (stage)
        {
            case Stage::Attack:
                level += 1.f / (std::max (a, 0.001f) * fs);
                if (level >= 1.f) { level = 1.f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
            {
                const float c = 1.f - std::exp (-1.f / (std::max (d, 0.001f) * fs));
                level += (s - level) * c;
                if (std::abs (level - s) < 1e-4f) stage = Stage::Sustain;
                break;
            }
            case Stage::Sustain: level = s; break;
            case Stage::Release:
            {
                const float c = 1.f - std::exp (-1.f / (std::max (r, 0.001f) * fs));
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
    int note = -1;          // note sonore (apres transposition)
    int trigNote = -1;      // note MIDI d'origine (pour le note-off)
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
        if (p.wave == 3) phase[0] = phase[1] = phase[2] = 0.f;
        fbState = 0.f;
        pos = 0.0;

        rng = (0x9E3779B9u ^ uint32_t (a * 2654435761u) ^ uint32_t (n)) | 1u;
        rndVal = nextRnd();
        lfoPh[0] = lfoPh[1] = 0.f;
        lfoSH[0] = nextRnd();
        lfoSH[1] = nextRnd();
        msegPos[0] = msegPos[1] = 0.0;

        fl.reset(); fr.reset();
        env.reset();  env.noteOn();
        menv.reset(); menv.noteOn();
        mlev = 0.f;
    }

    void stop()
    {
        if (holdOneShot) return;
        released = true;
        env.noteOff();
        menv.noteOff();
    }

    void kill() { active = false; bank = nullptr; zone = nullptr; }

    float level() const { return env.level; }

    void render (float* L, float* R, int n, const Params& p, const RenderCtx& ctx, float fs)
    {
        int done = 0;
        while (done < n && active)
        {
            const int m = std::min (kSub, n - done);
            const ModOut mo = computeMod (p, ctx, fs, m);
            renderSub (L + done, R + done, m, p, ctx, fs, mo);
            done += m;
        }
        if (! active) { bank = nullptr; zone = nullptr; }
    }

private:
    static constexpr int kOsc = 7;
    static constexpr int kSub = 32;

    struct ModOut
    {
        float pitch = 0.f, cutoff = 8000.f, res = 0.8f, wt = 0.f,
              fmIdx = 0.f, detune = 0.f, level = 1.f;
    };

    float nextRnd()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float (rng & 0xFFFFFFu) / 8388608.f - 1.f;
    }

    ModOut computeMod (const Params& p, const RenderCtx& ctx, float fs, int m)
    {
        float s[kNumSrc] = {};
        s[SrcModEnv] = mlev;
        s[SrcLfo1]   = lfoValue (p.lfoShape[0], lfoPh[0], lfoSH[0]);
        s[SrcLfo2]   = lfoValue (p.lfoShape[1], lfoPh[1], lfoSH[1]);

        for (int q = 0; q < 2; ++q)
        {
            float t = float (msegPos[q] / std::max (double (p.msegTime[q]), 0.001));
            if (p.msegLoop[q]) t -= std::floor (t); else t = std::min (t, 1.f);
            s[q == 0 ? SrcMseg : SrcMseg2] = (ctx.mseg != nullptr) ? ctx.mseg[q].eval (t) : 0.f;
            msegPos[q] += double (m) / double (fs);
        }

        s[SrcVelocity] = vel;
        s[SrcKey]      = juce::jlimit (-1.f, 1.f, float (note - 60) / 48.f);
        s[SrcModWheel] = p.modWheel;
        s[SrcRandom]   = rndVal;
        for (int k = 0; k < 4; ++k) s[SrcMacro1 + k] = p.macro[k];

        float sum[kNumDst] = {};
        for (const auto& sl : p.slots)
            if (sl.src > 0 && sl.src < kNumSrc && sl.dst > 0 && sl.dst < kNumDst && sl.amount != 0.f)
                sum[sl.dst] += s[sl.src] * sl.amount;

        for (int k = 0; k < 2; ++k)
        {
            lfoPh[k] += p.lfoRate[k] * float (m) / fs;
            if (lfoPh[k] >= 1.f) { lfoPh[k] -= std::floor (lfoPh[k]); lfoSH[k] = nextRnd(); }
        }

        ModOut o;
        o.pitch  = sum[DstPitch] * 24.f + p.bend;
        o.cutoff = p.cutoff * std::exp2 (sum[DstCutoff] * 5.f);
        o.res    = juce::jlimit (0.3f, 20.f, p.res + sum[DstRes] * 5.f);
        o.wt     = juce::jlimit (0.f, 1.f, p.wtPos + sum[DstWtPos]);
        o.fmIdx  = std::max (0.f, p.fmIndex + sum[DstFmIndex] * 8.f);
        o.detune = juce::jlimit (0.f, 1.f, p.detune + sum[DstDetune]);
        o.level  = juce::jlimit (0.f, 2.f, 1.f + sum[DstLevel]);
        return o;
    }

    inline float fetch (const float* d, int i, int len) const noexcept
    {
        if (zone->hasLoop() && i >= zone->loopEnd)
            i = zone->loopStart + (i - zone->loopEnd) % (zone->loopEnd - zone->loopStart);
        if (i < 0 || i >= len) return 0.f;
        return d[i];
    }

    void renderSub (float* L, float* R, int m, const Params& p, const RenderCtx& ctx,
                    float fs, const ModOut& mo)
    {
        const bool isFm = (p.wave == 3);
        const float baseDt = freq * std::exp2 (mo.pitch / 12.f) / fs;
        const int u = isFm ? 1 : juce::jlimit (1, kOsc, p.unison);
        const bool unisonOn = synthOn && ! isFm && ! (p.wave == 2 && ctx.wt == nullptr);
        const bool fmOn = synthOn && isFm && ctx.wt != nullptr;

        float dt[kOsc] {}, gl[kOsc] {}, gr[kOsc] {};
        int lvl[kOsc] {};
        if (unisonOn)
        {
            for (int i = 0; i < u; ++i)
            {
                const float o = (u > 1) ? (2.f * float (i) / float (u - 1) - 1.f) : 0.f;
                dt[i] = baseDt * std::exp2 (o * mo.detune * 0.5f / 12.f);
                const float pan = o * 0.8f;
                gl[i] = std::sqrt (0.5f * (1.f - pan));
                gr[i] = std::sqrt (0.5f * (1.f + pan));
                if (p.wave == 2) lvl[i] = ctx.wt->levelFor (dt[i]);
            }
        }
        const float norm = 0.87f / std::sqrt (float (u));
        const float framePos = mo.wt * float (WavetableBank::kFrames - 1);
        const float fmK = mo.fmIdx * 0.15915f;

        const float* dl = nullptr;
        const float* dr = nullptr;
        int len = 0;
        double inc = 0.0;
        if (sampleOn)
        {
            dl  = zone->data.getReadPointer (0);
            dr  = zone->data.getReadPointer (1);
            len = zone->data.getNumSamples();
            inc = zone->srcRate / double (fs)
                * std::pow (2.0, double (note - zone->rootKey) / 12.0 + double (mo.pitch) / 12.0);
        }

        const float fc = juce::jlimit (20.f, 0.45f * fs, mo.cutoff);
        const float g  = std::tan (kPi * fc / fs);
        const float k  = 1.f / std::max (mo.res, 0.3f);
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        for (int s = 0; s < m; ++s)
        {
            float l = 0.f, r = 0.f;

            if (unisonOn)
            {
                float sl = 0.f, sr = 0.f;
                for (int c = 0; c < u; ++c)
                {
                    float ph = phase[c];
                    float v;
                    if (p.wave == 2)
                    {
                        v = ctx.wt->read (lvl[c], framePos, ph);
                    }
                    else if (p.wave == 1)
                    {
                        v = ph < 0.5f ? 1.f : -1.f;
                        v += polyBlep (ph, dt[c]);
                        float p2 = ph + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
                        v -= polyBlep (p2, dt[c]);
                    }
                    else
                    {
                        v = 2.f * ph - 1.f - polyBlep (ph, dt[c]);
                    }
                    sl += v * gl[c];
                    sr += v * gr[c];
                    ph += dt[c]; if (ph >= 1.f) ph -= 1.f;
                    phase[c] = ph;
                }
                l += sl * norm;
                r += sr * norm;
            }
            else if (fmOn)
            {
                const WavetableBank& w = *ctx.wt;
                const float fbTerm = fbState * p.fmFeedback * 0.25f;
                float m1;
                if (p.fmAlgo == 1)
                {
                    const float o2 = w.sinCycles (phase[2]);
                    const float o1 = w.sinCycles (phase[1] + o2 * fmK + fbTerm);
                    fbState = o1;
                    m1 = o1 * fmK;
                }
                else if (p.fmAlgo == 2)
                {
                    const float o1 = w.sinCycles (phase[1] + fbTerm);
                    fbState = o1;
                    const float o2 = w.sinCycles (phase[2]);
                    m1 = (o1 + o2) * 0.7f * fmK;
                }
                else
                {
                    const float o1 = w.sinCycles (phase[1] + fbTerm);
                    fbState = o1;
                    m1 = o1 * fmK;
                }
                const float out = w.sinCycles (phase[0] + m1) * 0.6f;
                l += out;
                r += out;

                phase[0] += baseDt;
                phase[1] += baseDt * p.fmRatio1;
                phase[2] += baseDt * p.fmRatio2;
                for (int q = 0; q < 3; ++q) if (phase[q] >= 1.f) phase[q] -= std::floor (phase[q]);
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

            const float e = env.next (p.atk, p.dec, p.sus, p.rel, fs) * vel * mo.level;
            mlev = menv.next (p.matk, p.mdec, p.msus, p.mrel, fs);
            L[s] += l * e;
            R[s] += r * e;

            if (! env.active()) { active = false; break; }
        }
    }

    float freq = 440.f;
    float phase[kOsc] {};
    float fbState = 0.f;
    bool synthOn = true, sampleOn = false, holdOneShot = false;
    double pos = 0.0;
    SampleBank::Ptr bank;
    const Zone* zone = nullptr;

    Adsr env, menv;
    float mlev = 0.f;
    float lfoPh[2] {}, lfoSH[2] {};
    double msegPos[2] { 0.0, 0.0 };
    uint32_t rng = 1;
    float rndVal = 0.f;
    Svf fl, fr;
};

class Synth
{
public:
    static constexpr int kVoices = 64;

    Synth() { ctx.mseg = mseg; }

    void prepare (double sampleRate)
    {
        fs = float (sampleRate);
        killAll();
    }

    void killAll() { for (auto& v : voices) v.kill(); }

    void setParams (const Params& p)
    {
        params = p;
        params.modWheel = modWheel;
        params.bend = bend;
    }
    void setBank (const SampleBank::Ptr& b) { bank = b; }
    void setMseg (int idx, const MsegShape& s) { mseg[idx & 1] = s; }
    void setWavetables (const WavetableBank* w) { ctx.wt = w; }

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
        if (g != 1.f)
            for (int i = 0; i < numSamples; ++i) { L[i] *= g; R[i] *= g; }
    }

private:
    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())         noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())   noteOff (m.getNoteNumber());
        else if (m.isController() && m.getControllerNumber() == 1)
        {
            modWheel = float (m.getControllerValue()) / 127.f;
            params.modWheel = modWheel;
        }
        else if (m.isPitchWheel())
        {
            bend = float (m.getPitchWheelValue() - 8192) / 8192.f * 2.f;
            params.bend = bend;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) if (v.active) v.stop();
    }

    void noteOn (int note, float vel)
    {
        const int vel127 = juce::jlimit (1, 127, int (vel * 127.f + 0.5f));
        if (note < params.keyLo || note > params.keyHi) return;
        if (vel127 < params.velLo || vel127 > params.velHi) return;

        const int snote = juce::jlimit (0, 127, note + params.transpose);

        const Zone* zone = nullptr;
        if (params.source >= 1 && bank != nullptr)
        {
            const Zone* matches[16];
            const int c = bank->match (snote, vel127, matches, 16);
            if (c > 0)
                zone = matches[rrCounter[size_t (snote)]++ % uint32_t (c)];
        }
        if (params.source == 1 && zone == nullptr) return;

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
        target->start (snote, vel, ++counter, params, bank, zone);
        target->trigNote = note;
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && ! v.released && v.trigNote == note) v.stop();
    }

    void renderVoices (float* L, float* R, int n)
    {
        if (n <= 0) return;
        for (auto& v : voices)
            if (v.active) v.render (L, R, n, params, ctx, fs);
    }

    std::array<Voice, kVoices> voices;
    std::array<uint32_t, 128> rrCounter {};
    SampleBank::Ptr bank;
    MsegShape mseg[2];
    RenderCtx ctx;
    Params params;
    float modWheel = 0.f, bend = 0.f;
    float fs = 44100.f;
    uint64_t counter = 0;
};
} // namespace aura
