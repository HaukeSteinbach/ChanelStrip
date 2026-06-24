#include "InstanceRegistry.h"
#include <cmath>
#include <algorithm>

InstanceRegistry& InstanceRegistry::getInstance() noexcept
{
    static InstanceRegistry registry;
    return registry;
}

int InstanceRegistry::acquireSlot(int groupId) noexcept
{
    for (int i = 0; i < kMaxInstances; ++i)
    {
        bool expected = false;
        if (slots[i].active.compare_exchange_strong(expected, true))
        {
            slots[i].rmsL.store(0.0f);
            slots[i].rmsR.store(0.0f);
            slots[i].groupId.store(groupId);
            slots[i].voltageSag.store(1.0f);
            return i;
        }
    }
    return -1; // kein freier Slot
}

void InstanceRegistry::releaseSlot(int slot) noexcept
{
    if (slot < 0 || slot >= kMaxInstances)
        return;

    slots[slot].rmsL.store(0.0f);
    slots[slot].rmsR.store(0.0f);
    slots[slot].voltageSag.store(1.0f);
    slots[slot].active.store(false); // zuletzt: Slot als frei markieren
}

void InstanceRegistry::setGroup(int slot, int groupId) noexcept
{
    if (slot < 0 || slot >= kMaxInstances)
        return;
    slots[slot].groupId.store(groupId, std::memory_order_relaxed);
}

void InstanceRegistry::writeRMS(int slot, float rmsL, float rmsR) noexcept
{
    if (slot < 0 || slot >= kMaxInstances)
        return;

    slots[slot].rmsL.store(rmsL, std::memory_order_relaxed);
    slots[slot].rmsR.store(rmsR, std::memory_order_relaxed);

    const float peak = std::max(rmsL, rmsR);

    // Voltage Sag: eigener Beitrag (andere lesen ihn via readConsoleState)
    const float sagThis = 1.0f - 0.4f * peak;
    slots[slot].voltageSag.store(std::clamp(sagThis, 0.3f, 1.0f),
                                 std::memory_order_relaxed);

    // Morph-Drive: je lauter dieser Kanal, desto mehr treibt er das
    // Waveshaping der anderen Instanzen an (~30 % max bei Vollpegel)
    const float morphDrive = peak * 0.3f;
    slots[slot].morphDrive.store(std::clamp(morphDrive, 0.0f, 0.3f),
                                 std::memory_order_relaxed);
}

void InstanceRegistry::readConsoleState(int slot, int groupId,
                                        float& outCrosstalk,
                                        float& outSag,
                                        float& outMorphMod) const noexcept
{
    float maxRms     = 0.0f;
    float minSag     = 1.0f;
    float maxMorph   = 0.0f;

    for (int i = 0; i < kMaxInstances; ++i)
    {
        if (i == slot) continue;
        if (!slots[i].active.load(std::memory_order_relaxed)) continue;
        if (slots[i].groupId.load(std::memory_order_relaxed) != groupId) continue;

        const float rL = slots[i].rmsL.load(std::memory_order_relaxed);
        const float rR = slots[i].rmsR.load(std::memory_order_relaxed);
        maxRms   = std::max(maxRms, std::max(rL, rR));
        minSag   = std::min(minSag, slots[i].voltageSag.load(std::memory_order_relaxed));
        maxMorph = std::max(maxMorph, slots[i].morphDrive.load(std::memory_order_relaxed));
    }

    outCrosstalk = maxRms * 0.001f;  // ~-60 dB Übersprechen
    outSag       = minSag;
    outMorphMod  = maxMorph;         // bis zu +30 % extra Morph
}
