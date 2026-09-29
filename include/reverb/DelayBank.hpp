// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>
#include <cstddef>

namespace creativereverb {

class DelayBank {
public:
    bool configure(
        float* storage, std::size_t storageSamples,
        const int* delayLengths, int lineCount) noexcept
    {
        if (!storage || !delayLengths || lineCount < 1
            || lineCount > kMaxDelayLines)
            return false;
        std::size_t required = 0;
        for (int line = 0; line < lineCount; ++line) {
            if (delayLengths[line] < 1)
                return false;
            offsets_[line] = required;
            lengths_[line] = delayLengths[line];
            positions_[line] = 0;
            required += static_cast<std::size_t>(
                delayLengths[line]);
            if (required > storageSamples)
                return false;
        }
        storage_ = storage;
        storageSamples_ = required;
        lineCount_ = lineCount;
        clear();
        return true;
    }

    void clear() noexcept
    {
        if (storage_)
            std::fill(
                storage_, storage_ + storageSamples_, 0.f);
        std::fill(
            positions_, positions_ + kMaxDelayLines, 0);
    }

    float read(int line) const noexcept
    {
        return storage_[
            offsets_[line]
            + static_cast<std::size_t>(positions_[line])];
    }

    void write(int line, float value) noexcept
    {
        storage_[
            offsets_[line]
            + static_cast<std::size_t>(positions_[line])] =
                safeAudio(value);
        int position = positions_[line] + 1;
        positions_[line] =
            position >= lengths_[line] ? 0 : position;
    }

    int lineCount() const noexcept { return lineCount_; }
    int length(int line) const noexcept
    {
        return lengths_[line];
    }
    std::size_t requiredSamples() const noexcept
    {
        return storageSamples_;
    }

private:
    float* storage_ { nullptr };
    std::size_t storageSamples_ { 0 };
    std::size_t offsets_[kMaxDelayLines] {};
    int lengths_[kMaxDelayLines] {};
    int positions_[kMaxDelayLines] {};
    int lineCount_ { 0 };
};

class FractionalDelayLine {
public:
    bool configure(float* storage, int length) noexcept
    {
        if (!storage || length < 4)
            return false;
        storage_ = storage;
        length_ = length;
        writePosition_ = 0;
        clear();
        return true;
    }

    void clear() noexcept
    {
        if (storage_)
            std::fill(storage_, storage_ + length_, 0.f);
        writePosition_ = 0;
    }

    void write(float value) noexcept
    {
        storage_[writePosition_] = safeAudio(value);
        if (++writePosition_ >= length_)
            writePosition_ = 0;
    }

    float readLinear(float delaySamples) const noexcept
    {
        delaySamples = clampValue(
            finiteOr(delaySamples, 1.f), 1.f,
            static_cast<float>(length_ - 2));
        float position =
            static_cast<float>(writePosition_) - delaySamples;
        while (position < 0.f)
            position += static_cast<float>(length_);
        const int index0 = static_cast<int>(position);
        const int index1 =
            index0 + 1 >= length_ ? 0 : index0 + 1;
        const float fraction = position - static_cast<float>(index0);
        return storage_[index0]
            + (storage_[index1] - storage_[index0])
                * fraction;
    }

private:
    float* storage_ { nullptr };
    int length_ { 0 };
    int writePosition_ { 0 };
};

class FractionalDelayBank {
public:
    bool configure(
        float* storage, std::size_t storageSamples,
        const int* maximumLengths, int lineCount) noexcept
    {
        if (!storage || !maximumLengths || lineCount < 1
            || lineCount > kMaxDelayLines)
            return false;
        std::size_t required = 0;
        for (int line = 0; line < lineCount; ++line) {
            if (maximumLengths[line] < 4)
                return false;
            offsets_[line] = required;
            lengths_[line] = maximumLengths[line];
            positions_[line] = 0;
            required += static_cast<std::size_t>(
                maximumLengths[line]);
            if (required > storageSamples)
                return false;
        }
        storage_ = storage;
        storageSamples_ = required;
        lineCount_ = lineCount;
        clear();
        return true;
    }

    void clear() noexcept
    {
        if (storage_)
            std::fill(
                storage_, storage_ + storageSamples_, 0.f);
        std::fill(
            positions_, positions_ + kMaxDelayLines, 0);
    }

    float readLinear(
        int line, float delaySamples) const noexcept
    {
        const int length = lengths_[line];
        delaySamples = clampValue(
            finiteOr(delaySamples, 1.f), 1.f,
            static_cast<float>(length - 2));
        float position =
            static_cast<float>(positions_[line])
            - delaySamples;
        while (position < 0.f)
            position += static_cast<float>(length);
        const int first = static_cast<int>(position);
        const int second =
            first + 1 >= length ? 0 : first + 1;
        const float fraction =
            position - static_cast<float>(first);
        const std::size_t offset = offsets_[line];
        const std::size_t firstIndex =
            offset + static_cast<std::size_t>(first);
        const std::size_t secondIndex =
            offset + static_cast<std::size_t>(second);
        return storage_[firstIndex]
            + (
                storage_[secondIndex]
                - storage_[firstIndex])
                * fraction;
    }

    void write(int line, float value) noexcept
    {
        storage_[
            offsets_[line]
            + static_cast<std::size_t>(positions_[line])] =
                safeAudio(value);
        if (++positions_[line] >= lengths_[line])
            positions_[line] = 0;
    }

private:
    float* storage_ { nullptr };
    std::size_t storageSamples_ { 0 };
    std::size_t offsets_[kMaxDelayLines] {};
    int lengths_[kMaxDelayLines] {};
    int positions_[kMaxDelayLines] {};
    int lineCount_ { 0 };
};

} // namespace creativereverb
