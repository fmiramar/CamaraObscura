// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>
#include <cmath>

namespace creativereverb {

struct TwoStageAttenuationFilter {
    float lowState { 0.f };

    void clear() noexcept { lowState = 0.f; }

    float process(
        float input, float lowGain, float highGain,
        float pole) noexcept
    {
        pole = clampValue(pole, 0.f, 0.99999f);
        lowState =
            (1.f - pole) * input + pole * lowState;
        lowState = flushDenormal(lowState);
        return safeAudio(
            highGain * input
            + (lowGain - highGain) * lowState);
    }
};

inline float onePoleForFrequency(
    float frequency, float sampleRate) noexcept
{
    const float safeFrequency = clampValue(
        finiteOr(frequency, 4000.f), 10.f,
        sampleRate * 0.45f);
    return std::exp(
        -2.f * static_cast<float>(kPi)
        * safeFrequency / sampleRate);
}

class AttenuationFilterBank {
public:
    void clear() noexcept
    {
        for (auto& filter : filters_)
            filter.clear();
    }

    float process(
        int line, float input, float lowGain,
        float highGain, float pole) noexcept
    {
        return filters_[line].process(
            input, lowGain, highGain, pole);
    }

private:
    TwoStageAttenuationFilter filters_[kMaxDelayLines] {};
};

} // namespace creativereverb
