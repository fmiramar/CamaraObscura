// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"
#include "PreparedModel.hpp"

#include <algorithm>
#include <cmath>

namespace creativereverb {

struct ModalSpec {
    float frequency { 440.f };
    float t60 { 1.f };
    float inputGain { 1.f };
    float outputLeft { 1.f };
    float outputRight { 1.f };
    float phase { 0.f };
    float phaseCosine { 1.f };
    float phaseSine { 0.f };
};

constexpr int kModalStride = 6;

inline bool readModalSpecs(
    const float* data, int availableSamples,
    ModalSpec* specs, int modeCount,
    float sampleRate) noexcept
{
    if (!data || !specs || modeCount < 1
        || modeCount > kMaxModes
        || availableSamples < modeCount * kModalStride)
        return false;
    for (int mode = 0; mode < modeCount; ++mode) {
        const float* entry = data + mode * kModalStride;
        if (entry[0] <= 0.f
            || entry[0] >= sampleRate * 0.5f
            || entry[1] < 0.005f || entry[1] > 120.f
            || !std::isfinite(entry[2])
            || !std::isfinite(entry[3])
            || !std::isfinite(entry[4])
            || !std::isfinite(entry[5]))
            return false;
        const float phase = entry[5];
        specs[mode] = {
            entry[0], entry[1], entry[2],
            entry[3], entry[4], phase,
            std::cos(phase), std::sin(phase)
        };
    }
    return true;
}

class ModalBank {
public:
    bool configure(
        const ModalSpec* specs, int modeCount,
        float* realState, float* imaginaryState,
        float* realCoefficient, float* imaginaryCoefficient,
        float sampleRate) noexcept
    {
        if (!specs || modeCount < 1
            || modeCount > kMaxModes || !realState
            || !imaginaryState || !realCoefficient
            || !imaginaryCoefficient || sampleRate < 8000.f)
            return false;
        specs_ = specs;
        modeCount_ = modeCount;
        real_ = realState;
        imaginary_ = imaginaryState;
        coefficientReal_ = realCoefficient;
        coefficientImaginary_ = imaginaryCoefficient;
        sampleRate_ = sampleRate;
        outputScale_ =
            1.f / std::sqrt(static_cast<float>(modeCount));
        clear();
        updateCoefficients(1.f, 1.f, 0.f, 0.f);
        return true;
    }

    void clear() noexcept
    {
        if (real_)
            std::fill(real_, real_ + modeCount_, 0.f);
        if (imaginary_)
            std::fill(
                imaginary_, imaginary_ + modeCount_, 0.f);
    }

    void updateCoefficients(
        float pitch, float decayScale,
        float dampingTilt, float dispersion,
        float freeze = 0.f) noexcept
    {
        pitch = clampValue(
            finiteOr(pitch, 1.f), 0.03125f, 16.f);
        decayScale = clampValue(
            finiteOr(decayScale, 1.f), 0.02f, 50.f);
        dampingTilt = clampValue(
            finiteOr(dampingTilt, 0.f), -4.f, 4.f);
        dispersion = clampValue(
            finiteOr(dispersion, 0.f), -1.f, 1.f);
        freeze = clampValue(
            finiteOr(freeze, 0.f), 0.f, 1.f);
        for (int mode = 0; mode < modeCount_; ++mode) {
            const double baseFrequency = specs_[mode].frequency;
            const double normalized =
                baseFrequency / std::max(20.f, sampleRate_ * 0.5f);
            const double warp =
                1.0 + dispersion
                    * normalized * normalized;
            const double frequency = clampValue(
                baseFrequency * pitch * warp,
                0.001, sampleRate_ * 0.495);
            const double tilt =
                std::pow(
                    std::max(baseFrequency / 1000.0, 0.02),
                    -0.25 * dampingTilt);
            const double t60 = std::max(
                0.005,
                specs_[mode].t60 * decayScale * tilt);
            double radius =
                amplitudeT60Radius(t60, sampleRate_);
            radius += (0.9999995 - radius) * freeze;
            radius = std::min(radius, 0.9999995);
            const double angle =
                kTwoPi * frequency / sampleRate_;
            coefficientReal_[mode] =
                static_cast<float>(radius * std::cos(angle));
            coefficientImaginary_[mode] =
                static_cast<float>(radius * std::sin(angle));
        }
        excitationGain_ = 1.f - freeze;
    }

    void process(
        float input, float drive,
        float& outputLeft, float& outputRight) noexcept
    {
        const float safeDrive =
            clampValue(finiteOr(drive, 0.f), 0.f, 16.f);
        float excitation = safeAudio(input);
        if (safeDrive > 1.0e-6f)
            excitation = std::tanh(
                excitation * (1.f + safeDrive))
                / std::tanh(1.f + safeDrive);
        excitation *= excitationGain_;
        double left = 0.0;
        double right = 0.0;
        for (int mode = 0; mode < modeCount_; ++mode) {
            const float oldReal = real_[mode];
            const float oldImaginary = imaginary_[mode];
            float nextReal =
                coefficientReal_[mode] * oldReal
                - coefficientImaginary_[mode] * oldImaginary
                + excitation * specs_[mode].inputGain;
            float nextImaginary =
                coefficientImaginary_[mode] * oldReal
                + coefficientReal_[mode] * oldImaginary;
            nextReal = safeAudio(nextReal);
            nextImaginary = safeAudio(nextImaginary);
            real_[mode] = nextReal;
            imaginary_[mode] = nextImaginary;
            const float modalOutput =
                nextReal * specs_[mode].phaseCosine
                - nextImaginary * specs_[mode].phaseSine;
            left += static_cast<double>(modalOutput)
                * specs_[mode].outputLeft;
            right += static_cast<double>(modalOutput)
                * specs_[mode].outputRight;
        }
        outputLeft =
            safeAudio(static_cast<float>(left) * outputScale_);
        outputRight =
            safeAudio(static_cast<float>(right) * outputScale_);
    }

private:
    const ModalSpec* specs_ { nullptr };
    float* real_ { nullptr };
    float* imaginary_ { nullptr };
    float* coefficientReal_ { nullptr };
    float* coefficientImaginary_ { nullptr };
    int modeCount_ { 0 };
    float sampleRate_ { 48000.f };
    float outputScale_ { 1.f };
    float excitationGain_ { 1.f };
};

} // namespace creativereverb
