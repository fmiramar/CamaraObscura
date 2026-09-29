// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"
#include "PreparedModel.hpp"

#include <cmath>

namespace creativereverb {

inline double plateRigidity(
    double thickness, double youngModulus,
    double poissonRatio) noexcept
{
    return youngModulus
        * thickness * thickness * thickness
        / (12.0 * (1.0 - poissonRatio * poissonRatio));
}

inline double simplySupportedPlateFrequency(
    int modeX, int modeY, double width, double height,
    double thickness, double density,
    double youngModulus, double poissonRatio) noexcept
{
    if (modeX < 1 || modeY < 1 || width <= 0.0
        || height <= 0.0 || thickness <= 0.0
        || density <= 0.0 || youngModulus <= 0.0
        || poissonRatio <= -0.999
        || poissonRatio >= 0.499)
        return 0.0;
    const double rigidity = plateRigidity(
        thickness, youngModulus, poissonRatio);
    const double surfaceDensity = density * thickness;
    const double waveFactor =
        std::sqrt(rigidity / surfaceDensity);
    const double spatial =
        static_cast<double>(modeX * modeX)
            / (width * width)
        + static_cast<double>(modeY * modeY)
            / (height * height);
    return (kPi * 0.5) * waveFactor * spatial;
}

inline float simplySupportedModeShape(
    int modeX, int modeY, float x, float y) noexcept
{
    x = clampValue(finiteOr(x, 0.5f), 0.f, 1.f);
    y = clampValue(finiteOr(y, 0.5f), 0.f, 1.f);
    return std::sin(
        static_cast<float>(modeX) * static_cast<float>(kPi) * x)
        * std::sin(
            static_cast<float>(modeY)
            * static_cast<float>(kPi) * y);
}

struct PlateModeSpec {
    int modeX { 1 };
    int modeY { 1 };
    float frequency { 100.f };
    float t60 { 2.f };
    float normalization { 1.f };
};

constexpr int kPlateModeStride = 5;

inline bool readPlateModeSpecs(
    const float* data, int availableSamples,
    PlateModeSpec* specs, int modeCount,
    float sampleRate) noexcept
{
    if (!data || !specs || modeCount < 1
        || modeCount > kMaxModes
        || availableSamples
            < modeCount * kPlateModeStride)
        return false;
    for (int mode = 0; mode < modeCount; ++mode) {
        const float* entry =
            data + mode * kPlateModeStride;
        int modeX = 0;
        int modeY = 0;
        if (!exactFloatInteger(entry[0], 1, 4096, modeX)
            || !exactFloatInteger(entry[1], 1, 4096, modeY)
            || entry[2] <= 0.f
            || entry[2] >= sampleRate * 0.5f
            || entry[3] < 0.005f || entry[3] > 120.f
            || !std::isfinite(entry[4]))
            return false;
        specs[mode] = {
            modeX, modeY, entry[2], entry[3], entry[4]
        };
    }
    return true;
}

} // namespace creativereverb
