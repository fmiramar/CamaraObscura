// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <cstddef>
#include <cstdint>

namespace creativereverb {

constexpr int kModelHeaderSize = 12;
constexpr int kModelVersion = 1;
constexpr int kDarkVelvetMagic = 444001;
constexpr int kGroupedFDNMagic = 444002;
constexpr int kVelvetFDNMagic = 444003;
constexpr int kRIRFDNMagic = 444004;
constexpr int kModalReverbMagic = 444005;
constexpr int kModalPlateMagic = 444006;
constexpr int kGeometryMagic = 444007;

struct PreparedModelView {
    const float* data { nullptr };
    int sampleCount { 0 };
    int magic { 0 };
    int version { 0 };
    int headerSize { 0 };
    int totalSize { 0 };
    double sampleRate { 0.0 };
    int channelCount { 0 };
    int flags { 0 };
    std::uint32_t seed { 0 };
    int fieldA { 0 };
    int fieldB { 0 };
    int fieldC { 0 };
    int fieldD { 0 };

    const float* payload() const noexcept
    {
        return data ? data + headerSize : nullptr;
    }

    int payloadSize() const noexcept
    {
        return totalSize - headerSize;
    }
};

inline bool exactFloatInteger(
    float value, int minimum, int maximum,
    int& result) noexcept
{
    if (!std::isfinite(value))
        return false;
    const int converted = static_cast<int>(value);
    if (static_cast<float>(converted) != value
        || converted < minimum || converted > maximum)
        return false;
    result = converted;
    return true;
}

inline bool readPreparedModel(
    const float* data, int sampleCount, int expectedMagic,
    PreparedModelView& result) noexcept
{
    if (!data || sampleCount < kModelHeaderSize)
        return false;
    PreparedModelView candidate;
    candidate.data = data;
    candidate.sampleCount = sampleCount;
    int seed = 0;
    if (!exactFloatInteger(
            data[0], expectedMagic, expectedMagic,
            candidate.magic)
        || !exactFloatInteger(
            data[1], kModelVersion, kModelVersion,
            candidate.version)
        || !exactFloatInteger(
            data[2], kModelHeaderSize, kModelHeaderSize,
            candidate.headerSize)
        || !exactFloatInteger(
            data[3], kModelHeaderSize, sampleCount,
            candidate.totalSize)
        || candidate.totalSize != sampleCount
        || !std::isfinite(data[4]) || data[4] < 8000.f
        || data[4] > 768000.f
        || !exactFloatInteger(data[5], 1, 64,
            candidate.channelCount)
        || !exactFloatInteger(data[6], 0, 16777215,
            candidate.flags)
        || !exactFloatInteger(data[7], 0, 16777215, seed)
        || !exactFloatInteger(data[8], 0, 16777215,
            candidate.fieldA)
        || !exactFloatInteger(data[9], 0, 16777215,
            candidate.fieldB)
        || !exactFloatInteger(data[10], 0, 16777215,
            candidate.fieldC)
        || !exactFloatInteger(data[11], 0, 16777215,
            candidate.fieldD))
        return false;
    candidate.sampleRate = data[4];
    candidate.seed = static_cast<std::uint32_t>(seed);
    for (int index = candidate.headerSize;
         index < candidate.totalSize; ++index) {
        if (!std::isfinite(data[index]))
            return false;
    }
    result = candidate;
    return true;
}

inline bool sampleRateMatches(
    const PreparedModelView& model, double serverRate,
    double tolerance = 0.5) noexcept
{
    return std::abs(model.sampleRate - serverRate) <= tolerance;
}

} // namespace creativereverb
