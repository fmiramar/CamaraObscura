// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>
#include <cmath>

namespace creativereverb {

constexpr int kDarkFilterCount = 6;
constexpr int kDarkSegmentStride =
    5 + kDarkFilterCount;

struct DarkSegment {
    float startSeconds { 0.f };
    float endSeconds { 1.f };
    float startDb { 0.f };
    float endDb { -60.f };
    float densityScale { 1.f };
    float probabilities[kDarkFilterCount] {
        1.f, 0.f, 0.f, 0.f, 0.f, 0.f
    };
};

inline bool readDarkSegments(
    const float* data, int availableSamples,
    DarkSegment* segments, int segmentCount,
    float duration) noexcept
{
    if (!data || !segments || segmentCount < 1
        || segmentCount > 256
        || availableSamples
            < segmentCount * kDarkSegmentStride)
        return false;
    float previousEnd = 0.f;
    for (int segment = 0; segment < segmentCount;
         ++segment) {
        const float* entry =
            data + segment * kDarkSegmentStride;
        if (entry[0] < 0.f || entry[1] <= entry[0]
            || entry[1] > duration + 1.0e-4f
            || std::abs(entry[0] - previousEnd) > 1.0e-3f
            || entry[4] <= 0.f || entry[4] > 16.f)
            return false;
        DarkSegment result;
        result.startSeconds = entry[0];
        result.endSeconds = entry[1];
        result.startDb = entry[2];
        result.endDb = entry[3];
        result.densityScale = entry[4];
        float sum = 0.f;
        for (int filter = 0;
             filter < kDarkFilterCount; ++filter) {
            const float probability = entry[5 + filter];
            if (probability < 0.f)
                return false;
            result.probabilities[filter] = probability;
            sum += probability;
        }
        if (sum <= 0.f)
            return false;
        for (float& probability : result.probabilities)
            probability /= sum;
        segments[segment] = result;
        previousEnd = entry[1];
    }
    return std::abs(previousEnd - duration) < 1.0e-3f;
}

inline const DarkSegment& segmentAt(
    const DarkSegment* segments, int segmentCount,
    float time) noexcept
{
    int low = 0;
    int high = segmentCount - 1;
    while (low < high) {
        const int middle = (low + high) / 2;
        if (time < segments[middle].endSeconds)
            high = middle;
        else
            low = middle + 1;
    }
    return segments[low];
}

inline float darkEnvelopeAmplitude(
    const DarkSegment& segment, float time) noexcept
{
    const float fraction = clampValue(
        (time - segment.startSeconds)
            / std::max(
                segment.endSeconds - segment.startSeconds,
                1.0e-6f),
        0.f, 1.f);
    const float decibels =
        segment.startDb
        + (segment.endDb - segment.startDb) * fraction;
    return static_cast<float>(dbToAmplitude(decibels));
}

inline int selectDictionaryFilter(
    const DarkSegment& segment, float random) noexcept
{
    float cumulative = 0.f;
    for (int filter = 0;
         filter < kDarkFilterCount; ++filter) {
        cumulative += segment.probabilities[filter];
        if (random <= cumulative)
            return filter;
    }
    return kDarkFilterCount - 1;
}

inline float dictionaryImpulse(
    int filter, int sample, float sampleRate) noexcept
{
    if (sample < 0 || sample >= 64)
        return 0.f;
    const float t = static_cast<float>(sample) / sampleRate;
    switch (filter) {
    case 0:
        return 0.22f * std::exp(-t * 3200.f);
    case 1:
        return sample == 0 ? 1.f
            : -0.24f * std::exp(-t * 9000.f);
    case 2:
        return 0.32f * std::exp(-t * 5000.f)
            * std::sin(
                2.f * static_cast<float>(kPi)
                * 1800.f * t);
    case 3:
        return (sample == 0 ? 0.72f : 0.f)
            + 0.10f * std::exp(-t * 2200.f);
    case 4:
        return (sample == 0 ? 1.1f : 0.f)
            - 0.10f * std::exp(-t * 2200.f);
    default:
        return (sample == 0 ? 0.82f : 0.f)
            + 0.18f * std::exp(-t * 6000.f)
                * std::cos(
                    2.f * static_cast<float>(kPi)
                    * 3600.f * t);
    }
}

} // namespace creativereverb
