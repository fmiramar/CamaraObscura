// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AttenuationFilterBank.hpp"
#include "DelayBank.hpp"
#include "PreparedModel.hpp"
#include "StructuredOrthogonalMatrix.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace creativereverb {

struct FDNLineSpec {
    int delaySamples { 1 };
    int group { 0 };
    float t60Low { 2.f };
    float t60High { 1.f };
    float inputGain { 0.25f };
    float outputLeft { 0.25f };
    float outputRight { 0.25f };
    float dampingFrequency { 4000.f };
};

constexpr int kFDNLineStride = 8;

inline bool readFDNLineSpecs(
    const float* data, int availableSamples,
    FDNLineSpec* specs, int lineCount,
    int groupCount) noexcept
{
    if (!data || !specs || lineCount < 1
        || lineCount > kMaxDelayLines
        || groupCount < 1 || groupCount > kMaxGroups
        || availableSamples < lineCount * kFDNLineStride)
        return false;
    for (int line = 0; line < lineCount; ++line) {
        const float* entry = data + line * kFDNLineStride;
        int delay = 0;
        int group = 0;
        if (!exactFloatInteger(
                entry[0], 1, 4000000, delay)
            || !exactFloatInteger(
                entry[1], 0, groupCount - 1, group)
            || entry[2] < 0.02f || entry[2] > 120.f
            || entry[3] < 0.02f || entry[3] > 120.f
            || !std::isfinite(entry[4])
            || !std::isfinite(entry[5])
            || !std::isfinite(entry[6])
            || entry[7] < 10.f || entry[7] > 100000.f)
            return false;
        specs[line] = {
            delay, group, entry[2], entry[3],
            entry[4], entry[5], entry[6], entry[7]
        };
    }
    return true;
}

class FDNCore {
public:
    bool configure(
        float* delayStorage, std::size_t storageSamples,
        const FDNLineSpec* specs, int lineCount,
        int groupCount, int linesPerGroup,
        float maximumCouplingAngle,
        float sampleRate,
        const float* pairAngles = nullptr,
        int pairAngleCount = 0) noexcept
    {
        if (!delayStorage || !specs
            || lineCount < 1 || lineCount > kMaxDelayLines
            || groupCount < 1 || groupCount > kMaxGroups
            || linesPerGroup < 1
            || groupCount * linesPerGroup != lineCount
            || !std::isfinite(maximumCouplingAngle)
            || sampleRate < 8000.f)
            return false;
        int lengths[kMaxDelayLines] {};
        for (int line = 0; line < lineCount; ++line) {
            if (specs[line].group
                    != line / linesPerGroup)
                return false;
            specs_[line] = specs[line];
            lengths[line] = specs[line].delaySamples;
        }
        if (!delays_.configure(
                delayStorage, storageSamples,
                lengths, lineCount))
            return false;
        lineCount_ = lineCount;
        groupCount_ = groupCount;
        linesPerGroup_ = linesPerGroup;
        maximumCouplingAngle_ = clampValue(
            maximumCouplingAngle, 0.f,
            static_cast<float>(kPi * 0.5));
        pairAngleCount_ =
            groupCount * (groupCount - 1) / 2;
        if (pairAngles) {
            if (pairAngleCount != pairAngleCount_)
                return false;
            for (int pair = 0; pair < pairAngleCount_;
                 ++pair) {
                if (!std::isfinite(pairAngles[pair])
                    || pairAngles[pair] < 0.f
                    || pairAngles[pair]
                        > static_cast<float>(kPi * 0.5))
                    return false;
                pairAngles_[pair] = pairAngles[pair];
            }
        } else {
            std::fill(
                pairAngles_,
                pairAngles_ + pairAngleCount_,
                maximumCouplingAngle_);
        }
        sampleRate_ = sampleRate;
        outputScale_ =
            1.f / std::sqrt(static_cast<float>(lineCount));
        filters_.clear();
        updateCoefficients(1.f, 0.f, 0.f);
        return true;
    }

    void clear() noexcept
    {
        delays_.clear();
        filters_.clear();
        std::fill(
            vector_, vector_ + kMaxDelayLines, 0.f);
    }

