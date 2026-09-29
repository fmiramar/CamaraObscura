// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

#include "reverb/ModalBank.hpp"

#include <cstddef>

namespace {

struct ModalReverbBank : public Unit {
    creativereverb::ModelBufferState modelBuffer;
    creativereverb::PreparedModelView model;
    creativereverb::ModalSpec* specs { nullptr };
    float* realState { nullptr };
    float* imaginaryState { nullptr };
    float* realCoefficient { nullptr };
    float* imaginaryCoefficient { nullptr };
    creativereverb::ModalBank bank;
    creativereverb::EdgeTrigger resetTrigger;
    int modeCount { 0 };
    bool failed { false };
};

void ModalReverbBank_next(
    ModalReverbBank* unit, int inNumSamples)
{
    if (unit->failed
        || !unit->modelBuffer.unchanged(unit)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    unit->bank.updateCoefficients(
        creativereverb::finiteOr(IN0(3), 1.f),
        creativereverb::finiteOr(IN0(4), 1.f),
        creativereverb::finiteOr(IN0(5), 0.f),
        creativereverb::finiteOr(IN0(6), 0.f),
        creativereverb::clampValue(
            creativereverb::finiteOr(IN0(8), 0.f),
            0.f, 1.f));
    for (int sample = 0; sample < inNumSamples; ++sample) {
        if (unit->resetTrigger.rising(
                creativereverb::inputAt(
                    unit, 9, sample, 0.f)))
            unit->bank.clear();
        float left = 0.f;
        float right = 0.f;
        unit->bank.process(
            creativereverb::inputAt(unit, 0, sample, 0.f),
            creativereverb::inputAt(unit, 7, sample, 0.f),
            left, right);
        OUT(0)[sample] = left;
        OUT(1)[sample] = right;
    }
}

void ModalReverbBank_Ctor(ModalReverbBank* unit)
{
    creativereverb::constructUnit(unit);
    SETCALC(ModalReverbBank_next);
    unit->failed = false;
    int bufferNumber = 0;
    int requestedModes = 0;
    const bool initValid =
        creativereverb::readInitInteger(
            unit, 1, 0, 16777215, bufferNumber)
        && creativereverb::readInitInteger(
            unit, 2, 1, creativereverb::kMaxModes,
            requestedModes)
        && unit->mNumOutputs == 2;
    if (!initValid
        || !unit->modelBuffer.capture(unit, bufferNumber)
        || !creativereverb::readPreparedModel(
            unit->modelBuffer.data,
            unit->modelBuffer.samples,
            creativereverb::kModalReverbMagic,
            unit->model)
        || !creativereverb::sampleRateMatches(
            unit->model, SAMPLERATE)
        || unit->model.channelCount != 2
        || unit->model.fieldA < requestedModes
        || unit->model.fieldA > creativereverb::kMaxModes
        || unit->model.fieldB
            != creativereverb::kModalStride
        || unit->model.payloadSize()
            != unit->model.fieldA
                * creativereverb::kModalStride) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "ModalReverbBank: invalid model, sample "
            "rate, or fixed mode count.");
        return;
    }
    unit->modeCount = requestedModes;
    unit->specs =
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
    if (!unit->specs || !unit->realState
        || !unit->imaginaryState
        || !unit->realCoefficient
        || !unit->imaginaryCoefficient
        || !creativereverb::readModalSpecs(
            unit->model.payload(),
            unit->model.payloadSize(), unit->specs,
            unit->modeCount,
            static_cast<float>(SAMPLERATE))
        || !unit->bank.configure(
            unit->specs, unit->modeCount,
            unit->realState, unit->imaginaryState,
            unit->realCoefficient,
            unit->imaginaryCoefficient,
            static_cast<float>(SAMPLERATE))) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "ModalReverbBank: real-time allocation "
            "or modal configuration failed.");
        return;
    }
    unit->resetTrigger.reset(IN0(9));
    ClearUnitOutputs(unit, 1);
}

void ModalReverbBank_Dtor(ModalReverbBank* unit)
{
    creativereverb::releaseRT(
        unit->mWorld, unit->specs);
    creativereverb::releaseRT(
        unit->mWorld, unit->realState);
    creativereverb::releaseRT(
        unit->mWorld, unit->imaginaryState);
    creativereverb::releaseRT(
        unit->mWorld, unit->realCoefficient);
    creativereverb::releaseRT(
        unit->mWorld, unit->imaginaryCoefficient);
    creativereverb::destroyUnit(unit);
}

} // namespace

void registerModalReverbBank()
{
    DefineDtorCantAliasUnit(ModalReverbBank);
}
