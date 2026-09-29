// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace creativereverb {

struct ComplexValue {
    float real { 0.f };
    float imaginary { 0.f };
};

inline ComplexValue add(
    ComplexValue first, ComplexValue second) noexcept
{
    return {
        first.real + second.real,
        first.imaginary + second.imaginary
    };
}

inline ComplexValue subtract(
    ComplexValue first, ComplexValue second) noexcept
{
    return {
        first.real - second.real,
        first.imaginary - second.imaginary
    };
}

inline ComplexValue multiply(
    ComplexValue first, ComplexValue second) noexcept
{
    return {
        first.real * second.real
            - first.imaginary * second.imaginary,
        first.real * second.imaginary
            + first.imaginary * second.real
    };
}

inline void radix2FFT(
    ComplexValue* data, int size, bool inverse) noexcept
{
    if (!data || !isPowerOfTwo(size))
        return;
    for (int index = 1, reverse = 0;
         index < size; ++index) {
        int bit = size >> 1;
        while (reverse & bit) {
            reverse ^= bit;
            bit >>= 1;
        }
        reverse ^= bit;
        if (index < reverse)
            std::swap(data[index], data[reverse]);
    }
    for (int length = 2; length <= size; length <<= 1) {
        const float angle = static_cast<float>(
            (inverse ? 1.0 : -1.0)
            * kTwoPi / length);
        const ComplexValue step {
            std::cos(angle), std::sin(angle)
        };
        for (int start = 0; start < size;
             start += length) {
            ComplexValue weight { 1.f, 0.f };
            const int half = length >> 1;
            for (int offset = 0; offset < half; ++offset) {
                const ComplexValue even =
                    data[start + offset];
                const ComplexValue odd = multiply(
                    data[start + offset + half], weight);
                data[start + offset] = add(even, odd);
                data[start + offset + half] =
                    subtract(even, odd);
                weight = multiply(weight, step);
            }
        }
    }
    if (inverse) {
        const float normalization =
            1.f / static_cast<float>(size);
        for (int index = 0; index < size; ++index) {
            data[index].real *= normalization;
            data[index].imaginary *= normalization;
        }
    }
}

class UniformPartitionedConvolver {
public:
    bool configure(
        int partitionSize, int partitionCount,
        int channelCount, ComplexValue* impulseSpectra,
        ComplexValue* inputHistory, float* inputFifo,
        float* outputFifo, float* overlap,
        ComplexValue* fftScratch,
        ComplexValue* accumulationScratch) noexcept
    {
        const int fftSize = partitionSize * 2;
        if (!isPowerOfTwo(partitionSize)
            || partitionSize < 64
            || partitionCount < 1 || channelCount < 1
            || channelCount > 8 || !impulseSpectra
            || !inputHistory || !inputFifo || !outputFifo
            || !overlap || !fftScratch
            || !accumulationScratch)
            return false;
        partitionSize_ = partitionSize;
        fftSize_ = fftSize;
        binCount_ = partitionSize + 1;
        partitionCount_ = partitionCount;
        channelCount_ = channelCount;
        impulseSpectra_ = impulseSpectra;
        inputHistory_ = inputHistory;
        inputFifo_ = inputFifo;
        outputFifo_ = outputFifo;
        overlap_ = overlap;
        fftScratch_ = fftScratch;
        accumulationScratch_ = accumulationScratch;
        clear();
        return true;
    }

    void clear() noexcept
    {
        if (inputHistory_)
            std::fill(
                inputHistory_,
                inputHistory_
                    + static_cast<std::size_t>(partitionCount_)
                        * static_cast<std::size_t>(binCount_),
                ComplexValue {});
        if (inputFifo_)
            std::fill(
                inputFifo_,
                inputFifo_ + partitionSize_, 0.f);
        if (outputFifo_)
            std::fill(
                outputFifo_,
                outputFifo_
                    + static_cast<std::size_t>(channelCount_)
                        * static_cast<std::size_t>(
                            partitionSize_),
                0.f);
        if (overlap_)
            std::fill(
                overlap_,
                overlap_
                    + static_cast<std::size_t>(channelCount_)
                        * static_cast<std::size_t>(
                            partitionSize_),
                0.f);
        fifoPosition_ = 0;
        historyPosition_ = 0;
    }

