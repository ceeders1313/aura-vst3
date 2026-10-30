#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace aura
{
struct ArpParams
{
    bool  on = false;
    int   mode = 0;        // 0 Up, 1 Down, 2 UpDown, 3 Random, 4 As Played, 5 Chord
    int   rate = 3;        // 0 1/4, 1 1/8, 2 1/8T, 3 1/16, 4 1/16T, 5 1/32
    int   octaves = 1;
    float gate = 0.6f;     // 0.05 .. 0.95
    float swing = 0.f;     // 0 .. 1
    bool  latch = false;
};

class Arp
{
public:
    void prepare (double sampleRate) { fs = sampleRate; hardReset(); }

    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                  const ArpParams& p, double bpm, bool playing, double ppq, bool havePpq)
    {
        if (! p.on)
        {
            if (wasOn) { flush (out); hardReset(); }
            wasOn = false;
            for (const auto meta : in) out.addEvent (meta.getMessage(), meta.samplePosition);
            return;
        }
        if (! wasOn)
        {
            out.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            hardReset();
            wasOn = true;
        }

        // 1) entree MIDI
        for (const auto meta : in)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())          noteOn (m.getNoteNumber(), m.getVelocity(), p.latch);
            else if (m.isNoteOff())    noteOff (m.getNoteNumber(), p.latch);
            else
            {
                if (m.isAllNotesOff() || m.isAllSoundOff()) { nHeld = 0; }
                out.addEvent (m, meta.samplePosition);
            }
        }
        if (! p.latch) removeNonPhysical();

        // 2) horloge
        static constexpr double kRates[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double stepBeats = kRates[std::min (5, std::max (0, p.rate))];
        const double spb = fs * 60.0 / std::max (bpm, 20.0);
        const double stepSamples = spb * stepBeats;

        if (nHeld == 0)
        {
            active = false;
        }
        else
        {
            const bool synced = playing && havePpq;
            if (synced)
            {
                if (ppq < lastPpq - 1e-6) lastTriggered = -1000000000LL;     // boucle / saut arriere
                lastPpq = ppq;
                long long idx = (long long) std::floor (ppq / stepBeats + 1e-9);
                double phase = ppq - double (idx) * stepBeats;
                if (phase < 1e-6) { samplesUntilStep = 0.0; stepCounter = idx; }
                else              { samplesUntilStep = (stepBeats - phase) * spb; stepCounter = idx + 1; }
                active = true;
            }
            else if (! active)
            {
                active = true;
                samplesUntilStep = 0.0;
                stepCounter = 0;
                lastTriggered = -1;
            }

            double t = samplesUntilStep;
            while (t < double (numSamples))
            {
                if (stepCounter > lastTriggered)
                {
                    trigger (t, stepCounter, stepSamples, p);
                    lastTriggered = stepCounter;
                }
                ++stepCounter;
                t += stepSamples;
            }
            samplesUntilStep = t - double (numSamples);
        }

        // 3) emission des evenements programmes
        int w = 0;
        for (int i = 0; i < nPend; ++i)
        {
            Ev e = pend[(size_t) i];
            if (e.t < double (numSamples))
            {
                const int pos = std::max (0, (int) e.t);
                out.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel)
                                   : juce::MidiMessage::noteOff (1, e.note), pos);
            }
            else
            {
                e.t -= double (numSamples);
                pend[(size_t) w++] = e;
            }
        }
        nPend = w;
    }

