#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include "Pattern.h"

namespace aura
{
struct PlayParams
{
    bool  on = false;
    int   rate = 3;              // 0 1/4, 1 1/8, 2 1/8T, 3 1/16, 4 1/16T, 5 1/32
    int   length = 16;           // pas (1..kMaxPatSteps)
    float gate = 0.9f;           // echelle de la longueur des notes du motif
    float swing = 0.f;
    bool  latch = false;
    bool  filterZone = true;     // premier etage de la chaine : applique la zone touches / velocite
    int   keyLo = 0, keyHi = 127, velLo = 1, velHi = 127;
};

// Lecteur de motif (un Arp et un Seq par couche). Rejoue un motif de piano roll a partir des
// touches tenues, synchronise sur la position ppq de l'hote (horloge libre sinon).
//  - Arp : row = index dans les touches tenues triees (wrap avec +12 par tour).
//  - Seq : row = transposition en demi-tons appliquee a chaque touche tenue.
class PatternPlayer
{
public:
    enum class Kind { Arp, Seq };

    explicit PatternPlayer (Kind k = Kind::Seq) : kind (k) {}
    void setKind (Kind k) { kind = k; }
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { hardReset(); wasOn = false; }

    int currentStep() const noexcept { return playStep.load (std::memory_order_relaxed); }

    // Retourne true si 'out' doit remplacer 'in' pour l'etage suivant.
    bool process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                  const PlayParams& p, const Pattern& pat,
                  double bpm, bool playing, double ppq, bool havePpq)
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
            out.addEvent (juce::MidiMessage::allNotesOff (1), 0);
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
                if (! p.filterZone || (nn >= p.keyLo && nn <= p.keyHi && v >= p.velLo && v <= p.velHi))
                    noteOn (nn, v, p.latch);
            }
            else if (m.isNoteOff())
            {
                noteOff (m.getNoteNumber(), p.latch);
            }
            else
            {
                if (m.isAllNotesOff() || m.isAllSoundOff()) { nHeld = 0; phys.fill (false); physCount = 0; }
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
                    trigger (t, stepCounter, stepSamples, p, pat);
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
    static constexpr int kMaxPend = 512;

    struct Held { int note = 0; int vel = 0; };
    struct Ev { double t = 0; int note = 0; int vel = 0; bool on = false; };

    void hardReset()
    {
        nHeld = 0; nPend = 0; physCount = 0;
        phys.fill (false);
        active = false; samplesUntilStep = 0.0; stepCounter = 0; lastTriggered = -1; lastPpq = -1.0;
        playStep.store (-1, std::memory_order_relaxed);
    }

    void flush (juce::MidiBuffer& out)
    {
        for (int i = 0; i < nPend; ++i)
            if (! pend[(size_t) i].on) out.addEvent (juce::MidiMessage::noteOff (1, pend[(size_t) i].note), 0);
        nPend = 0;
    }

    void noteOn (int note, int vel, bool latch)
    {
        if (note < 0 || note > 127) return;
        if (latch && physCount == 0) nHeld = 0;          // nouvel accord apres relachement total
        if (! phys[(size_t) note]) { phys[(size_t) note] = true; ++physCount; }
        for (int i = 0; i < nHeld; ++i)
            if (held[(size_t) i].note == note) { held[(size_t) i].vel = vel; return; }
        if (nHeld < kMaxHeld) held[(size_t) nHeld++] = { note, vel };
    }

    void noteOff (int note, bool latch)
    {
        if (note < 0 || note > 127) return;
        if (phys[(size_t) note]) { phys[(size_t) note] = false; --physCount; }
        if (latch) return;
        int w = 0;
        for (int i = 0; i < nHeld; ++i) if (held[(size_t) i].note != note) held[(size_t) w++] = held[(size_t) i];
        nHeld = w;
    }

    void push (double t, int note, int vel, bool on)
    {
        if (nPend >= kMaxPend) return;
        pend[(size_t) nPend++] = { t, note, vel, on };
    }

    void trigger (double t, long long counter, double stepSamples, const PlayParams& p, const Pattern& pat)
    {
        const long long len = std::min (kMaxPatSteps, std::max (1, p.length));
        const int st = (int) (((counter % len) + len) % len);
        playStep.store (st, std::memory_order_relaxed);

        // touches tenues triees (pour l'arp)
        std::array<Held, kMaxHeld> sorted {};
        for (int i = 0; i < nHeld; ++i) sorted[(size_t) i] = held[(size_t) i];
        std::sort (sorted.begin(), sorted.begin() + nHeld, [] (const Held& a, const Held& b) { return a.note < b.note; });

        const double swingOff = (st & 1) ? double (p.swing) * 0.5 * stepSamples : 0.0;
        const double gate = std::min (0.98, std::max (0.05, double (p.gate)));

        for (int k = 0; k < pat.n; ++k)
        {
            const PatNote& pn = pat.notes[k];
            if (pn.start != st) continue;

            const double onT  = t + swingOff;
            const double offT = onT + std::max (8.0, double (pn.len) * stepSamples * gate);

            if (kind == Kind::Arp)
            {
                if (pn.row < 0 || nHeld == 0) continue;
                const auto& h = sorted[(size_t) (pn.row % nHeld)];
                const int nn = h.note + 12 * (pn.row / nHeld);
                if (nn > 127) continue;
                const int v = std::clamp ((int) pn.vel * h.vel / 127, 1, 127);
                push (onT, nn, v, true);
                push (offT, nn, 0, false);
            }
            else
            {
                for (int i = 0; i < nHeld; ++i)
                {
                    const int nn = held[(size_t) i].note + pn.row;
                    if (nn < 0 || nn > 127) continue;
                    const int v = std::clamp ((int) pn.vel * held[(size_t) i].vel / 127, 1, 127);
                    push (onT, nn, v, true);
                    push (offT, nn, 0, false);
                }
            }
        }
    }

    Kind kind;
    double fs = 44100.0;
    bool wasOn = false, active = false;
    std::array<Held, kMaxHeld> held {};
    int nHeld = 0;
    std::array<bool, 128> phys {};
    int physCount = 0;

    std::array<Ev, kMaxPend> pend {};
    int nPend = 0;

    double samplesUntilStep = 0.0, lastPpq = -1.0;
    long long stepCounter = 0, lastTriggered = -1;
    std::atomic<int> playStep { -1 };
};
} // namespace aura
