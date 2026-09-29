// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

#include "reverb/DelayBank.hpp"
#include "reverb/FDNCore.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

constexpr int kGeometryHeaderFloats = 4;
constexpr int kGeometryPathStride = 7;

struct GeometryReverb : public Unit {
    creativereverb::ModelBufferState modelBuffer;
    creativereverb::PreparedModelView model;
    creativereverb::FDNLineSpec specs[
        creativereverb::kMaxDelayLines] {};
    creativereverb::FDNCore lateCore;
    creativereverb::FractionalDelayLine earlyDelay;
    float* lateStorage { nullptr };
    float* earlyStorage { nullptr };
    float currentDelay[creativereverb::kMaxGeometryPaths] {};
    float currentGain[creativereverb::kMaxGeometryPaths] {};
    float currentPan[creativereverb::kMaxGeometryPaths] {};
    float currentPole[
        creativereverb::kMaxGeometryPaths] {};
    float delayStep[creativereverb::kMaxGeometryPaths] {};
    float gainStep[creativereverb::kMaxGeometryPaths] {};
    float panStep[creativereverb::kMaxGeometryPaths] {};
    float poleStep[creativereverb::kMaxGeometryPaths] {};
    float filterState[creativereverb::kMaxGeometryPaths] {};
    creativereverb::EdgeTrigger resetTrigger;
    const float* gridData { nullptr };
    int gridSize { 0 };
    int gridPointCount { 0 };
    int pathCount { 0 };
    int lineCount { 0 };
    int earlyStorageSize { 0 };
    int lateOffset { 0 };
    float room[3] {};
    bool initializedPaths { false };
    bool failed { false };
};

int gridPointIndex(
    int x, int y, int z, int gridSize) noexcept
{
    return (x * gridSize + y) * gridSize + z;
}

void axisCoordinates(
    float position, float extent, int gridSize,
    int& lower, float& fraction) noexcept
{
    const float normalized = creativereverb::clampValue(
        position / std::max(extent, 1.0e-6f),
        0.f, 1.f);
    const float coordinate =
        normalized * static_cast<float>(gridSize - 1);
    lower = std::min(
        static_cast<int>(std::floor(coordinate)),
        gridSize - 2);
    lower = std::max(lower, 0);
    fraction = coordinate - static_cast<float>(lower);
}

