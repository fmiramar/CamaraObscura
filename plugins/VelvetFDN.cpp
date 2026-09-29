// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

#include "reverb/FDNCore.hpp"
#include "reverb/VelvetFIR.hpp"

#include <algorithm>
#include <cstddef>

namespace {

struct VelvetFDN : public Unit {
    creativereverb::ModelBufferState modelBuffer;
    creativereverb::PreparedModelView model;
    creativereverb::FDNLineSpec specs[
        creativereverb::kMaxDelayLines] {};
    creativereverb::VelvetTap inputTaps[
        creativereverb::kMaxDelayLines]
        [creativereverb::kMaxVelvetTaps] {};
    creativereverb::VelvetTap outputTaps[2]
        [creativereverb::kMaxVelvetTaps] {};
    creativereverb::VelvetFIR inputFilters[
        creativereverb::kMaxDelayLines];
    creativereverb::VelvetFIR outputFilters[2];
    creativereverb::FDNCore core;
    creativereverb::DelayBank scatterDelays;
    float* delayStorage { nullptr };
    float* velvetStorage { nullptr };
    float* scatterStorage { nullptr };
    float injections[creativereverb::kMaxDelayLines] {};
    creativereverb::EdgeTrigger resetTrigger;
    int lineCount { 0 };
    int mode { 0 };
    int tapCount { 0 };
    int historySize { 0 };
    bool failed { false };
};

void clearVelvet(VelvetFDN* unit)
{
    unit->core.clear();
    for (int line = 0; line < unit->lineCount; ++line)
        unit->inputFilters[line].clear();
    unit->outputFilters[0].clear();
    unit->outputFilters[1].clear();
    if (unit->mode == 1)
        unit->scatterDelays.clear();
}

void VelvetFDN_next(VelvetFDN* unit, int inNumSamples)
{
    if (unit->failed
        || !unit->modelBuffer.unchanged(unit)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const float freeze = creativereverb::clampValue(
        creativereverb::finiteOr(IN0(5), 0.f), 0.f, 1.f);
    unit->core.updateCoefficients(1.f, 0.f, freeze);
    for (int sample = 0; sample < inNumSamples; ++sample) {
        if (unit->resetTrigger.rising(
                creativereverb::inputAt(
                    unit, 6, sample, 0.f)))
            clearVelvet(unit);
        const float input =
            creativereverb::inputAt(unit, 0, sample, 0.f);
        const float density = creativereverb::clampValue(
            creativereverb::inputAt(
                unit, 3, sample, 1.f),
            0.f, 1.f);
        const float diffusion = creativereverb::clampValue(
            creativereverb::inputAt(
                unit, 4, sample, 1.f),
            0.f, 1.f);
        for (int line = 0; line < unit->lineCount; ++line) {
            unit->injections[line] =
                unit->inputFilters[line].process(
                    input, density, diffusion)
                * unit->specs[line].inputGain;
        }
        float left = 0.f;
        float right = 0.f;
        unit->core.readAndAttenuate(left, right);
        unit->core.mix(1.f);
        if (unit->mode == 1) {
            float* vector = unit->core.feedbackVector();
            creativereverb::hadamard(
                vector, unit->lineCount);
            for (int line = 0; line < unit->lineCount;
                 ++line) {
                const float delayed =
                    unit->scatterDelays.read(line);
                unit->scatterDelays.write(
                    line, vector[line]);
                vector[line] = delayed;
            }
            creativereverb::hadamard(
                vector, unit->lineCount);
        }
        unit->core.write(0.f, unit->injections);
        OUT(0)[sample] =
            unit->outputFilters[0].process(
                left, density, diffusion);
        OUT(1)[sample] =
            unit->outputFilters[1].process(
                right, density, diffusion);
    }
}

bool parseVelvetModel(VelvetFDN* unit)
{
    const float* payload = unit->model.payload();
    const int lineData =
        unit->lineCount * creativereverb::kFDNLineStride;
    const int tapPairs =
        (unit->lineCount + 2) * unit->tapCount;
    const int required =
        1 + lineData + tapPairs * 2
        + (unit->mode == 1 ? unit->lineCount : 0);
    if (unit->model.payloadSize() != required
        || payload[0] < 0.f
        || payload[0]
            > static_cast<float>(
                creativereverb::kPi * 0.5)
        || !creativereverb::readFDNLineSpecs(
            payload + 1, lineData, unit->specs,
            unit->lineCount, 1))
        return false;
    int offset = 1 + lineData;
    for (int line = 0; line < unit->lineCount; ++line) {
        for (int tap = 0; tap < unit->tapCount; ++tap) {
            int delay = 0;
            if (!creativereverb::exactFloatInteger(
                    payload[offset], 0,
                    unit->historySize - 1, delay)
                || !std::isfinite(payload[offset + 1]))
                return false;
            unit->inputTaps[line][tap] = {
                delay, payload[offset + 1]
            };
            offset += 2;
        }
    }
    for (int channel = 0; channel < 2; ++channel) {
        for (int tap = 0; tap < unit->tapCount; ++tap) {
            int delay = 0;
            if (!creativereverb::exactFloatInteger(
                    payload[offset], 0,
                    unit->historySize - 1, delay)
                || !std::isfinite(payload[offset + 1]))
                return false;
            unit->outputTaps[channel][tap] = {
                delay, payload[offset + 1]
            };
            offset += 2;
        }
    }
    return true;
}

void VelvetFDN_Ctor(VelvetFDN* unit)
{
    creativereverb::constructUnit(unit);
    SETCALC(VelvetFDN_next);
    unit->failed = false;
    int bufferNumber = 0;
    int requestedLines = 0;
    const bool initValid =
        creativereverb::readInitInteger(
            unit, 1, 0, 16777215, bufferNumber)
        && creativereverb::readInitInteger(
            unit, 2, 2, creativereverb::kMaxDelayLines,
            requestedLines)
        && unit->mNumOutputs == 2;
    if (!initValid
        || !unit->modelBuffer.capture(unit, bufferNumber)
        || !creativereverb::readPreparedModel(
            unit->modelBuffer.data,
            unit->modelBuffer.samples,
            creativereverb::kVelvetFDNMagic,
            unit->model)
        || !creativereverb::sampleRateMatches(
            unit->model, SAMPLERATE)
        || unit->model.channelCount != 2
        || unit->model.fieldA != requestedLines
        || unit->model.fieldB < 0
        || unit->model.fieldB > 1
        || unit->model.fieldC < 1
        || unit->model.fieldC
            > creativereverb::kMaxVelvetTaps
        || unit->model.fieldD < 2
        || unit->model.fieldD > 65536) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "VelvetFDN: invalid model, sample rate, "
            "or fixed delay count.");
        return;
    }
    unit->lineCount = requestedLines;
    unit->mode = unit->model.fieldB;
    unit->tapCount = unit->model.fieldC;
    unit->historySize = unit->model.fieldD;
    if ((unit->mode == 1
            && !creativereverb::isPowerOfTwo(
                unit->lineCount))
        || !parseVelvetModel(unit)) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "VelvetFDN: invalid sparse or paraunitary "
            "model payload.");
        return;
    }
    const std::size_t delaySamples =
        creativereverb::requiredDelaySamples(
            unit->specs, unit->lineCount);
    const std::size_t velvetSamples =
        static_cast<std::size_t>(unit->lineCount + 2)
        * static_cast<std::size_t>(unit->historySize);
    unit->delayStorage =
        creativereverb::allocateRT<float>(
            unit->mWorld, delaySamples);
    unit->velvetStorage =
        creativereverb::allocateRT<float>(
            unit->mWorld, velvetSamples);
    bool valid = unit->delayStorage
        && unit->velvetStorage
        && unit->core.configure(
            unit->delayStorage, delaySamples,
            unit->specs, unit->lineCount, 1,
            unit->lineCount, unit->model.payload()[0],
            static_cast<float>(SAMPLERATE));
    for (int line = 0; valid && line < unit->lineCount;
         ++line) {
        valid = unit->inputFilters[line].configure(
            unit->velvetStorage
                + static_cast<std::size_t>(line)
                    * unit->historySize,
            unit->historySize, unit->inputTaps[line],
            unit->tapCount);
    }
    for (int channel = 0; valid && channel < 2; ++channel) {
        valid = unit->outputFilters[channel].configure(
            unit->velvetStorage
                + static_cast<std::size_t>(
                    unit->lineCount + channel)
                    * unit->historySize,
            unit->historySize,
            unit->outputTaps[channel], unit->tapCount);
    }
    if (valid && unit->mode == 1) {
        const int scatterOffset =
            1
            + unit->lineCount
                * creativereverb::kFDNLineStride
            + (unit->lineCount + 2)
                * unit->tapCount * 2;
        int lengths[creativereverb::kMaxDelayLines] {};
        std::size_t scatterSamples = 0;
        for (int line = 0; line < unit->lineCount; ++line) {
            valid = valid
                && creativereverb::exactFloatInteger(
                    unit->model.payload()[
                        scatterOffset + line],
                    1, 4096, lengths[line]);
            scatterSamples += static_cast<std::size_t>(
                lengths[line]);
        }
        if (valid) {
            unit->scatterStorage =
                creativereverb::allocateRT<float>(
                    unit->mWorld, scatterSamples);
            valid = unit->scatterStorage
                && unit->scatterDelays.configure(
                    unit->scatterStorage, scatterSamples,
                    lengths, unit->lineCount);
        }
    }
    if (!valid) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "VelvetFDN: real-time allocation or "
            "core configuration failed.");
        return;
    }
    unit->resetTrigger.reset(IN0(6));
    ClearUnitOutputs(unit, 1);
}

void VelvetFDN_Dtor(VelvetFDN* unit)
{
    creativereverb::releaseRT(
        unit->mWorld, unit->delayStorage);
    creativereverb::releaseRT(
        unit->mWorld, unit->velvetStorage);
    creativereverb::releaseRT(
        unit->mWorld, unit->scatterStorage);
    creativereverb::destroyUnit(unit);
}

} // namespace

void registerVelvetFDN()
{
    DefineDtorCantAliasUnit(VelvetFDN);
}
