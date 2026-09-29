// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

#include "reverb/FDNCore.hpp"

#include <cstddef>

namespace {

struct RIRFDN : public Unit {
    creativereverb::ModelBufferState primaryBuffer;
    creativereverb::ModelBufferState morphBuffer;
    creativereverb::PreparedModelView primaryModel;
    creativereverb::PreparedModelView morphModel;
    creativereverb::FDNLineSpec primarySpecs[
        creativereverb::kMaxDelayLines] {};
    creativereverb::FDNLineSpec morphSpecs[
        creativereverb::kMaxDelayLines] {};
    creativereverb::FDNCore primaryCore;
    creativereverb::FDNCore morphCore;
    float* primaryStorage { nullptr };
    float* morphStorage { nullptr };
    creativereverb::EdgeTrigger resetTrigger;
    int lineCount { 0 };
    bool hasMorph { false };
    bool failed { false };
};

bool parseRIRModel(
    const creativereverb::PreparedModelView& model,
    creativereverb::FDNLineSpec* specs) noexcept
{
    return model.channelCount == 2
        && model.fieldA >= 2
        && model.fieldA <= creativereverb::kMaxDelayLines
        && model.fieldB == 1
        && model.fieldC == model.fieldA
        && model.fieldD == creativereverb::kFDNLineStride
        && model.payloadSize()
            == 1
                + model.fieldA
                    * creativereverb::kFDNLineStride
        && model.payload()[0] >= 0.f
        && model.payload()[0]
            <= static_cast<float>(
                creativereverb::kPi * 0.5)
        && creativereverb::readFDNLineSpecs(
            model.payload() + 1,
            model.payloadSize() - 1,
            specs, model.fieldA, 1);
}

void RIRFDN_next(RIRFDN* unit, int inNumSamples)
{
    if (unit->failed
        || !unit->primaryBuffer.unchanged(unit)
        || (
            unit->hasMorph
            && !unit->morphBuffer.unchanged(unit))) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    const float decayScale =
        creativereverb::clampValue(
            creativereverb::finiteOr(IN0(4), 1.f),
            0.05f, 20.f);
    const float tone = creativereverb::clampValue(
        creativereverb::finiteOr(IN0(5), 0.f),
        -2.f, 2.f);
    const float freeze = creativereverb::clampValue(
        creativereverb::finiteOr(IN0(6), 0.f),
        0.f, 1.f);
    unit->primaryCore.updateCoefficients(
        decayScale, tone, freeze);
    if (unit->hasMorph)
        unit->morphCore.updateCoefficients(
            decayScale, tone, freeze);
    for (int sample = 0; sample < inNumSamples; ++sample) {
        if (unit->resetTrigger.rising(
                creativereverb::inputAt(
                    unit, 7, sample, 0.f))) {
            unit->primaryCore.clear();
            if (unit->hasMorph)
                unit->morphCore.clear();
        }
        const float input =
            creativereverb::inputAt(unit, 0, sample, 0.f);
        float primaryLeft = 0.f;
        float primaryRight = 0.f;
        unit->primaryCore.process(
            input, 1.f, primaryLeft, primaryRight);
        if (unit->hasMorph) {
            float morphLeft = 0.f;
            float morphRight = 0.f;
            unit->morphCore.process(
                input, 1.f, morphLeft, morphRight);
            const float amount = creativereverb::clampValue(
                creativereverb::inputAt(
                    unit, 3, sample, 0.f),
                0.f, 1.f);
            primaryLeft +=
                (morphLeft - primaryLeft) * amount;
            primaryRight +=
                (morphRight - primaryRight) * amount;
        }
        OUT(0)[sample] = primaryLeft;
        OUT(1)[sample] = primaryRight;
    }
}

void RIRFDN_Ctor(RIRFDN* unit)
{
    creativereverb::constructUnit(unit);
    SETCALC(RIRFDN_next);
    unit->failed = false;
    int primaryNumber = 0;
    int morphNumber = -1;
    const bool initValid =
        creativereverb::readInitInteger(
            unit, 1, 0, 16777215, primaryNumber)
        && creativereverb::readInitInteger(
            unit, 2, -1, 16777215, morphNumber)
        && unit->mNumOutputs == 2;
    if (!initValid
        || !unit->primaryBuffer.capture(
            unit, primaryNumber)
        || !creativereverb::readPreparedModel(
            unit->primaryBuffer.data,
            unit->primaryBuffer.samples,
            creativereverb::kRIRFDNMagic,
            unit->primaryModel)
        || !creativereverb::sampleRateMatches(
            unit->primaryModel, SAMPLERATE)
        || !parseRIRModel(
            unit->primaryModel, unit->primarySpecs)) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "RIRFDN: invalid primary late-field model "
            "or sample rate.");
        return;
    }
    unit->lineCount = unit->primaryModel.fieldA;
    unit->hasMorph = morphNumber >= 0;
    if (unit->hasMorph
        && (
            !unit->morphBuffer.capture(unit, morphNumber)
            || !creativereverb::readPreparedModel(
                unit->morphBuffer.data,
                unit->morphBuffer.samples,
                creativereverb::kRIRFDNMagic,
                unit->morphModel)
            || !creativereverb::sampleRateMatches(
                unit->morphModel, SAMPLERATE)
            || unit->morphModel.fieldA != unit->lineCount
            || unit->morphModel.fieldB
                != unit->primaryModel.fieldB
            || unit->morphModel.fieldC
                != unit->primaryModel.fieldC
            || unit->morphModel.fieldD
                != unit->primaryModel.fieldD
            || !parseRIRModel(
                unit->morphModel, unit->morphSpecs))) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "RIRFDN: morph model is invalid or "
            "incompatible.");
        return;
    }
    const std::size_t primarySamples =
        creativereverb::requiredDelaySamples(
            unit->primarySpecs, unit->lineCount);
    unit->primaryStorage =
        creativereverb::allocateRT<float>(
            unit->mWorld, primarySamples);
    bool valid = unit->primaryStorage
        && unit->primaryCore.configure(
            unit->primaryStorage, primarySamples,
            unit->primarySpecs, unit->lineCount, 1,
            unit->lineCount,
            unit->primaryModel.payload()[0],
            static_cast<float>(SAMPLERATE));
    if (valid && unit->hasMorph) {
        const std::size_t morphSamples =
            creativereverb::requiredDelaySamples(
                unit->morphSpecs, unit->lineCount);
        unit->morphStorage =
            creativereverb::allocateRT<float>(
                unit->mWorld, morphSamples);
        valid = unit->morphStorage
            && unit->morphCore.configure(
                unit->morphStorage, morphSamples,
                unit->morphSpecs, unit->lineCount, 1,
                unit->lineCount,
                unit->morphModel.payload()[0],
                static_cast<float>(SAMPLERATE));
    }
    if (!valid) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "RIRFDN: real-time allocation or stable "
            "core configuration failed.");
        return;
    }
    unit->resetTrigger.reset(IN0(7));
    ClearUnitOutputs(unit, 1);
}

void RIRFDN_Dtor(RIRFDN* unit)
{
    creativereverb::releaseRT(
        unit->mWorld, unit->primaryStorage);
    creativereverb::releaseRT(
        unit->mWorld, unit->morphStorage);
    creativereverb::destroyUnit(unit);
}

} // namespace

void registerRIRFDN()
{
    DefineDtorCantAliasUnit(RIRFDN);
}
