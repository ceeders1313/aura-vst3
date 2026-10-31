#pragma once
#include <atomic>
#include <memory>
#include "FxUnits.h"

namespace aura::fx
{
// Pointeurs vers les parametres APVTS d'une chaine (lus cote audio).
struct ChainPtrs
{
    std::atomic<float>* type[kFxSlots] {};
    std::atomic<float>* on[kFxSlots] {};
    std::atomic<float>* p[kFxSlots][kFxParams] {};
    std::atomic<float>* gain = nullptr;
};

// Chaine d'inserts. Les instances d'effets sont creees a la demande sur le thread message
// (ensure) puis publiees par pointeur atomique : le thread audio n'alloue ni ne detruit jamais.
class FxChain
{
public:
    // thread non audio (verrou externe)
    void ensure (const ChainPtrs& ptrs, double sr, int maxBlock)
    {
        for (int s = 0; s < kFxSlots; ++s)
        {
            const int t = std::clamp (int (ptrs.type[s]->load() + 0.5f), 0, kNumFxTypes - 1);
            auto& sl = slots[s];
            if (t == FxNone || sl.owned[t] != nullptr) continue;
            auto u = createFx (t);
            if (u == nullptr) continue;
            u->prepare (sr, maxBlock);
            sl.inst[t].store (u.get(), std::memory_order_release);
            sl.owned[t] = std::move (u);
        }
    }

    // thread non audio, audio arrete (prepareToPlay)
    void prepareAll (double sr, int maxBlock)
    {
        for (auto& sl : slots)
            for (auto& u : sl.owned)
                if (u != nullptr) u->prepare (sr, maxBlock);
    }

    // thread audio
    void process (float* L, float* R, int n, const ChainPtrs& ptrs, const FxContext& ctx)
    {
        for (int s = 0; s < kFxSlots; ++s)
        {
            auto& sl = slots[s];
            const int t = std::clamp (int (ptrs.type[s]->load() + 0.5f), 0, kNumFxTypes - 1);
            const bool on = ptrs.on[s]->load() > 0.5f;
            FxUnit* u = (t != FxNone && on) ? sl.inst[t].load (std::memory_order_acquire) : nullptr;
            if (u != nullptr)
            {
                if (t != sl.lastType || ! sl.lastOn) u->reset();      // pas de vieille queue au changement
                const auto& def = fxDef (t);
                float real[kFxParams] {};
                for (int i = 0; i < def.numParams; ++i) real[i] = fxToReal (def.p[i], ptrs.p[s][i]->load());
                u->process (L, R, n, real, ctx);
            }
            sl.lastType = t;
            sl.lastOn = (u != nullptr);
        }
    }

    bool isEmpty (const ChainPtrs& ptrs) const
    {
        for (int s = 0; s < kFxSlots; ++s)
            if (ptrs.type[s]->load() > 0.5f && ptrs.on[s]->load() > 0.5f) return false;
        return true;
    }

private:
    struct Slot
    {
        std::unique_ptr<FxUnit> owned[kNumFxTypes];
        std::atomic<FxUnit*> inst[kNumFxTypes] {};
        int lastType = 0;
        bool lastOn = false;
    };
    Slot slots[kFxSlots];
};
} // namespace aura::fx