    void setImpulsePartition(
        int channel, int partition,
        const float* timeData, int sampleCount) noexcept
    {
        if (channel < 0 || channel >= channelCount_
            || partition < 0
            || partition >= partitionCount_
            || !timeData)
            return;
        std::fill(
            fftScratch_, fftScratch_ + fftSize_,
            ComplexValue {});
        const int count =
            std::min(sampleCount, partitionSize_);
        for (int sample = 0; sample < count; ++sample)
            fftScratch_[sample].real = timeData[sample];
        radix2FFT(fftScratch_, fftSize_, false);
        ComplexValue* destination =
            impulseSpectra_
            + (
                static_cast<std::size_t>(channel)
                    * static_cast<std::size_t>(
                        partitionCount_)
                + static_cast<std::size_t>(partition))
                * static_cast<std::size_t>(binCount_);
        std::copy(
            fftScratch_, fftScratch_ + binCount_,
            destination);
    }

    void processSample(
        float input, float* outputs) noexcept
    {
        inputFifo_[fifoPosition_] = safeAudio(input);
        for (int channel = 0; channel < channelCount_;
             ++channel) {
            outputs[channel] =
                outputFifo_[
                    static_cast<std::size_t>(channel)
                        * static_cast<std::size_t>(
                            partitionSize_)
                    + static_cast<std::size_t>(
                        fifoPosition_)];
        }
        ++fifoPosition_;
        if (fifoPosition_ >= partitionSize_) {
            processPartition();
            fifoPosition_ = 0;
        }
    }

    int latencySamples() const noexcept
    {
        return partitionSize_;
    }

private:
    void processPartition() noexcept
    {
        std::fill(
            fftScratch_, fftScratch_ + fftSize_,
            ComplexValue {});
        for (int sample = 0; sample < partitionSize_;
             ++sample)
            fftScratch_[sample].real = inputFifo_[sample];
        radix2FFT(fftScratch_, fftSize_, false);
        ComplexValue* history =
            inputHistory_
            + static_cast<std::size_t>(historyPosition_)
                * static_cast<std::size_t>(binCount_);
        std::copy(
            fftScratch_, fftScratch_ + binCount_,
            history);

        for (int channel = 0; channel < channelCount_;
             ++channel) {
            std::fill(
                accumulationScratch_,
                accumulationScratch_ + fftSize_,
                ComplexValue {});
            for (int partition = 0;
                 partition < partitionCount_; ++partition) {
                int historyIndex =
                    historyPosition_ - partition;
                if (historyIndex < 0)
                    historyIndex += partitionCount_;
                const ComplexValue* inputSpectrum =
                    inputHistory_
                    + static_cast<std::size_t>(historyIndex)
                        * static_cast<std::size_t>(
                            binCount_);
                const ComplexValue* impulseSpectrum =
                    impulseSpectra_
                    + (
                        static_cast<std::size_t>(channel)
                            * static_cast<std::size_t>(
                                partitionCount_)
                        + static_cast<std::size_t>(partition))
                        * static_cast<std::size_t>(
                            binCount_);
                for (int bin = 0; bin < binCount_; ++bin) {
                    accumulationScratch_[bin] = add(
                        accumulationScratch_[bin],
                        multiply(
                            inputSpectrum[bin],
                            impulseSpectrum[bin]));
                }
            }
            for (int bin = 1; bin < partitionSize_; ++bin) {
                const ComplexValue positive =
                    accumulationScratch_[bin];
                accumulationScratch_[fftSize_ - bin] = {
                    positive.real, -positive.imaginary
                };
            }
            accumulationScratch_[0].imaginary = 0.f;
            accumulationScratch_[partitionSize_].imaginary =
                0.f;
            radix2FFT(
                accumulationScratch_, fftSize_, true);
            float* output =
                outputFifo_
                + static_cast<std::size_t>(channel)
                    * static_cast<std::size_t>(
                        partitionSize_);
            float* overlap =
                overlap_
                + static_cast<std::size_t>(channel)
                    * static_cast<std::size_t>(
                        partitionSize_);
            for (int sample = 0; sample < partitionSize_;
                 ++sample) {
                output[sample] = safeAudio(
                    accumulationScratch_[sample].real
                    + overlap[sample]);
                overlap[sample] = safeAudio(
                    accumulationScratch_[
                        sample + partitionSize_].real);
            }
        }
        if (++historyPosition_ >= partitionCount_)
            historyPosition_ = 0;
    }

    int partitionSize_ { 0 };
    int fftSize_ { 0 };
    int binCount_ { 0 };
    int partitionCount_ { 0 };
    int channelCount_ { 0 };
    int fifoPosition_ { 0 };
    int historyPosition_ { 0 };
    ComplexValue* impulseSpectra_ { nullptr };
    ComplexValue* inputHistory_ { nullptr };
    float* inputFifo_ { nullptr };
    float* outputFifo_ { nullptr };
    float* overlap_ { nullptr };
    ComplexValue* fftScratch_ { nullptr };
    ComplexValue* accumulationScratch_ { nullptr };
};

} // namespace creativereverb
