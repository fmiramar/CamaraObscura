// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

#include "reverb/FDNCore.hpp"

#include <cstddef>

namespace {

struct GroupedFDN : public Unit {
    creativereverb::ModelBufferState modelBuffer;
    creativereverb::PreparedModelView model;
    creativereverb::FDNLineSpec specs[
        creativereverb::kMaxDelayLines] {};
    float pairAngles[creativereverb::kMaxGroupPairs] {};
    creativereverb::FDNCore core;
    float* delayStorage { nullptr };
    creativereverb::EdgeTrigger resetTrigger;
    int lineCount { 0 };
    int groupCount { 0 };
    int linesPerGroup { 0 };
    bool failed { false };
};

void GroupedFDN_next(GroupedFDN* unit, int inNumSamples)
{
    if (unit->failed
        || !unit->modelBuffer.unchanged(unit)) {
        unit->failed = true;
        ClearUnitOutputs(unit, inNumSamples);
        return;
    }
    unit->core.updateCoefficients(
        1.f, 0.f,
        creativereverb::clampValue(
            creativereverb::finiteOr(IN0(5), 0.f),
            0.f, 1.f));
    for (int sample = 0; sample < inNumSamples; ++sample) {
        if (unit->resetTrigger.rising(
                creativereverb::inputAt(
                    unit, 6, sample, 0.f)))
            unit->core.clear();
        float left = 0.f;
        float right = 0.f;
        unit->core.process(
            creativereverb::inputAt(unit, 0, sample, 0.f),
            creativereverb::clampValue(
                creativereverb::inputAt(
                    unit, 4, sample, 1.f),
                0.f, 1.f),
            left, right);
        OUT(0)[sample] = left;
        OUT(1)[sample] = right;
    }
}

void GroupedFDN_Ctor(GroupedFDN* unit)
{
    creativereverb::constructUnit(unit);
    SETCALC(GroupedFDN_next);
    unit->failed = false;
    int bufferNumber = 0;
    int requestedGroups = 0;
    int requestedLinesPerGroup = 0;
    const bool initValid =
        creativereverb::readInitInteger(
            unit, 1, 0, 16777215, bufferNumber)
        && creativereverb::readInitInteger(
            unit, 2, 1, creativereverb::kMaxGroups,
            requestedGroups)
        && creativereverb::readInitInteger(
            unit, 3, 1, creativereverb::kMaxDelayLines,
            requestedLinesPerGroup)
        && unit->mNumOutputs == 2;
    if (!initValid
        || !unit->modelBuffer.capture(unit, bufferNumber)
        || !creativereverb::readPreparedModel(
            unit->modelBuffer.data,
            unit->modelBuffer.samples,
            creativereverb::kGroupedFDNMagic,
            unit->model)
        || !creativereverb::sampleRateMatches(
            unit->model, SAMPLERATE)
        || unit->model.channelCount != 2
        || unit->model.fieldA < 1
        || unit->model.fieldA
            > creativereverb::kMaxDelayLines
        || unit->model.fieldB != requestedGroups
        || unit->model.fieldC != requestedLinesPerGroup
        || unit->model.fieldA
            != requestedGroups * requestedLinesPerGroup
        || unit->model.fieldD
            != creativereverb::kFDNLineStride
        || unit->model.payloadSize()
            != (
                requestedGroups
                    * (requestedGroups - 1) / 2)
                + unit->model.fieldA
                * creativereverb::kFDNLineStride
        ) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "GroupedFDN: invalid model, sample rate, "
            "or fixed group layout.");
        return;
    }
    unit->lineCount = unit->model.fieldA;
    unit->groupCount = requestedGroups;
    unit->linesPerGroup = requestedLinesPerGroup;
    const int pairCount =
        unit->groupCount * (unit->groupCount - 1) / 2;
    bool modelValid = true;
    for (int pair = 0; pair < pairCount; ++pair) {
        unit->pairAngles[pair] =
            unit->model.payload()[pair];
        modelValid = modelValid
            && std::isfinite(unit->pairAngles[pair])
            && unit->pairAngles[pair] >= 0.f
            && unit->pairAngles[pair]
                <= static_cast<float>(
                    creativereverb::kPi * 0.5);
    }
    modelValid = modelValid
        && creativereverb::readFDNLineSpecs(
            unit->model.payload() + pairCount,
            unit->model.payloadSize() - pairCount,
            unit->specs, unit->lineCount,
            unit->groupCount);
    if (!modelValid) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "GroupedFDN: invalid pairwise coupling "
            "angles or line data.");
        return;
    }
    const std::size_t delaySamples =
        creativereverb::requiredDelaySamples(
            unit->specs, unit->lineCount);
    unit->delayStorage =
        creativereverb::allocateRT<float>(
            unit->mWorld, delaySamples);
    if (!unit->delayStorage
        || !unit->core.configure(
            unit->delayStorage, delaySamples,
            unit->specs, unit->lineCount,
            unit->groupCount, unit->linesPerGroup,
            0.f, static_cast<float>(SAMPLERATE),
            unit->pairAngles, pairCount)) {
        unit->failed = true;
        creativereverb::failUnit(
            unit,
            "GroupedFDN: real-time allocation or "
            "stable core configuration failed.");
        return;
    }
    unit->resetTrigger.reset(IN0(6));
    ClearUnitOutputs(unit, 1);
}

void GroupedFDN_Dtor(GroupedFDN* unit)
{
    creativereverb::releaseRT(
        unit->mWorld, unit->delayStorage);
    creativereverb::destroyUnit(unit);
}

} // namespace

void registerGroupedFDN()
{
    DefineDtorCantAliasUnit(GroupedFDN);
}
