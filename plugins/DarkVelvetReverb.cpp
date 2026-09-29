// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

#include "reverb/DarkVelvetCore.hpp"
#include "reverb/PartitionedConvolver.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int kPartitionSize = 4096;
constexpr int kFFTSize = kPartitionSize * 2;
constexpr int kBinCount = kPartitionSize + 1;
constexpr int kMaximumEvents = 250000;

struct DarkVelvetReverb : public Unit {
    creativereverb::ModelBufferState modelBuffer;
    creativereverb::PreparedModelView model;
    creativereverb::DarkSegment segments[256] {};
    creativereverb::UniformPartitionedConvolver convolver;
    creativereverb::ComplexValue* impulseSpectra { nullptr };
    creativereverb::ComplexValue* inputHistory { nullptr };
    float* inputFifo { nullptr };
    float* outputFifo { nullptr };
    float* overlap { nullptr };
    creativereverb::ComplexValue* fftScratch { nullptr };
    creativereverb::ComplexValue* accumulationScratch {
        nullptr
    };
    creativereverb::EdgeTrigger resetTrigger;
    int channelCount { 0 };
    int partitionCount { 0 };
    int durationSamples { 0 };
    bool failed { false };
};

bool allocateConvolver(DarkVelvetReverb* unit)
{
    const std::size_t impulseCount =
        static_cast<std::size_t>(unit->channelCount)
        * static_cast<std::size_t>(unit->partitionCount)
        * kBinCount;
    const std::size_t historyCount =
        static_cast<std::size_t>(unit->partitionCount)
        * kBinCount;
    const std::size_t channelFifoCount =
        static_cast<std::size_t>(unit->channelCount)
        * kPartitionSize;
    unit->impulseSpectra =
        creativereverb::allocateRT<
            creativereverb::ComplexValue>(
            unit->mWorld, impulseCount);
    unit->inputHistory =
        creativereverb::allocateRT<
            creativereverb::ComplexValue>(
            unit->mWorld, historyCount);
    unit->inputFifo =
        creativereverb::allocateRT<float>(
            unit->mWorld, kPartitionSize);
    unit->outputFifo =
        creativereverb::allocateRT<float>(
            unit->mWorld, channelFifoCount);
    unit->overlap =
        creativereverb::allocateRT<float>(
            unit->mWorld, channelFifoCount);
    unit->fftScratch =
        creativereverb::allocateRT<
            creativereverb::ComplexValue>(
            unit->mWorld, kFFTSize);
    unit->accumulationScratch =
        creativereverb::allocateRT<
            creativereverb::ComplexValue>(
            unit->mWorld, kFFTSize);
    return unit->impulseSpectra && unit->inputHistory
        && unit->inputFifo && unit->outputFifo
        && unit->overlap && unit->fftScratch
        && unit->accumulationScratch
        && unit->convolver.configure(
            kPartitionSize, unit->partitionCount,
            unit->channelCount, unit->impulseSpectra,
            unit->inputHistory, unit->inputFifo,
            unit->outputFifo, unit->overlap,
            unit->fftScratch,
            unit->accumulationScratch);
}

bool buildImpulseResponses(
    DarkVelvetReverb* unit, float density,
    float spread, std::uint32_t seed)
{
    const int irSize = unit->durationSamples + 64;
    float* impulse = creativereverb::allocateRT<float>(
        unit->mWorld, static_cast<std::size_t>(irSize));
    if (!impulse)
        return false;

    float filterEnergy[creativereverb::kDarkFilterCount] {};
    for (int filter = 0;
         filter < creativereverb::kDarkFilterCount; ++filter) {
        double energy = 0.0;
        for (int sample = 0; sample < 64; ++sample) {
            const float value =
                creativereverb::dictionaryImpulse(
                    filter, sample,
                    static_cast<float>(SAMPLERATE));
            energy += value * value;
        }
        filterEnergy[filter] = static_cast<float>(
            1.0 / std::sqrt(std::max(energy, 1.0e-12)));
    }

    density = creativereverb::clampValue(
        density, 20.f, 8000.f);
    spread = creativereverb::clampValue(
        spread, 0.f, 1.f);
    const float channelScale =
        0.15f / std::sqrt(
            static_cast<float>(unit->channelCount));
    bool valid = true;

    for (int channel = 0; channel < unit->channelCount;
         ++channel) {
        std::fill(impulse, impulse + irSize, 0.f);
        creativereverb::XorShift32 commonRandom;
        creativereverb::XorShift32 independentRandom;
        commonRandom.seed(seed);
        independentRandom.seed(
            seed
            ^ (
                0x9e3779b9u
                * static_cast<std::uint32_t>(channel + 1)));
        double cursor = 0.0;
        int eventCount = 0;
        while (cursor < unit->durationSamples) {
            const float time =
                static_cast<float>(
                    cursor / SAMPLERATE);
            const auto& segment =
                creativereverb::segmentAt(
                    unit->segments, unit->model.fieldA,
                    time);
            const double localDensity =
                creativereverb::clampValue(
                    static_cast<double>(density)
                        * segment.densityScale,
                    20.0, 8000.0);
            const double grid = SAMPLERATE / localDensity;
            const float commonJitter = commonRandom.uniform();
            const float independentJitter =
                independentRandom.uniform();
            const float jitter =
                commonJitter
                + (independentJitter - commonJitter)
                    * spread;
            const int position = static_cast<int>(
                std::floor(
                    cursor + jitter
                        * std::max(grid - 1.0, 0.0)));
            const int commonSign = commonRandom.sign();
            const int independentSign =
                independentRandom.sign();
            const float sign =
                static_cast<float>(commonSign)
                + (
                    static_cast<float>(independentSign)
                    - static_cast<float>(commonSign))
                    * spread;
            const float commonChoice = commonRandom.uniform();
            const float independentChoice =
                independentRandom.uniform();
            const int filter =
                creativereverb::selectDictionaryFilter(
                    segment,
                    commonChoice
                        + (independentChoice - commonChoice)
                            * spread);
            const float envelope =
                creativereverb::darkEnvelopeAmplitude(
                    segment,
                    static_cast<float>(
                        position / SAMPLERATE));
            const float amplitude =
                sign * envelope
                * static_cast<float>(std::sqrt(grid))
                * channelScale * filterEnergy[filter];
            for (int tap = 0; tap < 64; ++tap) {
                const int sample = position + tap;
                if (sample >= 0 && sample < irSize) {
                    impulse[sample] += amplitude
                        * creativereverb::dictionaryImpulse(
                            filter, tap,
                            static_cast<float>(SAMPLERATE));
                }
            }
            cursor += grid;
            if (++eventCount > kMaximumEvents) {
                valid = false;
                break;
            }
        }
        if (!valid)
            break;
        for (int partition = 0;
             partition < unit->partitionCount; ++partition) {
            const int start = partition * kPartitionSize;
            const int remaining =
                std::max(
                    0, unit->durationSamples - start);
            unit->convolver.setImpulsePartition(
                channel, partition, impulse + start,
                std::min(remaining, kPartitionSize));
        }
    }
    creativereverb::releaseRT(unit->mWorld, impulse);
    return valid;
}

