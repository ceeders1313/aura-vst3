#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cstdint>

namespace aura
{
constexpr int kMaxPatSteps = 64;

// Note d'un motif (piano roll). start / len en pas ; row = index de note tenue (Arp) ou demi-tons (Seq).
struct PatNote
{
    int16_t start = 0;
    int16_t len = 1;
    int8_t  row = 0;
    uint8_t vel = 100;     // 1..127
};

// Motif POD copiable (echange UI -> audio sans allocation).
struct Pattern
{
    static constexpr int kMax = 256;
    int n = 0;
    PatNote notes[kMax];

    bool add (int start, int len, int row, int vel)
    {
        if (n >= kMax || start < 0 || start >= kMaxPatSteps) return false;
        notes[n].start = (int16_t) start;
        notes[n].len   = (int16_t) std::clamp (len, 1, kMaxPatSteps);
        notes[n].row   = (int8_t) std::clamp (row, -64, 63);
        notes[n].vel   = (uint8_t) std::clamp (vel, 1, 127);
        ++n;
        return true;
    }

    void remove (int i)
    {
        if (i < 0 || i >= n) return;
        for (int k = i; k < n - 1; ++k) notes[k] = notes[k + 1];
        --n;
    }

    juce::String toString() const
    {
        juce::String s (n);
        for (int i = 0; i < n; ++i)
            s << "|" << (int) notes[i].start << "," << (int) notes[i].len << ","
              << (int) notes[i].row << "," << (int) notes[i].vel;
        return s;
    }

    static Pattern fromString (const juce::String& str, const Pattern& fallback)
    {
        const auto parts = juce::StringArray::fromTokens (str, "|", "");
        if (parts.isEmpty()) return fallback;
        const int cnt = parts[0].getIntValue();
        if (cnt < 0 || cnt > kMax || parts.size() != cnt + 1) return fallback;
        Pattern p;
        for (int i = 0; i < cnt; ++i)
        {
            const auto f = juce::StringArray::fromTokens (parts[i + 1], ",", "");
            if (f.size() != 4) return fallback;
            p.add (f[0].getIntValue(), f[1].getIntValue(), f[2].getIntValue(), f[3].getIntValue());
        }
        return p;
    }

    // Arp par defaut : Up sur 4 notes, 16 pas
    static Pattern defaultArp()
    {
        Pattern p;
        for (int i = 0; i < 16; ++i) p.add (i, 1, i % 4, 100);
        return p;
    }

    // Seq par defaut : la touche rejouee sur chaque pas, accent sur les temps
    static Pattern defaultSeq()
    {
        Pattern p;
        for (int i = 0; i < 16; ++i) p.add (i, 1, 0, (i % 4 == 0) ? 120 : 85);
        return p;
    }
};
} // namespace aura
