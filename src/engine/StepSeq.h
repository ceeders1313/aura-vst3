#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include "ModTypes.h"

namespace aura
{
struct SeqStep
{
    bool  on = true;
    int   pitch = 0;           // -24 .. +24 demi-tons
    float vel = 0.8f;          // 0 .. 1
};

struct SeqParams
{
    bool  on = false;
    int   rate = 3;            // meme table que l'arp : 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
    int   length = kSeqSteps;
    float gate = 0.5f;
    float swing = 0.f;
    int   keyLo = 0, keyHi = 127, velLo = 1, velHi = 127;   // zone de la couche (appliquee aux touches)
    SeqStep steps[kSeqSteps];
};

// Sequenceur pas a pas (un par couche). Les touches tenues sont rejouees a chaque pas actif,
// transposees de la hauteur du pas ; velocite = velocite de la touche x velocite du pas.
// Synchro sur la position ppq de l'hote quand le transport tourne, sinon horloge libre.
class StepSeq
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { hardReset(); wasOn = false; }

    // Pas en cours (-1 si arrete). Lu par l'UI.
    int currentStep() const noexcept { return playStep.load (std::memory_order_relaxed); }

    // Retourne true si 'out' doit remplacer 'in' pour le synthe.
    bool process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                  const SeqParams& p, double bpm, bool playing, double ppq, bool havePpq)
    {
        if (! p.on)
        {
            if (! wasOn) return false;
            flush (out);
            hardReset();
            wasOn = false;
            for (const auto meta : in) out.addEvent (meta.getMessage(), meta.samplePosition);
            return true;
        }
        if (! wasOn)
        {
            out.addEvent (juce::MidiMessage::allNotesOff (1), 0);   // coupe les notes jouees en direct
            hardReset();
            wasOn = true;
        }

        // 1) entree MIDI
        for (const auto meta : in)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const int nn = m.getNoteNumber(), v = m.getVelocity();
                if (nn >= p.keyLo && nn <= p.keyHi && v >= p.velLo && v <= p.velHi) addHeld (nn, v);
            }
            else if (m.isNoteOff())
            {
                removeHeld (m.getNoteNumber());
            }
            else
            {
                if (m.isAllNotesOff() || m.isAllSoundOff()) nHeld = 0;
                out.addEvent (m, meta.samplePosition);
            }
        }

        // 2) horloge
        static constexpr double kRates[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double stepBeats = kRates[std::min (5, std::max (0, p.rate))];
        const double spb = fs * 60.0 / std::max (bpm, 20.0);
        const double stepSamples = spb * stepBeats;

        if (nHeld == 0)
        {
            active = false;
            playStep.store (-1, std::memory_order_relaxed);
        }
        else
        {
            if (playing && havePpq)
            {
                if (ppq < lastPpq - 1e-6) lastTriggered = -1000000000LL;     // boucle / saut arriere
                lastPpq = ppq;
                const long long idx = (long long) std::floor (ppq / stepBeats + 1e-9);
                const double phase = ppq - double (idx) * stepBeats;
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
        return true;
    }

private:
    static constexpr int kMaxHeld = 16;
    static constexpr int kMaxPend = 256;

    struct Held { int note = 0; int vel = 0; };
    struct Ev { double t = 0; int note = 0; int vel = 0; bool on = false; };

    void hardReset()
    {
        nHeld = 0; nPend = 0;
        active = false; samplesUntilStep = 0.0; stepCounter = 0; lastTriggered = -1; lastPpq = -1.0;
        playStep.store (-1, std::memory_order_relaxed);
    }

    void flush (juce::MidiBuffer& out)
    {
        for (int i = 0; i < nPend; ++i)
            if (! pend[(size_t) i].on) out.addEvent (juce::MidiMessage::noteOff (1, pend[(size_t) i].note), 0);
        nPend = 0;
    }

    void addHeld (int note, int vel)
    {
        for (int i = 0; i < nHeld; ++i)
            if (held[(size_t) i].note == note) { held[(size_t) i].vel = vel; return; }
        if (nHeld < kMaxHeld) held[(size_t) nHeld++] = { note, vel };
    }

    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < nHeld; ++i) if (held[(size_t) i].note != note) held[(size_t) w++] = held[(size_t) i];
        nHeld = w;
    }

    void push (double t, int note, int vel, bool on)
    {
        if (nPend >= kMaxPend) return;
        pend[(size_t) nPend++] = { t, note, vel, on };
    }

    void trigger (double t, long long counter, double stepSamples, const SeqParams& p)
    {
        const long long len = std::min (kSeqSteps, std::max (1, p.length));
        const int st = (int) (((counter % len) + len) % len);
        playStep.store (st, std::memory_order_relaxed);

        const SeqStep& s = p.steps[st];
        if (! s.on || s.vel < 0.01f) return;

        const double swingOff = (counter & 1) ? double (p.swing) * 0.5 * stepSamples : 0.0;
        const double onT  = t + swingOff;
        const double offT = onT + std::max (8.0, double (p.gate) * stepSamples);

        for (int i = 0; i < nHeld; ++i)
        {
            const int nn = held[(size_t) i].note + s.pitch;
            if (nn < 0 || nn > 127) continue;
            const int v = std::min (127, std::max (1, int (float (held[(size_t) i].vel) * s.vel + 0.5f)));
            push (onT, nn, v, true);
            push (offT, nn, 0, false);
        }
    }

    double fs = 44100.0;
    bool wasOn = false, active = false;
    std::array<Held, kMaxHeld> held {};
    int nHeld = 0;

    std::array<Ev, kMaxPend> pend {};
    int nPend = 0;

    double samplesUntilStep = 0.0, lastPpq = -1.0;
    long long stepCounter = 0, lastTriggered = -1;
    std::atomic<int> playStep { -1 };
};
} // namespace aura
