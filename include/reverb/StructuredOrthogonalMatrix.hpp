// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>
#include <cmath>

namespace creativereverb {

inline void householder(
    float* vector, int count) noexcept
{
    if (!vector || count <= 0)
        return;
    double sum = 0.0;
    for (int index = 0; index < count; ++index)
        sum += vector[index];
    const float scale =
        static_cast<float>(2.0 * sum / count);
    for (int index = 0; index < count; ++index)
        vector[index] -= scale;
}

inline bool hadamard(float* vector, int count) noexcept
{
    if (!vector || !isPowerOfTwo(count))
        return false;
    for (int span = 1; span < count; span *= 2) {
        for (int block = 0; block < count;
             block += span * 2) {
            for (int offset = 0; offset < span; ++offset) {
                const int first = block + offset;
                const int second = first + span;
                const float a = vector[first];
                const float b = vector[second];
                vector[first] = a + b;
                vector[second] = a - b;
            }
        }
    }
    const float normalization =
        1.f / std::sqrt(static_cast<float>(count));
    for (int index = 0; index < count; ++index)
        vector[index] *= normalization;
    return true;
}

inline void givens(
    float& first, float& second, float angle) noexcept
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float a = first;
    const float b = second;
    first = cosine * a - sine * b;
    second = sine * a + cosine * b;
}

inline void groupedOrthogonal(
    float* vector, int groups, int linesPerGroup,
    float coupling, float maximumAngle) noexcept
{
    if (!vector || groups < 1 || linesPerGroup < 1)
        return;
    for (int group = 0; group < groups; ++group)
        householder(
            vector + group * linesPerGroup,
            linesPerGroup);
    const float angle =
        clampValue(coupling, 0.f, 1.f) * maximumAngle;
    for (int group = 0; group < groups - 1; ++group) {
        for (int line = 0; line < linesPerGroup; ++line) {
            float& first =
                vector[group * linesPerGroup + line];
            float& second =
                vector[(group + 1) * linesPerGroup + line];
            givens(first, second, angle);
        }
    }
}

inline void groupedOrthogonalAngles(
    float* vector, int groups, int linesPerGroup,
    float coupling, const float* pairAngles,
    int pairCount) noexcept
{
    if (!vector || groups < 1 || linesPerGroup < 1)
        return;
    for (int group = 0; group < groups; ++group)
        householder(
            vector + group * linesPerGroup,
            linesPerGroup);
    const int expectedPairs = groups * (groups - 1) / 2;
    if (!pairAngles || pairCount != expectedPairs)
        return;
    const float amount = clampValue(coupling, 0.f, 1.f);
    int pair = 0;
    for (int firstGroup = 0; firstGroup < groups - 1;
         ++firstGroup) {
        for (int secondGroup = firstGroup + 1;
             secondGroup < groups; ++secondGroup) {
            const float angle = amount * pairAngles[pair++];
            for (int line = 0; line < linesPerGroup; ++line) {
                float& first =
                    vector[
                        firstGroup * linesPerGroup + line];
                float& second =
                    vector[
                        secondGroup * linesPerGroup + line];
                givens(first, second, angle);
            }
        }
    }
}

inline double energy(
    const float* vector, int count) noexcept
{
    double result = 0.0;
    for (int index = 0; index < count; ++index)
        result += static_cast<double>(vector[index])
            * vector[index];
    return result;
}

class SignedPermutationTransform {
public:
    bool configure(
        const int* permutation, const float* signs,
        int count) noexcept
    {
        if (!permutation || !signs || count < 1
            || count > kMaxDelayLines)
            return false;
        bool used[kMaxDelayLines] {};
        for (int index = 0; index < count; ++index) {
            const int source = permutation[index];
            if (source < 0 || source >= count || used[source]
                || (
                    signs[index] != -1.f
                    && signs[index] != 1.f))
                return false;
            used[source] = true;
            permutation_[index] = source;
            signs_[index] = signs[index];
        }
        count_ = count;
        return true;
    }

    void process(float* vector) noexcept
    {
        if (!vector)
            return;
        for (int index = 0; index < count_; ++index)
            scratch_[index] =
                vector[permutation_[index]] * signs_[index];
        std::copy(
            scratch_, scratch_ + count_, vector);
    }

private:
    int permutation_[kMaxDelayLines] {};
    float signs_[kMaxDelayLines] {};
    float scratch_[kMaxDelayLines] {};
    int count_ { 0 };
};

class DenseOrthogonalTransform {
public:
    bool configure(
        const float* rowMajorMatrix, int count,
        double tolerance = 1.0e-4) noexcept
    {
        if (!rowMajorMatrix || count < 1
            || count > kMaxDelayLines
            || tolerance <= 0.0)
            return false;
        for (int first = 0; first < count; ++first) {
            for (int second = 0; second < count; ++second) {
                double inner = 0.0;
                for (int row = 0; row < count; ++row) {
                    const float firstValue =
                        rowMajorMatrix[row * count + first];
                    const float secondValue =
                        rowMajorMatrix[row * count + second];
                    if (!std::isfinite(firstValue)
                        || !std::isfinite(secondValue))
                        return false;
                    inner += static_cast<double>(firstValue)
                        * secondValue;
                }
                const double target =
                    first == second ? 1.0 : 0.0;
                if (std::abs(inner - target) > tolerance)
                    return false;
            }
        }
        count_ = count;
        std::copy(
            rowMajorMatrix,
            rowMajorMatrix
                + static_cast<std::size_t>(count)
                    * static_cast<std::size_t>(count),
            matrix_);
        return true;
    }

    void process(float* vector) noexcept
    {
        if (!vector)
            return;
        for (int row = 0; row < count_; ++row) {
            double sum = 0.0;
            for (int column = 0; column < count_; ++column)
                sum += static_cast<double>(
                    matrix_[row * count_ + column])
                    * vector[column];
            scratch_[row] = safeAudio(
                static_cast<float>(sum));
        }
        std::copy(scratch_, scratch_ + count_, vector);
    }

private:
    float matrix_[
        kMaxDelayLines * kMaxDelayLines] {};
    float scratch_[kMaxDelayLines] {};
    int count_ { 0 };
};

} // namespace creativereverb