private:
    static constexpr int kMaxHeld = 32;
    static constexpr int kMaxPend = 128;
    static constexpr int kMaxSeq = 128;

    struct Ev { double t = 0; int note = 0; int vel = 0; bool on = false; };

    void hardReset()
    {
        nHeld = 0; nPend = 0; physCount = 0;
        phys.fill (false);
        active = false; samplesUntilStep = 0.0; stepCounter = 0; lastTriggered = -1;
        lastPpq = -1.0; rng = 12345u;
    }

    void flush (juce::MidiBuffer& out)
    {
        for (int i = 0; i < nPend; ++i)
            if (! pend[(size_t) i].on) out.addEvent (juce::MidiMessage::noteOff (1, pend[(size_t) i].note), 0);
        nPend = 0;
    }

    bool inHeld (int note) const
    {
        for (int i = 0; i < nHeld; ++i) if (order[(size_t) i] == note) return true;
        return false;
    }

    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < nHeld; ++i) if (order[(size_t) i] != note) order[(size_t) w++] = order[(size_t) i];
        nHeld = w;
    }

    void removeNonPhysical()
    {
        int w = 0;
        for (int i = 0; i < nHeld; ++i)
            if (phys[(size_t) order[(size_t) i]]) order[(size_t) w++] = order[(size_t) i];
        nHeld = w;
    }

    void noteOn (int note, int vel, bool latch)
    {
        if (note < 0 || note > 127) return;
        if (latch && physCount == 0) nHeld = 0;
        if (! phys[(size_t) note]) { phys[(size_t) note] = true; ++physCount; }
        if (! inHeld (note) && nHeld < kMaxHeld) order[(size_t) nHeld++] = note;
        velArr[(size_t) note] = vel;
    }

    void noteOff (int note, bool latch)
    {
        if (note < 0 || note > 127) return;
        if (phys[(size_t) note]) { phys[(size_t) note] = false; --physCount; }
        if (! latch) removeHeld (note);
    }

    uint32_t nextRand()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return rng;
    }

    void push (double t, int note, int vel, bool on)
    {
        if (nPend >= kMaxPend) return;
        pend[(size_t) nPend++] = { t, note, vel, on };
    }

    static long long mod (long long a, long long m) { return ((a % m) + m) % m; }

    void trigger (double t, long long counter, double stepSamples, const ArpParams& p)
    {
        // sequence : notes (tri croissant sauf As Played) x octaves
        std::array<int, kMaxHeld> base {};
        for (int i = 0; i < nHeld; ++i) base[(size_t) i] = order[(size_t) i];
        if (p.mode != 4) std::sort (base.begin(), base.begin() + nHeld);

        std::array<int, kMaxSeq> seq {}, seqVel {};
        int n = 0;
        for (int o = 0; o < std::max (1, p.octaves); ++o)
            for (int i = 0; i < nHeld && n < kMaxSeq; ++i)
            {
                const int nn = base[(size_t) i] + 12 * o;
                if (nn > 127) continue;
                seq[(size_t) n] = nn;
                seqVel[(size_t) n] = velArr[(size_t) base[(size_t) i]];
                ++n;
            }
        if (n == 0) return;

        const double swingOff = (counter & 1) ? double (p.swing) * 0.5 * stepSamples : 0.0;
        const double onT  = t + swingOff;
        const double offT = onT + std::max (8.0, double (p.gate) * stepSamples);

        if (p.mode == 5)
        {
            for (int k = 0; k < n; ++k) { push (onT, seq[(size_t) k], seqVel[(size_t) k], true);
                                          push (offT, seq[(size_t) k], 0, false); }
            return;
        }

        int idx = 0;
        switch (p.mode)
        {
            case 1:  idx = n - 1 - (int) mod (counter, n); break;
            case 2:
            {
                const int m = std::max (1, 2 * n - 2);
                const int k = (int) mod (counter, m);
                idx = (k < n) ? k : m - k;
                break;
            }
            case 3:  idx = (int) (nextRand() % (uint32_t) n); break;
            default: idx = (int) mod (counter, n); break;
        }
        push (onT, seq[(size_t) idx], seqVel[(size_t) idx], true);
        push (offT, seq[(size_t) idx], 0, false);
    }

    double fs = 44100.0;
    bool wasOn = false, active = false;
    std::array<int, kMaxHeld> order {};
    int nHeld = 0;
    std::array<bool, 128> phys {};
    std::array<int, 128> velArr {};
    int physCount = 0;

    std::array<Ev, kMaxPend> pend {};
    int nPend = 0;

    double samplesUntilStep = 0.0, lastPpq = -1.0;
    long long stepCounter = 0, lastTriggered = -1;
    uint32_t rng = 12345u;
};
} // namespace aura
