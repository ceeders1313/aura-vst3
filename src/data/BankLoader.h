#pragma once
#include "engine/SampleBank.h"

namespace aura
{
class BankLoader
{
public:
    // Thread non temps-reel uniquement. Retourne nullptr si rien n'a pu etre charge.
    static SampleBank::Ptr build (const juce::Array<juce::File>& files,
                                  const juce::String& bankName,
                                  juce::String& report);
};
}
