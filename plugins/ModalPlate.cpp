// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

#include "reverb/ModalBank.hpp"
#include "reverb/ModalPlateCore.hpp"

#include <cmath>
#include <cstddef>

namespace {

struct ModalPlate : public Unit {
    creativereverb::ModelBufferState modelBuffer;
    creativereverb::PreparedModelView model;
    creativereverb::PlateModeSpec* plateSpecs { nullptr };
    creativereverb::ModalSpec* modalSpecs { nullptr };
    float* realState { nullptr };
    float* imaginaryState { nullptr };
    float* realCoefficient { nullptr };
    float* imaginaryCoefficient { nullptr };
    float* excitationResidues { nullptr };
    float* pickupResidues { nullptr };
    creativereverb::ModalBank bank;
    creativereverb::EdgeTrigger resetTrigger;
    int modeCount { 0 };
    bool failed { false };
};

void updateResidues(ModalPlate* unit, int blockSize)
{
    const float excitationX = creativereverb::clampValue(
        creativereverb::finiteOr(IN0(2), 0.3f), 0.f, 1.f);
    const float excitationY = creativereverb::clampValue(
        creativereverb::finiteOr(IN0(3), 0.4f), 0.f, 1.f);
    const float pickupX = creativereverb::clampValue(
        creativereverb::finiteOr(IN0(4), 0.7f), 0.f, 1.f);
    const float pickupY = creativereverb::clampValue(
        creativereverb::finiteOr(IN0(5), 0.6f), 0.f, 1.f);
    const float smoothing = 1.f - std::exp(
        -static_cast<float>(blockSize)
        / (0.02f * static_cast<float>(SAMPLERATE)));
    const float outputNormalization =
        1.f / std::sqrt(
            static_cast<float>(unit->modeCount));
    for (int mode = 0; mode < unit->modeCount; ++mode) {
        const auto& plate = unit->plateSpecs[mode];
        const float targetExcitation =
            creativereverb::simplySupportedModeShape(
                plate.modeX, plate.modeY,
                excitationX, excitationY)
            * plate.normalization;
        const float targetPickup =
            creativereverb::simplySupportedModeShape(
                plate.modeX, plate.modeY,
                pickupX, pickupY)
            * plate.normalization * outputNormalization;
        unit->excitationResidues[mode] +=
            (targetExcitation
                - unit->excitationResidues[mode])
            * smoothing;
        unit->pickupResidues[mode] +=
            (targetPickup - unit->pickupResidues[mode])
            * smoothing;
        unit->modalSpecs[mode].inputGain =
            unit->excitationResidues[mode];
        unit->modalSpecs[mode].outputLeft =
            unit->pickupResidues[mode];
        unit->modalSpecs[mode].outputRight =
            unit->pickupResidues[mode];
    }
}

void ModalPlate_next(ModalPlate* unit, int inNumSamples)
{
    if (unit->failed
        || !unit->modelBuffer.unchanged(unit)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    updateResidues(unit, inNumSamples);
    unit->bank.updateCoefficients(
        creativereverb::finiteOr(IN0(6), 1.f),
        creativereverb::finiteOr(IN0(7), 1.f),
        0.f, 0.f);
    for (int sample = 0; sample < inNumSamples; ++sample) {
        if (unit->resetTrigger.rising(
                creativereverb::inputAt(
                    unit, 9, sample, 0.f)))
            unit->bank.clear();
        float left = 0.f;
        float right = 0.f;
        unit->bank.process(
            creativereverb::inputAt(unit, 0, sample, 0.f),
            creativereverb::inputAt(unit, 8, sample, 0.f),
            left, right);
        OUT(0)[sample] = left;
    }
}

void ModalPlate_Ctor(ModalPlate* unit)
{
    creativereverb::constructUnit(unit);
    SETCALC(ModalPlate_next);
    unit->failed = false;
    int bufferNumber = 0;
    const bool initValid =
        creativereverb::readInitInteger(
            unit, 1, 0, 16777215, bufferNumber)
        && unit->mNumOutputs == 1;
    if (!initValid
        || !unit->modelBuffer.capture(unit, bufferNumber)
        || !creativereverb::readPreparedModel(
            unit->modelBuffer.data,
            unit->modelBuffer.samples,
            creativereverb::kModalPlateMagic,
            unit->model)
        || !creativereverb::sampleRateMatches(
            unit->model, SAMPLERATE)
        || unit->model.channelCount != 1
        || unit->model.fieldA < 1
        || unit->model.fieldA > creativereverb::kMaxModes
        || unit->model.fieldB
            != creativereverb::kPlateModeStride
        || unit->model.payloadSize()
            != unit->model.fieldA
                * creativereverb::kPlateModeStride) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "ModalPlate: invalid physical model or "
            "sample rate.");
        return;
    }
    unit->modeCount = unit->model.fieldA;
    unit->plateSpecs =
        creativereverb::allocateRT<
            creativereverb::PlateModeSpec>(
            unit->mWorld, unit->modeCount);
    unit->modalSpecs =
        creativereverb::allocateRT<
            creativereverb::ModalSpec>(
            unit->mWorld, unit->modeCount);
    unit->realState =
        creativereverb::allocateRT<float>(
            unit->mWorld, unit->modeCount);
    unit->imaginaryState =
        creativereverb::allocateRT<float>(
            unit->mWorld, unit->modeCount);
    unit->realCoefficient =
        creativereverb::allocateRT<float>(
            unit->mWorld, unit->modeCount);
    unit->imaginaryCoefficient =
        creativereverb::allocateRT<float>(
            unit->mWorld, unit->modeCount);
    unit->excitationResidues =
        creativereverb::allocateRT<float>(
            unit->mWorld, unit->modeCount);
    unit->pickupResidues =
        creativereverb::allocateRT<float>(
            unit->mWorld, unit->modeCount);
    bool valid = unit->plateSpecs && unit->modalSpecs
        && unit->realState && unit->imaginaryState
        && unit->realCoefficient
        && unit->imaginaryCoefficient
        && unit->excitationResidues
        && unit->pickupResidues
        && creativereverb::readPlateModeSpecs(
            unit->model.payload(),
            unit->model.payloadSize(), unit->plateSpecs,
            unit->modeCount,
            static_cast<float>(SAMPLERATE));
    for (int mode = 0; valid && mode < unit->modeCount;
         ++mode) {
        const auto& plate = unit->plateSpecs[mode];
        unit->modalSpecs[mode] = {
            plate.frequency, plate.t60, 0.f,
            0.f, 0.f, 0.f
        };
    }
    valid = valid && unit->bank.configure(
        unit->modalSpecs, unit->modeCount,
        unit->realState, unit->imaginaryState,
        unit->realCoefficient,
        unit->imaginaryCoefficient,
        static_cast<float>(SAMPLERATE));
    if (!valid) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "ModalPlate: real-time allocation or "
            "modal configuration failed.");
        return;
    }
    updateResidues(unit, 64);
    unit->resetTrigger.reset(IN0(9));
    ClearUnitOutputs(unit, 1);
}

void ModalPlate_Dtor(ModalPlate* unit)
{
    creativereverb::releaseRT(
        unit->mWorld, unit->plateSpecs);
    creativereverb::releaseRT(
        unit->mWorld, unit->modalSpecs);
    creativereverb::releaseRT(
        unit->mWorld, unit->realState);
    creativereverb::releaseRT(
        unit->mWorld, unit->imaginaryState);
    creativereverb::releaseRT(
        unit->mWorld, unit->realCoefficient);
    creativereverb::releaseRT(
        unit->mWorld, unit->imaginaryCoefficient);
    creativereverb::releaseRT(
        unit->mWorld, unit->excitationResidues);
    creativereverb::releaseRT(
        unit->mWorld, unit->pickupResidues);
    creativereverb::destroyUnit(unit);
}

} // namespace

void registerModalPlate()
{
    DefineDtorCantAliasUnit(ModalPlate);
}
