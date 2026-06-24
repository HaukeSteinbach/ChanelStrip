#include "WavetableManager.h"
#include <cmath>
#include <algorithm>
#include <random>

static constexpr float kPi = 3.14159265358979323846f;

WavetableManager::WavetableManager()
{
    buildPreset(0, tableA);
    buildPreset(0, tableB);
}

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

    for (std::size_t r = rows; r < static_cast<std::size_t>(kTableSize); ++r)
        (*stagingTable)[r] = (*stagingTable)[rows - 1];

    commitStagingBuffer();
    activePreset.store(-1);
}

void WavetableManager::applyInstanceVariation(float variationAmount) noexcept
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-variationAmount, variationAmount);

    for (std::size_t r = 0; r < static_cast<std::size_t>(kTableSize); ++r)
        for (std::size_t c = 0; c < static_cast<std::size_t>(kWaveSize); ++c)
        {
            float& val = (*stagingTable)[r][c];
            val = std::clamp(val + dist(rng), -1.0f, 1.0f);
        }

    // Odd-Symmetrie erzwingen nach Variation (f(-x) = -f(x), kein DC)
    for (std::size_t r = 0; r < static_cast<std::size_t>(kTableSize); ++r)
    {
        (*stagingTable)[r][kWaveSize / 2] = 0.0f;
        for (std::size_t c = 0; c < static_cast<std::size_t>(kWaveSize / 2); ++c)
        {
            const std::size_t mirror = static_cast<std::size_t>(kWaveSize) - 1u - c;
            const float avg = ((*stagingTable)[r][mirror] - (*stagingTable)[r][c]) * 0.5f;
            (*stagingTable)[r][mirror] =  avg;
            (*stagingTable)[r][c]      = -avg;
        }
    }

    commitStagingBuffer();
}

float WavetableManager::processSample(float sample, float morphPos) const noexcept
{
    const Table* tbl = activeTable.load(std::memory_order_acquire);
    return lookupInterpolated(*tbl, morphPos, sample);
}

// ── Preset-Builder ────────────────────────────────────────────────────────────
//
// Morph = 0  →  exakt linear (y = x), keinerlei Verzerrung
// Morph = 1  →  sanfte tanh-Saettigung (analog, kein Fold, kein Knirschen)
//
// Preset 0: sehr subtil  (Drive 2.5)  – Röhrenwärme
// Preset 1: medium       (Drive 5.0)  – klarer Charakter
// Preset 2: staerker     (Drive 9.0)  – offensiver, aber glatt
//
void WavetableManager::buildPreset(int presetIndex, Table& target) noexcept
{
    // maxDrive bei Morph=1 (je hoeher, desto mehr Saettigung, aber nie Fold)
    const float maxDrives[kMaxPresets] = { 2.5f, 5.0f, 9.0f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f };
    const float drive     = maxDrives[std::clamp(presetIndex, 0, kMaxPresets - 1)];
    const float tanhDrive = std::tanh(drive);

    for (std::size_t row = 0; row < static_cast<std::size_t>(kTableSize); ++row)
    {
        const float morphFrac = static_cast<float>(row) / static_cast<float>(kTableSize - 1);

        for (std::size_t col = 0; col < static_cast<std::size_t>(kWaveSize); ++col)
        {
            const float x = (static_cast<float>(col) / static_cast<float>(kWaveSize - 1))
                            * 2.0f - 1.0f;

            // Linearer Blend: 0%=clean, 100%=gesaettigt
            // Bei morphFrac=0: y = x (exakt identisch, kein Eingriff)
            // Bei morphFrac=1: y = tanh(x*drive)/tanh(drive)
            const float saturated = (tanhDrive > 1e-6f)
                                    ? std::tanh(x * drive) / tanhDrive
                                    : x;

            target[row][col] = x + morphFrac * (saturated - x);
        }
    }
}

void WavetableManager::commitStagingBuffer() noexcept
{
    Table* oldActive = activeTable.exchange(stagingTable, std::memory_order_acq_rel);
    stagingTable     = oldActive;
}

float WavetableManager::lookupInterpolated(const Table& table,
                                            float morphPos, float x) noexcept
{
    const float rowF = std::clamp(morphPos, 0.0f, 1.0f)
                       * static_cast<float>(kTableSize - 1);
    const auto  row0 = static_cast<std::size_t>(rowF);
    const auto  row1 = std::min(row0 + 1, static_cast<std::size_t>(kTableSize - 1));
    const float rowT = rowF - static_cast<float>(row0);

    const float colF = std::clamp((x + 1.0f) * 0.5f, 0.0f, 1.0f)
                       * static_cast<float>(kWaveSize - 1);
    const auto  col0 = static_cast<std::size_t>(colF);
    const auto  col1 = std::min(col0 + 1, static_cast<std::size_t>(kWaveSize - 1));
    const float colT = colF - static_cast<float>(col0);

    const float v00 = table[row0][col0];
    const float v10 = table[row1][col0];
    const float v01 = table[row0][col1];
    const float v11 = table[row1][col1];

    return (v00 * (1.0f - rowT) + v10 * rowT) * (1.0f - colT)
         + (v01 * (1.0f - rowT) + v11 * rowT) * colT;
}
