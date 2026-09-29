// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace creativereverb {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kTwoPi = 2.0 * kPi;
constexpr int kMaxDelayLines = 32;
constexpr int kMaxGroups = 8;
constexpr int kMaxGroupPairs =
    kMaxGroups * (kMaxGroups - 1) / 2;
constexpr int kMaxVelvetTaps = 16;
constexpr int kMaxModes = 4096;
constexpr int kMaxGeometryPaths = 32;
constexpr float kDenormal = 1.0e-20f;

template <typename T>
inline T clampValue(T value, T minimum, T maximum) noexcept
{
    return std::max(minimum, std::min(value, maximum));
}

template <typename T>
inline T finiteOr(T value, T fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

inline float flushDenormal(float value) noexcept
{
    return std::abs(value) < kDenormal ? 0.f : value;
}

inline float safeAudio(float value) noexcept
{
    return std::isfinite(value) ? flushDenormal(value) : 0.f;
}

inline bool isPowerOfTwo(int value) noexcept
{
    return value > 0 && (value & (value - 1)) == 0;
}

inline double dbToAmplitude(double decibels) noexcept
{
    return std::pow(10.0, decibels / 20.0);
}

inline double amplitudeT60Radius(
    double t60Seconds, double sampleRate) noexcept
{
    const double safeT60 = std::max(t60Seconds, 1.0e-4);
    const double safeRate = std::max(sampleRate, 1.0);
    return std::pow(10.0, -3.0 / (safeT60 * safeRate));
}

inline double delayT60Gain(
    double delaySamples, double t60Seconds,
    double sampleRate) noexcept
{
    const double safeT60 = std::max(t60Seconds, 1.0e-4);
    const double safeRate = std::max(sampleRate, 1.0);
    return std::pow(
        10.0, -3.0 * std::max(delaySamples, 0.0)
            / (safeT60 * safeRate));
}

class XorShift32 {
public:
    void seed(std::uint32_t value) noexcept
    {
        state_ = value == 0 ? 0x9e3779b9u : value;
    }

    std::uint32_t next() noexcept
    {
        std::uint32_t value = state_;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state_ = value;
        return value;
    }

    float uniform() noexcept
    {
        return static_cast<float>(
            (next() >> 8) * (1.0 / 16777216.0));
    }

    float bipolar() noexcept
    {
        return uniform() * 2.f - 1.f;
    }

    int sign() noexcept
    {
        return (next() & 1u) != 0u ? 1 : -1;
    }

private:
    std::uint32_t state_ { 0x9e3779b9u };
};

class ControlSmoother {
public:
    void reset(float value) noexcept
    {
        current_ = target_ = finiteOr(value, 0.f);
    }

    void setTarget(float value) noexcept
    {
        target_ = finiteOr(value, current_);
    }

    float next(int remainingSamples) noexcept
    {
        if (remainingSamples <= 1) {
            current_ = target_;
            return current_;
        }
        current_ +=
            (target_ - current_)
            / static_cast<float>(remainingSamples);
        return current_;
    }

    float current() const noexcept { return current_; }

private:
    float current_ { 0.f };
    float target_ { 0.f };
};

class EdgeTrigger {
public:
    void reset(float value = 0.f) noexcept
    {
        previous_ = finiteOr(value, 0.f);
    }

    bool rising(float value) noexcept
    {
        value = finiteOr(value, 0.f);
        const bool result = value > 0.f && previous_ <= 0.f;
        previous_ = value;
        return result;
    }

private:
    float previous_ { 0.f };
};

} // namespace creativereverb
