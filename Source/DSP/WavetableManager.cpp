#include "WavetableManager.h"
#include <cmath>
#include <algorithm>
#include <random>

// ── Konstanten ────────────────────────────────────────────────────────────────
static constexpr float kPi = 3.14159265358979323846f;

// ── Konstruktor ───────────────────────────────────────────────────────────────
WavetableManager::WavetableManager()
{
    buildPreset(0, tableA);
    buildPreset(0, tableB);
}

// ── Non-RT: Preset laden ──────────────────────────────────────────────────────
void WavetableManager::loadPreset(int presetIndex) noexcept
{
    buildPreset(presetIndex, *stagingTable);
    commitStagingBuffer();
    activePreset.store(presetIndex);
}

void WavetableManager::loadCustomWavetable(const float* data, int numRows, int numCols) noexcept
{
    const auto rows = static_cast<std::size_t>(std::min(numRows, kTableSize));
    const auto cols = static_cast<std::size_t>(std::min(numCols, kWaveSize));

    for (std::size_t r = 0; r < rows; ++r)
        for (std::size_t c = 0; c < cols; ++c)
            (*stagingTable)[r][c] = data[r * static_cast<std::size_t>(numCols) + c];

    // Lücken auffüllen (falls kleinere Tabelle übergeben)
    for (std::size_t r = rows; r < static_cast<std::size_t>(kTableSize); ++r)
        (*stagingTable)[r] = (*stagingTable)[rows - 1];

    commitStagingBuffer();
    activePreset.store(-1); // -1 = Custom
}

void WavetableManager::applyInstanceVariation(float variationAmount) noexcept
{
    // variationAmount ≈ 0.03 (3 %)
    // Verwendet einen deterministischen RNG – nicht im Audio-Thread!
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-variationAmount, variationAmount);

    for (std::size_t r = 0; r < static_cast<std::size_t>(kTableSize); ++r)
        for (std::size_t c = 0; c < static_cast<std::size_t>(kWaveSize); ++c)
        {
            float& val = (*stagingTable)[r][c];
            val = std::clamp(val + dist(rng), -1.0f, 1.0f);
        }

    commitStagingBuffer();
}

// ── RT-safe: Waveshaping ──────────────────────────────────────────────────────
float WavetableManager::processSample(float sample, float morphPos) const noexcept
{
    const Table* tbl = activeTable.load(std::memory_order_acquire);
    return lookupInterpolated(*tbl, morphPos, sample);
}

// ── Privat: Preset-Builder ────────────────────────────────────────────────────
void WavetableManager::buildPreset(int presetIndex, Table& target) noexcept
{
    // Preset 0: linear → soft clip (tanh-Wavefolding)
    // Preset 1: linear → hard fold (sinus-basiert)
    // Preset 2: linear → tube saturation (asymmetrisch)
    // ... weitere Presets können hier ergänzt werden.

    for (std::size_t row = 0; row < static_cast<std::size_t>(kTableSize); ++row)
    {
        const float morphFrac = static_cast<float>(row) / static_cast<float>(kTableSize - 1);

        for (std::size_t col = 0; col < static_cast<std::size_t>(kWaveSize); ++col)
        {
            // x ∈ [-1, +1]
            const float x = (static_cast<float>(col) / static_cast<float>(kWaveSize - 1)) * 2.0f - 1.0f;

            float y = x; // Standard: linear (clean)

            switch (presetIndex)
            {
                case 0: // Soft Clip / Tanh Wavefolding
                {
                    const float drive = 1.0f + morphFrac * 8.0f;
                    y = std::tanh(x * drive) / std::tanh(drive);
                    break;
                }
                case 1: // Sinus Wavefolding
                {
                    const float drive = 1.0f + morphFrac * 3.0f;
                    y = std::sin(x * drive * kPi * 0.5f);
                    break;
                }
                case 2: // Asymmetrische Tube-Sättigung
                {
                    const float drive = 1.0f + morphFrac * 5.0f;
                    if (x >= 0.0f)
                        y = 1.0f - std::exp(-x * drive);
                    else
                        y = -(1.0f - std::exp(x * drive * 0.5f));
                    // Normalisieren
                    const float norm = 1.0f - std::exp(-drive);
                    if (norm > 1e-6f) y /= norm;
                    break;
                }
                default: // Fallback: linear
                    y = x;
                    break;
            }

            target[row][col] = std::clamp(y, -1.0f, 1.0f);
        }
    }
}

void WavetableManager::commitStagingBuffer() noexcept
{
    Table* oldActive   = activeTable.exchange(stagingTable, std::memory_order_acq_rel);
    stagingTable       = oldActive; // alter aktiver Puffer wird neuer Staging
}

// ── Lookup mit bilinearer Interpolation ──────────────────────────────────────
float WavetableManager::lookupInterpolated(const Table& table,
                                            float morphPos, float x) noexcept
{
    // morphPos ∈ [0,1] → Zeilenindex
    const float rowF  = std::clamp(morphPos, 0.0f, 1.0f) * static_cast<float>(kTableSize - 1);
    const auto  row0  = static_cast<std::size_t>(rowF);
    const auto  row1  = std::min(row0 + 1, static_cast<std::size_t>(kTableSize - 1));
    const float rowT  = rowF - static_cast<float>(row0);

    // x ∈ [-1,1] → Spaltenindex
    const float colF  = std::clamp((x + 1.0f) * 0.5f, 0.0f, 1.0f) * static_cast<float>(kWaveSize - 1);
    const auto  col0  = static_cast<std::size_t>(colF);
    const auto  col1  = std::min(col0 + 1, static_cast<std::size_t>(kWaveSize - 1));
    const float colT  = colF - static_cast<float>(col0);

    // Bilineare Interpolation
    const float v00 = table[row0][col0];
    const float v10 = table[row1][col0];
    const float v01 = table[row0][col1];
    const float v11 = table[row1][col1];

    return (v00 * (1.0f - rowT) + v10 * rowT) * (1.0f - colT)
         + (v01 * (1.0f - rowT) + v11 * rowT) * colT;
}
