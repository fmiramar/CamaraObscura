// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SC_PlugIn.h"
#include "reverb/Common.hpp"
#include "reverb/PreparedModel.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>

extern InterfaceTable* ft;

namespace creativereverb {

// scsynth allocates Unit storage and initializes only the Unit base
// fields; it does not invoke a C++ constructor for the registered
// derived type. Preserve those server-owned fields while explicitly
// starting the lifetime of every C++ member and applying its default
// initialization. This is particularly important when the RT pool
// reuses a graph allocation containing old pointer values.
template <typename T>
void constructUnit(T* unit) noexcept
{
    static_assert(std::is_base_of<Unit, T>::value,
        "server plugin type must derive from Unit");
    const Unit serverState = *static_cast<Unit*>(unit);
    ::new (static_cast<void*>(unit)) T {};
    *static_cast<Unit*>(unit) = serverState;
}

template <typename T>
void destroyUnit(T* unit) noexcept
{
    if (unit)
        unit->~T();
}

template <typename T>
T* allocateRT(World* world, std::size_t count) noexcept
{
    if (!world || count == 0
        || count > std::numeric_limits<std::size_t>::max()
            / sizeof(T))
        return nullptr;
    T* result = static_cast<T*>(
        RTAlloc(world, count * sizeof(T)));
    if (result)
        std::fill(result, result + count, T {});
    return result;
}

template <typename T>
void releaseRT(World* world, T*& pointer) noexcept
{
    if (pointer) {
        RTFree(world, pointer);
        pointer = nullptr;
    }
}

inline SndBuf* resolveBuffer(
    Unit* unit, int bufferNumber) noexcept
{
    if (!unit || bufferNumber < 0)
        return nullptr;
    World* world = unit->mWorld;
    if (static_cast<std::uint32_t>(bufferNumber)
        < world->mNumSndBufs)
        return world->mSndBufs + bufferNumber;
    const int localNumber =
        bufferNumber - static_cast<int>(world->mNumSndBufs);
    Graph* parent = unit->mParent;
    if (!parent || localNumber < 0
        || localNumber > parent->localMaxBufNum)
        return nullptr;
    return parent->mLocalSndBufs + localNumber;
}

struct ModelBufferState {
    int number { -1 };
    SndBuf* buffer { nullptr };
    const float* data { nullptr };
    int samples { 0 };

    bool capture(Unit* unit, int bufferNumber) noexcept
    {
        SndBuf* candidate =
            resolveBuffer(unit, bufferNumber);
        if (!candidate || !candidate->data
            || candidate->channels != 1
            || candidate->samples <= 0)
            return false;
        number = bufferNumber;
        buffer = candidate;
        data = candidate->data;
        samples = static_cast<int>(candidate->samples);
        return true;
    }

    bool unchanged(Unit* unit) const noexcept
    {
        SndBuf* candidate = resolveBuffer(unit, number);
        return candidate && candidate == buffer
            && candidate->data == data
            && candidate->channels == 1
            && static_cast<int>(candidate->samples)
                == samples;
    }
};

inline float inputAt(
    Unit* unit, int input, int sample,
    float fallback = 0.f) noexcept
{
    const float value =
        INRATE(input) == calc_FullRate
        ? IN(input)[sample] : IN0(input);
    return finiteOr(value, fallback);
}

inline bool readInitInteger(
    Unit* unit, int input, int minimum,
    int maximum, int& result) noexcept
{
    return exactFloatInteger(
        IN0(input), minimum, maximum, result);
}

inline void failUnit(
    Unit* unit, const char* message) noexcept
{
    if (message)
        Print("%s\n", message);
    SETCALC(*ClearUnitOutputs);
    ClearUnitOutputs(unit, 1);
}

} // namespace creativereverb

void registerDarkVelvetReverb();
void registerGroupedFDN();
void registerVelvetFDN();
void registerRIRFDN();
void registerModalReverbBank();
void registerModalPlate();
void registerGeometryReverb();
