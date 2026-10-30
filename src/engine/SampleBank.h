#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace aura
{
struct Zone
{
    int lowKey = 0, highKey = 127;
    int lowVel = 1, highVel = 127;
    int rootKey = 60;
    int rr = 1;
    double srcRate = 44100.0;
    int loopStart = -1, loopEnd = -1;   // loopEnd exclusif
    float gain = 1.f;
    juce::String name;
    juce::AudioBuffer<float> data;      // toujours stereo, float

    bool hasLoop() const noexcept { return loopStart >= 0 && loopEnd > loopStart; }
};

// Immuable une fois construite : lisible depuis le thread audio sans verrou.
class SampleBank : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<SampleBank>;

    std::vector<Zone> zones;
    juce::String name;
    size_t bytes = 0;

    // Remplit out[] avec les zones correspondant a (note, velocite 1..127)
    int match (int note, int vel, const Zone** out, int maxOut) const noexcept
    {
        int c = 0;
        for (const auto& z : zones)
        {
            if (note >= z.lowKey && note <= z.highKey && vel >= z.lowVel && vel <= z.highVel)
            {
                out[c++] = &z;
                if (c >= maxOut) break;
            }
        }
        return c;
    }
};
} // namespace aura
