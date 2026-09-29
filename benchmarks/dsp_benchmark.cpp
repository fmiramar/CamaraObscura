// SPDX-License-Identifier: GPL-3.0-or-later

#include "reverb/FDNCore.hpp"
#include "reverb/ModalBank.hpp"
#include "reverb/PartitionedConvolver.hpp"
#include "reverb/VelvetFIR.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
volatile float benchmarkSink = 0.f;

template <typename Function>
double measure(int frames, Function function)
{
    const auto start = Clock::now();
    float checksum = 0.f;
    for (int frame = 0; frame < frames; ++frame)
        checksum += function(frame);
    const auto end = Clock::now();
    benchmarkSink = benchmarkSink + checksum;
    return std::chrono::duration<double, std::nano>(
        end - start)
        .count()
        / frames;
}

void printResult(
    const std::string& name, int size,
    double nanoseconds, std::size_t bytes)
{
    const double oneCore =
        nanoseconds * 48000.0 / 1.0e7;
    std::cout << std::left << std::setw(27) << name
              << std::right << std::setw(7) << size
              << std::setw(14) << std::fixed
              << std::setprecision(2) << nanoseconds
              << std::setw(13) << std::setprecision(3)
              << oneCore
              << std::setw(14) << bytes << '\n';
}

void benchmarkFDN(int lines)
{
    std::vector<creativereverb::FDNLineSpec> specs(lines);
    std::size_t samples = 0;
    for (int line = 0; line < lines; ++line) {
        specs[line].delaySamples = 1009 + line * 38;
        specs[line].group = 0;
        specs[line].t60Low = 2.5f;
        specs[line].t60High = 1.4f;
        specs[line].inputGain =
            (line % 2 ? -1.f : 1.f) / std::sqrt(lines);
        specs[line].outputLeft =
            (1.f - (line % 2 ? 0.35f : -0.35f))
            / std::sqrt(lines);
        specs[line].outputRight =
            (1.f + (line % 2 ? 0.35f : -0.35f))
            / std::sqrt(lines);
        samples +=
            static_cast<std::size_t>(specs[line].delaySamples);
    }
    std::vector<float> storage(samples);
    creativereverb::FDNCore core;
    core.configure(
        storage.data(), storage.size(), specs.data(),
        lines, 1, lines, 0.7f, 48000.f);
    const double time = measure(48000, [&](int frame) {
        float left = 0.f;
        float right = 0.f;
        core.process(frame == 0 ? 1.f : 0.f, 1.f, left, right);
        return left + right;
    });
    printResult(
        "FDNCore", lines, time,
        storage.size() * sizeof(float));
}

void benchmarkModal(int modes)
{
    std::vector<creativereverb::ModalSpec> specs(modes);
    for (int mode = 0; mode < modes; ++mode) {
        specs[mode].frequency =
            40.f * std::pow(
                18000.f / 40.f,
                static_cast<float>(mode) / modes);
        specs[mode].t60 = 0.4f + 3.f * mode / modes;
        specs[mode].inputGain = 1.f / std::sqrt(modes);
        specs[mode].outputLeft =
            (mode % 2 ? -1.f : 1.f) / std::sqrt(modes);
        specs[mode].outputRight =
            (mode % 3 ? 1.f : -1.f) / std::sqrt(modes);
    }
    std::vector<float> real(modes);
    std::vector<float> imaginary(modes);
    std::vector<float> coefficientReal(modes);
    std::vector<float> coefficientImaginary(modes);
    creativereverb::ModalBank bank;
    bank.configure(
        specs.data(), modes, real.data(), imaginary.data(),
        coefficientReal.data(), coefficientImaginary.data(),
        48000.f);
    const int frames = modes <= 256 ? 24000 : 4800;
    const double time = measure(frames, [&](int frame) {
        float left = 0.f;
        float right = 0.f;
        bank.process(frame == 0 ? 1.f : 0.f, 0.f, left, right);
        return left + right;
    });
    const std::size_t bytes =
        static_cast<std::size_t>(modes)
        * (sizeof(creativereverb::ModalSpec)
            + 4 * sizeof(float));
    printResult("ModalBank", modes, time, bytes);
}

void benchmarkConvolver(int channels, int partitions)
{
    constexpr int partitionSize = 4096;
    constexpr int fftSize = partitionSize * 2;
    constexpr int bins = partitionSize + 1;
    std::vector<creativereverb::ComplexValue> impulse(
        static_cast<std::size_t>(channels)
        * partitions * bins);
    std::vector<creativereverb::ComplexValue> history(
        static_cast<std::size_t>(partitions) * bins);
    std::vector<float> input(partitionSize);
    std::vector<float> output(
        static_cast<std::size_t>(channels) * partitionSize);
    std::vector<float> overlap(
        static_cast<std::size_t>(channels) * partitionSize);
    std::vector<creativereverb::ComplexValue> fft(fftSize);
    std::vector<creativereverb::ComplexValue> accumulation(
        fftSize);
    creativereverb::UniformPartitionedConvolver convolver;
    convolver.configure(
        partitionSize, partitions, channels,
        impulse.data(), history.data(), input.data(),
        output.data(), overlap.data(), fft.data(),
        accumulation.data());
    std::vector<float> partition(partitionSize);
    for (int channel = 0; channel < channels; ++channel) {
        for (int index = 0; index < partitions; ++index) {
            std::fill(partition.begin(), partition.end(), 0.f);
            partition[0] =
                0.1f / std::sqrt(1.f + index + channel);
            convolver.setImpulsePartition(
                channel, index, partition.data(), partitionSize);
        }
    }
    std::vector<float> result(channels);
    const double time = measure(48000, [&](int frame) {
        convolver.processSample(
            frame == 0 ? 1.f : 0.f, result.data());
        float sum = 0.f;
        for (float value : result)
            sum += value;
        return sum;
    });
    const std::size_t bytes =
        (impulse.size() + history.size()
            + fft.size() + accumulation.size())
            * sizeof(creativereverb::ComplexValue)
        + (input.size() + output.size() + overlap.size())
            * sizeof(float);
    printResult(
        "PartitionedConvolver", channels * partitions,
        time, bytes);
}

} // namespace

int main()
{
    std::cout
        << "CamaraObscura native DSP benchmark\n"
        << "sample precision=float, nominal rate=48000 Hz\n"
        << std::left << std::setw(27) << "kernel"
        << std::right << std::setw(7) << "size"
        << std::setw(14) << "ns/sample"
        << std::setw(13) << "% one core"
        << std::setw(14) << "state bytes" << '\n';
    benchmarkFDN(8);
    benchmarkFDN(16);
    benchmarkFDN(32);
    benchmarkModal(64);
    benchmarkModal(256);
    benchmarkModal(1024);
    benchmarkModal(2048);
    benchmarkConvolver(2, 6);
    benchmarkConvolver(8, 6);
    std::cout << "checksum=" << benchmarkSink << '\n';
    return 0;
}