void updatePaths(
    GeometryReverb* unit, int blockSize)
{
    int sourceLower[3] {};
    int listenerLower[3] {};
    float sourceFraction[3] {};
    float listenerFraction[3] {};
    for (int axis = 0; axis < 3; ++axis) {
        axisCoordinates(
            creativereverb::finiteOr(
                IN0(2 + axis), unit->room[axis] * 0.3f),
            unit->room[axis], unit->gridSize,
            sourceLower[axis], sourceFraction[axis]);
        axisCoordinates(
            creativereverb::finiteOr(
                IN0(5 + axis), unit->room[axis] * 0.7f),
            unit->room[axis], unit->gridSize,
            listenerLower[axis], listenerFraction[axis]);
    }
    const float sourceYaw =
        creativereverb::finiteOr(IN0(8), 0.f);
    const float listenerYaw =
        creativereverb::finiteOr(IN0(11), 0.f);
    const float sourceCosine = std::cos(sourceYaw);
    const float sourceSine = std::sin(sourceYaw);
    const float listenerCosine = std::cos(listenerYaw);
    const float listenerSine = std::sin(listenerYaw);

    float targetDelay[creativereverb::kMaxGeometryPaths] {};
    float targetGain[creativereverb::kMaxGeometryPaths] {};
    float targetDirectionX[
        creativereverb::kMaxGeometryPaths] {};
    float targetDirectionY[
        creativereverb::kMaxGeometryPaths] {};
    float targetCutoff[creativereverb::kMaxGeometryPaths] {};

    for (int sourceCorner = 0; sourceCorner < 8;
         ++sourceCorner) {
        int sourceCoordinate[3] {};
        float sourceWeight = 1.f;
        for (int axis = 0; axis < 3; ++axis) {
            const bool high =
                (sourceCorner & (1 << axis)) != 0;
            sourceCoordinate[axis] =
                sourceLower[axis] + (high ? 1 : 0);
            sourceWeight *= high
                ? sourceFraction[axis]
                : 1.f - sourceFraction[axis];
        }
        const int sourceIndex = gridPointIndex(
            sourceCoordinate[0], sourceCoordinate[1],
            sourceCoordinate[2], unit->gridSize);
        for (int listenerCorner = 0;
             listenerCorner < 8; ++listenerCorner) {
            int listenerCoordinate[3] {};
            float listenerWeight = 1.f;
            for (int axis = 0; axis < 3; ++axis) {
                const bool high =
                    (listenerCorner & (1 << axis)) != 0;
                listenerCoordinate[axis] =
                    listenerLower[axis] + (high ? 1 : 0);
                listenerWeight *= high
                    ? listenerFraction[axis]
                    : 1.f - listenerFraction[axis];
            }
            const int listenerIndex = gridPointIndex(
                listenerCoordinate[0],
                listenerCoordinate[1],
                listenerCoordinate[2],
                unit->gridSize);
            const float weight =
                sourceWeight * listenerWeight;
            const int combination =
                sourceIndex * unit->gridPointCount
                + listenerIndex;
            for (int path = 0; path < unit->pathCount;
                 ++path) {
                const float* entry =
                    unit->gridData
                    + (
                        static_cast<std::size_t>(combination)
                            * unit->pathCount
                        + path)
                        * kGeometryPathStride;
                targetDelay[path] +=
                    weight * entry[0]
                    * static_cast<float>(SAMPLERATE);
                targetGain[path] += weight * entry[1];
                targetDirectionX[path] += weight * entry[2];
                targetDirectionY[path] += weight * entry[3];
                targetCutoff[path] += weight * entry[4];
            }
        }
    }
    for (int path = 0; path < unit->pathCount; ++path) {
        const float sourceFacing =
            targetDirectionX[path] * sourceCosine
            + targetDirectionY[path] * sourceSine;
        const float directivity =
            0.5f + 0.5f * std::max(sourceFacing, 0.f);
        const float pan = creativereverb::clampValue(
            targetDirectionX[path] * listenerCosine
                + targetDirectionY[path] * listenerSine,
            -1.f, 1.f);
        const float targetPole =
            creativereverb::onePoleForFrequency(
                targetCutoff[path],
                static_cast<float>(SAMPLERATE));
        targetGain[path] *= directivity;
        if (!unit->initializedPaths) {
            unit->currentDelay[path] = targetDelay[path];
            unit->currentGain[path] = targetGain[path];
            unit->currentPan[path] = pan;
            unit->currentPole[path] = targetPole;
        }
        const float reciprocal =
            1.f / static_cast<float>(std::max(blockSize, 1));
        unit->delayStep[path] =
            (targetDelay[path] - unit->currentDelay[path])
            * reciprocal;
        unit->gainStep[path] =
            (targetGain[path] - unit->currentGain[path])
            * reciprocal;
        unit->panStep[path] =
            (pan - unit->currentPan[path]) * reciprocal;
        unit->poleStep[path] =
            (targetPole - unit->currentPole[path])
            * reciprocal;
    }
    unit->initializedPaths = true;
}

void clearGeometry(GeometryReverb* unit)
{
    unit->earlyDelay.clear();
    unit->lateCore.clear();
    std::fill(
        unit->filterState,
        unit->filterState
            + creativereverb::kMaxGeometryPaths,
        0.f);
}

