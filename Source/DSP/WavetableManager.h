#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

/**
 * WavetableManager – verwaltet Waveshaping-Kurven für den Preamp.
 *
 * Design-Ziele:
 *  - Null Allokationen im Audio-Thread.
 *  - Tabelle kann außerhalb des Audio-Threads gewechselt werden
 *    (double-buffered, atomarer Zeiger-Swap).
 *  - Gain morpht innerhalb der Tabelle von Index 0 (clean) bis
 *    kTableSize-1 (maximal gefaltet).
 *
 * Die Waveshaping-Funktion:
 *   Für einen Eingangswert x ∈ [-1,+1] und einen Morph-Wert m ∈ [0,1]:
 *     - Berechne die Lesestelle im Wavetable = m * kTableSize (Zeile)
 *     - Berechne den Spaltindex  = (x + 1) / 2 * (kWaveSize - 1)
 *     - Linearer Interpolation zwischen benachbarten Einträgen
 */
class WavetableManager
{
public:
    // Anzahl der Morph-Schritte in der Tabelle
    static constexpr int kTableSize = 64;

    // Samples pro Morph-Schritt (muss Potenz von 2 sein)
    static constexpr int kWaveSize  = 512;

    // Maximale Anzahl Preset-Wavetables
    static constexpr int kMaxPresets = 8;

    WavetableManager();

    // ── Non-RT API (prepareToPlay / User Interaction) ──────────────────────

    /** Lädt ein Preset in den Staging-Buffer. Call from non-audio thread. */
    void loadPreset(int presetIndex) noexcept;

    /**
     * Lädt eine externe Wavetable (normalisierte float-Daten, kTableSize × kWaveSize).
     * Sicheres Kopieren in Staging-Buffer, dann atomarer Swap.
     * Call from non-audio thread.
     */
    void loadCustomWavetable(const float* data, int numRows, int numCols) noexcept;

    /**
     * Erzeugt eine per-Instanz-Variation der aktuellen Tabelle (~3 %).
     * Muss VOR dem ersten Audio-Block aufgerufen werden.
     */
    void applyInstanceVariation(float variationAmount) noexcept;

    // ── RT-safe API ────────────────────────────────────────────────────────

    /**
     * Wendet Waveshaping auf ein einzelnes Sample an.
     * @param sample   Eingangswert (linear, vor Clipping)
     * @param morphPos Morphposition [0..1]: 0=linear, 1=maximal gefaltet
     * @return         geformtes Sample
     */
    float processSample(float sample, float morphPos) const noexcept;

    /** Gibt den aktiven Tabellen-Index zurück (für UI). */
    int getActivePreset() const noexcept { return activePreset.load(); }

private:
    // Festes Speicher-Layout: Zwei Puffer (Double-Buffer)
    using Table = std::array<std::array<float, kWaveSize>, kTableSize>;

    Table tableA;
    Table tableB;

    // Zeiger auf aktive Tabelle (atomic für lock-freien Swap)
    std::atomic<Table*> activeTable { &tableA };
    Table*              stagingTable { &tableB };

    std::atomic<int> activePreset { 0 };

    // Preset-Daten (statisch, kein Heap im Audio-Thread)
    void buildPreset(int presetIndex, Table& target) noexcept;
    void commitStagingBuffer() noexcept;

    // Linearer Lookup mit Interpolation
    static float lookupInterpolated(const Table& table,
                                    float morphPos, float x) noexcept;
};