void DarkVelvetReverb_next(
    DarkVelvetReverb* unit, int inNumSamples)
{
    if (unit->failed
        || !unit->modelBuffer.unchanged(unit)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    float outputs[8] {};
    for (int sample = 0; sample < inNumSamples; ++sample) {
        if (unit->resetTrigger.rising(
                creativereverb::inputAt(
                    unit, 6, sample, 0.f))) {
            unit->convolver.clear();
        }
        unit->convolver.processSample(
            creativereverb::inputAt(
                unit, 0, sample, 0.f),
            outputs);
        for (int channel = 0;
             channel < unit->channelCount; ++channel)
            OUT(channel)[sample] = outputs[channel];
    }
}

void DarkVelvetReverb_Ctor(DarkVelvetReverb* unit)
{
    creativereverb::constructUnit(unit);
    SETCALC(DarkVelvetReverb_next);
    unit->failed = false;
    int bufferNumber = 0;
    int seed = 0;
    const bool initValid =
        creativereverb::readInitInteger(
            unit, 1, 0, 16777215, bufferNumber)
        && creativereverb::readInitInteger(
            unit, 2, 1, 8, unit->channelCount)
        && creativereverb::readInitInteger(
            unit, 5, 0, 16777215, seed)
        && static_cast<int>(unit->mNumOutputs)
            == unit->channelCount;
    if (!initValid
        || !unit->modelBuffer.capture(unit, bufferNumber)
        || !creativereverb::readPreparedModel(
            unit->modelBuffer.data,
            unit->modelBuffer.samples,
            creativereverb::kDarkVelvetMagic,
            unit->model)
        || !creativereverb::sampleRateMatches(
            unit->model, SAMPLERATE)
        || unit->model.channelCount < unit->channelCount
        || unit->model.fieldA < 1
        || unit->model.fieldA > 256
        || unit->model.fieldB
            != creativereverb::kDarkFilterCount
        || unit->model.fieldC < 1
        || unit->model.fieldC
            > static_cast<int>(SAMPLERATE * 30.0)
        || !creativereverb::readDarkSegments(
            unit->model.payload(),
            unit->model.payloadSize(),
            unit->segments, unit->model.fieldA,
            static_cast<float>(
                unit->model.fieldC
                / unit->model.sampleRate))) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "DarkVelvetReverb: invalid profile, "
            "sample rate, or fixed argument.");
        return;
    }
    unit->durationSamples = unit->model.fieldC;
    unit->partitionCount =
        (unit->durationSamples + kPartitionSize - 1)
        / kPartitionSize;
    if (!allocateConvolver(unit)
        || !buildImpulseResponses(
            unit,
            creativereverb::finiteOr(IN0(3), 2000.f),
            creativereverb::finiteOr(IN0(4), 1.f),
            static_cast<std::uint32_t>(seed))) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "DarkVelvetReverb: bounded preparation or "
            "real-time allocation failed.");
        return;
    }
    unit->resetTrigger.reset(IN0(6));
    ClearUnitOutputs(unit, 1);
}

void DarkVelvetReverb_Dtor(DarkVelvetReverb* unit)
{
    creativereverb::releaseRT(
        unit->mWorld, unit->impulseSpectra);
    creativereverb::releaseRT(
        unit->mWorld, unit->inputHistory);
    creativereverb::releaseRT(
        unit->mWorld, unit->inputFifo);
    creativereverb::releaseRT(
        unit->mWorld, unit->outputFifo);
    creativereverb::releaseRT(
        unit->mWorld, unit->overlap);
    creativereverb::releaseRT(
        unit->mWorld, unit->fftScratch);
    creativereverb::releaseRT(
        unit->mWorld, unit->accumulationScratch);
    creativereverb::destroyUnit(unit);
}

} // namespace

void registerDarkVelvetReverb()
{
    DefineDtorCantAliasUnit(DarkVelvetReverb);
}