void GeometryReverb_next(
    GeometryReverb* unit, int inNumSamples)
{
    if (unit->failed
        || !unit->modelBuffer.unchanged(unit)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    updatePaths(unit, inNumSamples);
    unit->lateCore.updateCoefficients(1.f, 0.f, 0.f);
    for (int sample = 0; sample < inNumSamples; ++sample) {
        if (unit->resetTrigger.rising(
                creativereverb::inputAt(
                    unit, 16, sample, 0.f)))
            clearGeometry(unit);
        const float input =
            creativereverb::inputAt(unit, 0, sample, 0.f);
        float earlyLeft = 0.f;
        float earlyRight = 0.f;
        for (int path = 0; path < unit->pathCount;
             ++path) {
            unit->currentDelay[path] +=
                unit->delayStep[path];
            unit->currentGain[path] += unit->gainStep[path];
            unit->currentPan[path] += unit->panStep[path];
            unit->currentPole[path] += unit->poleStep[path];
            const float delayed = unit->earlyDelay.readLinear(
                unit->currentDelay[path]);
            const float pole = creativereverb::clampValue(
                unit->currentPole[path], 0.f, 0.99999f);
            unit->filterState[path] =
                (1.f - pole) * delayed
                + pole * unit->filterState[path];
            unit->filterState[path] =
                creativereverb::flushDenormal(
                    unit->filterState[path]);
            const float value =
                unit->filterState[path]
                * unit->currentGain[path];
            const float pan =
                creativereverb::clampValue(
                    unit->currentPan[path], -1.f, 1.f);
            earlyLeft += value
                * std::sqrt(0.5f * (1.f - pan));
            earlyRight += value
                * std::sqrt(0.5f * (1.f + pan));
        }
        // Reading before writing makes readLinear(d) a true
        // d-sample look-back from the current input sample.
        unit->earlyDelay.write(input);
        float lateLeft = 0.f;
        float lateRight = 0.f;
        unit->lateCore.process(
            input, 1.f, lateLeft, lateRight);
        const float earlyLevel =
            creativereverb::inputAt(
                unit, 14, sample, 1.f);
        const float lateLevel =
            creativereverb::inputAt(
                unit, 15, sample, 1.f);
        const float earlyNormalization =
            1.f / std::sqrt(
                static_cast<float>(unit->pathCount));
        OUT(0)[sample] = creativereverb::safeAudio(
            earlyLeft * earlyLevel * earlyNormalization
            + lateLeft * lateLevel);
        OUT(1)[sample] = creativereverb::safeAudio(
            earlyRight * earlyLevel * earlyNormalization
            + lateRight * lateLevel);
    }
}

bool parseGeometryModel(GeometryReverb* unit)
{
    const float* payload = unit->model.payload();
    if (unit->model.payloadSize() < kGeometryHeaderFloats
        || payload[0] <= 0.f || payload[1] <= 0.f
        || payload[2] <= 0.f || payload[3] < 250.f
        || payload[3] > 400.f)
        return false;
    unit->room[0] = payload[0];
    unit->room[1] = payload[1];
    unit->room[2] = payload[2];
    unit->gridPointCount =
        unit->gridSize * unit->gridSize * unit->gridSize;
    const std::size_t combinations =
        static_cast<std::size_t>(unit->gridPointCount)
        * unit->gridPointCount;
    const std::size_t gridSamples =
        combinations
        * static_cast<std::size_t>(unit->pathCount)
        * kGeometryPathStride;
    unit->lateOffset = kGeometryHeaderFloats
        + static_cast<int>(gridSamples);
    const int required = unit->lateOffset + 1
        + unit->lineCount
            * creativereverb::kFDNLineStride;
    if (required != unit->model.payloadSize())
        return false;
    unit->gridData = payload + kGeometryHeaderFloats;
    float maximumDelay = 0.f;
    for (std::size_t sample = 0; sample < gridSamples;
         sample += kGeometryPathStride) {
        const float delay = unit->gridData[sample];
        const float gain = unit->gridData[sample + 1];
        const float directionX = unit->gridData[sample + 2];
        const float directionY = unit->gridData[sample + 3];
        const float cutoff = unit->gridData[sample + 4];
        int pathType = 0;
        int generation = 0;
        if (delay <= 0.f || delay > 4.f
            || !std::isfinite(gain)
            || directionX < -1.001f
            || directionX > 1.001f
            || directionY < -1.001f
            || directionY > 1.001f
            || cutoff < 10.f
            || cutoff > SAMPLERATE * 0.5f
            || !creativereverb::exactFloatInteger(
                unit->gridData[sample + 5],
                0, 1, pathType)
            || !creativereverb::exactFloatInteger(
                unit->gridData[sample + 6],
                0, 1, generation)
            || generation != pathType)
            return false;
        maximumDelay = std::max(maximumDelay, delay);
    }
    unit->earlyStorageSize =
        static_cast<int>(
            std::ceil(maximumDelay * SAMPLERATE))
        + 4;
    const float angle = payload[unit->lateOffset];
    return angle >= 0.f
        && angle
            <= static_cast<float>(
                creativereverb::kPi * 0.5)
        && creativereverb::readFDNLineSpecs(
            payload + unit->lateOffset + 1,
            unit->lineCount
                * creativereverb::kFDNLineStride,
            unit->specs, unit->lineCount, 1);
}

void GeometryReverb_Ctor(GeometryReverb* unit)
{
    creativereverb::constructUnit(unit);
    SETCALC(GeometryReverb_next);
    unit->failed = false;
    int bufferNumber = 0;
    const bool initValid =
        creativereverb::readInitInteger(
            unit, 1, 0, 16777215, bufferNumber)
        && unit->mNumInputs == 17
        && unit->mNumOutputs == 2;
    if (!initValid
        || !unit->modelBuffer.capture(unit, bufferNumber)
        || !creativereverb::readPreparedModel(
            unit->modelBuffer.data,
            unit->modelBuffer.samples,
            creativereverb::kGeometryMagic,
            unit->model)
        || !creativereverb::sampleRateMatches(
            unit->model, SAMPLERATE)
        || unit->model.channelCount != 2
        || unit->model.fieldA < 2
        || unit->model.fieldA > 4
        || unit->model.fieldB < 1
        || unit->model.fieldB
            > creativereverb::kMaxGeometryPaths
        || unit->model.fieldC < 2
        || unit->model.fieldC
            > creativereverb::kMaxDelayLines
        || unit->model.fieldD
            != creativereverb::kFDNLineStride) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "GeometryReverb: invalid prepared scene, "
            "sample rate, or input layout.");
        return;
    }
    unit->gridSize = unit->model.fieldA;
    unit->pathCount = unit->model.fieldB;
    unit->lineCount = unit->model.fieldC;
    if (!parseGeometryModel(unit)) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "GeometryReverb: invalid path grid or "
            "late-field payload.");
        return;
    }
    const std::size_t lateSamples =
        creativereverb::requiredDelaySamples(
            unit->specs, unit->lineCount);
    unit->lateStorage =
        creativereverb::allocateRT<float>(
            unit->mWorld, lateSamples);
    unit->earlyStorage =
        creativereverb::allocateRT<float>(
            unit->mWorld, unit->earlyStorageSize);
    const float angle =
        unit->model.payload()[unit->lateOffset];
    if (!unit->lateStorage || !unit->earlyStorage
        || !unit->earlyDelay.configure(
            unit->earlyStorage, unit->earlyStorageSize)
        || !unit->lateCore.configure(
            unit->lateStorage, lateSamples,
            unit->specs, unit->lineCount, 1,
            unit->lineCount, angle,
            static_cast<float>(SAMPLERATE))) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "GeometryReverb: real-time allocation or "
            "late-core configuration failed.");
        return;
    }
    updatePaths(unit, 1);
    unit->resetTrigger.reset(IN0(16));
    ClearUnitOutputs(unit, 1);
}

void GeometryReverb_Dtor(GeometryReverb* unit)
{
    creativereverb::releaseRT(
        unit->mWorld, unit->lateStorage);
    creativereverb::releaseRT(
        unit->mWorld, unit->earlyStorage);
    creativereverb::destroyUnit(unit);
}

} // namespace

void registerGeometryReverb()
{
    DefineDtorCantAliasUnit(GeometryReverb);
}
