#pragma once

#include <atomic>
#include <array>
#include <cstdint>

/**
 * Lock-free Singleton-Registry für Instanz-Kommunikation (Analog Console Mode).
 *
 * Audio-Thread-Safety-Regeln:
 *  - Alle Lese-/Schreiboperationen auf Slot-Daten verwenden std::atomic.
 *  - acquireSlot() / releaseSlot() dürfen NUR außerhalb des Audio-Threads
 *    aufgerufen werden (prepareToPlay / Destruktor).
 *  - processBlock() schreibt nur auf den eigenen Slot und liest alle anderen.
 */
class InstanceRegistry
{
public:
    static constexpr int kMaxInstances = 64;

    struct Slot
    {
        std::atomic<bool>  active       { false };
        std::atomic<float> rmsL         { 0.0f  };
        std::atomic<float> rmsR         { 0.0f  };
        std::atomic<int>   groupId      { 0     };
        // Voltage-Sag-Faktor [0,1]: 1 = kein Sag
        std::atomic<float> voltageSag   { 1.0f  };
        // Morph-Modifier: lauteste Instanz der Gruppe treibt Waveshaping
        // anderer Instanzen leicht an (0 = kein Einfluss, ~0.3 = spürbar)
        std::atomic<float> morphDrive   { 0.0f  };
    };

    static InstanceRegistry& getInstance() noexcept;

    /** Gibt Index des zugewiesenen Slots zurück, oder -1 wenn voll. Nicht RT-safe. */
    int  acquireSlot(int groupId) noexcept;

    /** Slot freigeben. Nicht RT-safe. */
    void releaseSlot(int slot) noexcept;

    /** RT-safe: eigenen RMS schreiben. */
    void writeRMS(int slot, float rmsL, float rmsR) noexcept;

    /** RT-safe: Gruppe eines Slots aktualisieren (bei Laufzeit-Änderung der Group-Combo). */
    void setGroup(int slot, int groupId) noexcept;

    /**
     * RT-safe: Crosstalk-Beitrag, Voltage-Sag und Morph-Modifier lesen.
     * @param slot          eigener Slot
     * @param groupId       nur Slots gleicher Gruppe berücksichtigen
     * @param outCrosstalk  gewichteter Fremdpegel [0,1]
     * @param outSag        Voltage-Sag-Faktor [0,1]
     * @param outMorphMod   zusätzlicher Morph durch lauteste Instanz [0,~0.3]
     */
    void readConsoleState(int slot, int groupId,
                          float& outCrosstalk, float& outSag,
                          float& outMorphMod) const noexcept;

private:
    InstanceRegistry() = default;

    std::array<Slot, kMaxInstances> slots;
};