    void updateCoefficients(
        float decayScale, float tone,
        float freeze) noexcept
    {
        decayScale = clampValue(
            finiteOr(decayScale, 1.f), 0.05f, 20.f);
        tone = clampValue(finiteOr(tone, 0.f), -2.f, 2.f);
        freeze = clampValue(finiteOr(freeze, 0.f), 0.f, 1.f);
        const float lowFactor = std::pow(2.f, -0.5f * tone);
        const float highFactor = std::pow(2.f, 0.5f * tone);
        constexpr float freezeGain = 0.999999f;
        for (int line = 0; line < lineCount_; ++line) {
            const float lowT60 = std::max(
                0.02f,
                specs_[line].t60Low
                    * decayScale * lowFactor);
            const float highT60 = std::max(
                0.02f,
                specs_[line].t60High
                    * decayScale * highFactor);
            const float low = static_cast<float>(
                delayT60Gain(
                    specs_[line].delaySamples,
                    lowT60, sampleRate_));
            const float high = static_cast<float>(
                delayT60Gain(
                    specs_[line].delaySamples,
                    highT60, sampleRate_));
            lowGains_[line] =
                low + (freezeGain - low) * freeze;
            highGains_[line] =
                high + (freezeGain - high) * freeze;
            poles_[line] = onePoleForFrequency(
                specs_[line].dampingFrequency,
                sampleRate_);
        }
        excitationGain_ = 1.f - freeze;
    }

    void process(
        float input, float coupling,
        float& outputLeft,
        float& outputRight) noexcept
    {
        readAndAttenuate(outputLeft, outputRight);
        mix(coupling);
        write(input, nullptr);
    }

    void readAndAttenuate(
        float& outputLeft,
        float& outputRight) noexcept
    {
        outputLeft = 0.f;
        outputRight = 0.f;
        for (int line = 0; line < lineCount_; ++line) {
            const float delayed = delays_.read(line);
            outputLeft +=
                delayed * specs_[line].outputLeft;
            outputRight +=
                delayed * specs_[line].outputRight;
            vector_[line] = filters_.process(
                line, delayed, lowGains_[line],
                highGains_[line], poles_[line]);
        }
        outputLeft = safeAudio(outputLeft * outputScale_);
        outputRight = safeAudio(outputRight * outputScale_);
    }

    void mix(float coupling) noexcept
    {
        groupedOrthogonalAngles(
            vector_, groupCount_, linesPerGroup_,
            coupling, pairAngles_, pairAngleCount_);
    }

    void write(
        float input,
        const float* perLineInputs) noexcept
    {
        const float safeInput =
            safeAudio(input) * excitationGain_;
        for (int line = 0; line < lineCount_; ++line) {
            const float injection = perLineInputs
                ? perLineInputs[line] * excitationGain_
                : safeInput * specs_[line].inputGain;
            delays_.write(
                line, vector_[line] + injection);
        }
    }

    float* feedbackVector() noexcept { return vector_; }
    int lineCount() const noexcept { return lineCount_; }
    std::size_t delaySamples() const noexcept
    {
        return delays_.requiredSamples();
    }

private:
    DelayBank delays_;
    AttenuationFilterBank filters_;
    FDNLineSpec specs_[kMaxDelayLines] {};
    float vector_[kMaxDelayLines] {};
    float lowGains_[kMaxDelayLines] {};
    float highGains_[kMaxDelayLines] {};
    float poles_[kMaxDelayLines] {};
    int lineCount_ { 0 };
    int groupCount_ { 0 };
    int linesPerGroup_ { 0 };
    float maximumCouplingAngle_ { 0.f };
    float pairAngles_[kMaxGroupPairs] {};
    int pairAngleCount_ { 0 };
    float sampleRate_ { 48000.f };
    float outputScale_ { 1.f };
    float excitationGain_ { 1.f };
};

inline std::size_t requiredDelaySamples(
    const FDNLineSpec* specs, int lineCount) noexcept
{
    if (!specs || lineCount < 1
        || lineCount > kMaxDelayLines)
        return 0;
    std::size_t result = 0;
    for (int line = 0; line < lineCount; ++line)
        result += static_cast<std::size_t>(
            std::max(specs[line].delaySamples, 0));
    return result;
}

} // namespace creativereverb
