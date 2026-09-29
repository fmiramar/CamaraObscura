// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>

namespace creativereverb {

struct VelvetTap {
    int delay { 1 };
    float gain { 0.f };
};

class VelvetFIR {
public:
    bool configure(
        float* storage, int storageSize,
        const VelvetTap* taps, int tapCount) noexcept
    {
        if (!storage || storageSize < 2 || !taps
            || tapCount < 1 || tapCount > kMaxVelvetTaps)
            return false;
        for (int index = 0; index < tapCount; ++index) {
            if (taps[index].delay < 0
                || taps[index].delay >= storageSize
                || !std::isfinite(taps[index].gain))
                return false;
            taps_[index] = taps[index];
        }
        storage_ = storage;
        storageSize_ = storageSize;
        tapCount_ = tapCount;
        clear();
        return true;
    }

    void clear() noexcept
    {
        if (storage_)
            std::fill(storage_, storage_ + storageSize_, 0.f);
        writePosition_ = 0;
    }

    float process(
        float input, float density,
        float diffusion) noexcept
    {
        storage_[writePosition_] = safeAudio(input);
        const int active = clampValue(
            static_cast<int>(std::ceil(
                clampValue(density, 0.f, 1.f)
                * static_cast<float>(tapCount_))),
            1, tapCount_);
        float sum = 0.f;
        for (int tap = 0; tap < active; ++tap) {
            int position =
                writePosition_ - taps_[tap].delay;
            if (position < 0)
                position += storageSize_;
            sum += storage_[position] * taps_[tap].gain;
        }
        if (++writePosition_ >= storageSize_)
            writePosition_ = 0;
        const float normalized =
            sum / std::sqrt(static_cast<float>(active));
        return safeAudio(
            input * (1.f - clampValue(diffusion, 0.f, 1.f))
            + normalized
                * clampValue(diffusion, 0.f, 1.f));
    }

private:
    float* storage_ { nullptr };
    int storageSize_ { 0 };
    VelvetTap taps_[kMaxVelvetTaps] {};
    int tapCount_ { 0 };
    int writePosition_ { 0 };
};

} // namespace creativereverb
