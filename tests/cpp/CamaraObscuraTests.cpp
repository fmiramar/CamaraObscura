// SPDX-License-Identifier: GPL-3.0-or-later

#include "reverb/AttenuationFilterBank.hpp"
#include "reverb/DarkVelvetCore.hpp"
#include "reverb/DelayBank.hpp"
#include "reverb/FDNCore.hpp"
#include "reverb/GeometryPaths.hpp"
#include "reverb/ModalBank.hpp"
#include "reverb/ModalPlateCore.hpp"
#include "reverb/PartitionedConvolver.hpp"
#include "reverb/PreparedModel.hpp"
#include "reverb/StructuredOrthogonalMatrix.hpp"
#include "reverb/VelvetFIR.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool close(double first, double second, double tolerance)
{
    return std::abs(first - second) <= tolerance;
}

void testPreparedModel()
{
    std::vector<float> data {
        444002.f, 1.f, 12.f, 14.f, 48000.f, 2.f,
        0.f, 17.f, 8.f, 2.f, 4.f, 1.f, 0.25f, 0.5f
    };
    creativereverb::PreparedModelView model;
    require(
        creativereverb::readPreparedModel(
            data.data(), static_cast<int>(data.size()),
            creativereverb::kGroupedFDNMagic, model),
        "valid prepared model");
    require(
        model.payloadSize() == 2 && model.seed == 17,
        "prepared header fields");
    data[3] = 13.f;
    require(
        !creativereverb::readPreparedModel(
            data.data(), static_cast<int>(data.size()),
            creativereverb::kGroupedFDNMagic, model),
        "prepared total-size rejection");
}

void testDelayBank()
{
    int lengths[2] { 3, 5 };
    float storage[8] {};
    creativereverb::DelayBank bank;
    require(
        bank.configure(storage, 8, lengths, 2),
        "delay bank configure");
    bank.write(0, 1.f);
    require(bank.read(0) == 0.f, "delay before length");
    bank.write(0, 0.f);
    bank.write(0, 0.f);
    require(bank.read(0) == 1.f, "integer delay timing");
    bank.clear();
    require(bank.read(0) == 0.f, "delay clear");

    int fractionalLengths[2] { 8, 12 };
    float fractionalStorage[20] {};
    creativereverb::FractionalDelayBank fractional;
    require(
        fractional.configure(
            fractionalStorage, 20,
            fractionalLengths, 2),
        "fractional delay bank configure");
    fractional.write(0, 1.f);
    fractional.write(0, 0.f);
    require(
        close(
            fractional.readLinear(0, 1.5f),
            0.5, 1.0e-7),
        "fractional delay bank interpolation");

    float lineStorage[8] {};
    creativereverb::FractionalDelayLine line;
    require(
        line.configure(lineStorage, 8),
        "fractional delay line configure");
    float delayed = line.readLinear(1.f);
    line.write(1.f);
    require(delayed == 0.f, "fractional delay initial silence");
    delayed = line.readLinear(1.f);
    line.write(0.f);
    require(
        delayed == 1.f,
        "fractional delay read-before-write timing");
}

void testOrthogonalTransforms()
{
    float vector[8] {
        0.2f, -0.4f, 0.7f, 0.1f,
        -0.3f, 0.9f, 0.5f, -0.8f
    };
    const double original =
        creativereverb::energy(vector, 8);
    creativereverb::householder(vector, 8);
    require(
        close(
            creativereverb::energy(vector, 8),
            original, 1.0e-6),
        "Householder energy");
    require(
        creativereverb::hadamard(vector, 8),
        "Hadamard valid size");
    require(
        close(
            creativereverb::energy(vector, 8),
            original, 2.0e-6),
        "Hadamard energy");
    for (int step = 0; step <= 20; ++step) {
        float grouped[8] {
            0.2f, -0.4f, 0.7f, 0.1f,
            -0.3f, 0.9f, 0.5f, -0.8f
        };
        creativereverb::groupedOrthogonal(
            grouped, 2, 4, static_cast<float>(step) / 20.f, 0.7f);
        require(
            close(
                creativereverb::energy(grouped, 8),
                original, 3.0e-6),
            "coupling morph remains orthogonal");
    }
    const float pairAngles[3] { 0.1f, 0.35f, 0.7f };
    for (int step = 0; step <= 20; ++step) {
        float grouped[6] {
            0.2f, -0.4f, 0.7f,
            0.1f, -0.3f, 0.9f
        };
        const double before =
            creativereverb::energy(grouped, 6);
        creativereverb::groupedOrthogonalAngles(
            grouped, 3, 2, static_cast<float>(step) / 20.f,
            pairAngles, 3);
        require(
            close(
                creativereverb::energy(grouped, 6),
                before, 3.0e-6),
            "pairwise Givens coupling remains orthogonal");
    }
    const int permutation[4] { 2, 0, 3, 1 };
    const float signs[4] { 1.f, -1.f, 1.f, -1.f };
    float permuted[4] { 1.f, 2.f, 3.f, 4.f };
    creativereverb::SignedPermutationTransform signedPermutation;
    require(
        signedPermutation.configure(
            permutation, signs, 4),
        "signed permutation configure");
    signedPermutation.process(permuted);
    require(
        permuted[0] == 3.f && permuted[1] == -1.f
            && permuted[2] == 4.f
            && permuted[3] == -2.f,
        "signed permutation values");
    const float denseMatrix[4] {
        0.f, -1.f,
        1.f, 0.f
    };
    float denseVector[2] { 0.3f, -0.7f };
    const double denseEnergy =
        creativereverb::energy(denseVector, 2);
    creativereverb::DenseOrthogonalTransform dense;
    require(
        dense.configure(denseMatrix, 2),
        "dense orthogonal configure");
    dense.process(denseVector);
    require(
        close(
            creativereverb::energy(denseVector, 2),
            denseEnergy, 1.0e-7),
        "dense orthogonal energy");
}

void testAttenuationAndFDN()
{
    creativereverb::TwoStageAttenuationFilter filter;
    double energy = 0.0;
    for (int sample = 0; sample < 200000; ++sample) {
        const float input = sample == 0 ? 1.f : 0.f;
        const float output =
            filter.process(input, 0.999f, 0.95f, 0.7f);
        require(std::isfinite(output), "attenuation finite");
        energy += output * output;
    }
    require(energy > 0.0 && energy < 2.0,
        "attenuation bounded");

    constexpr int lines = 8;
    creativereverb::FDNLineSpec specs[lines];
    std::size_t delaySamples = 0;
    for (int line = 0; line < lines; ++line) {
        specs[line].delaySamples = 31 + line * 6;
        specs[line].group = line / 4;
        specs[line].t60Low = line < 4 ? 0.4f : 1.2f;
        specs[line].t60High = line < 4 ? 0.25f : 0.8f;
        specs[line].inputGain =
            line == 0 ? 1.f : (line % 2 ? -0.25f : 0.25f);
        specs[line].outputLeft =
            line % 2 ? -1.f : 1.f;
        specs[line].outputRight =
            line % 3 ? 0.7f : -0.7f;
        delaySamples +=
            static_cast<std::size_t>(specs[line].delaySamples);
    }
    std::vector<float> storage(delaySamples);
    creativereverb::FDNCore fdn;
    require(
        fdn.configure(
            storage.data(), storage.size(), specs,
            lines, 2, 4, 0.6f, 48000.f),
        "FDN configure");
    double tailEnergy = 0.0;
    for (int sample = 0; sample < 480000; ++sample) {
        float left = 0.f;
        float right = 0.f;
        fdn.process(sample == 0 ? 1.f : 0.f, 0.7f,
            left, right);
        require(
            std::isfinite(left) && std::isfinite(right),
            "FDN finite long render");
        tailEnergy += left * left + right * right;
    }
    require(tailEnergy > 1.0e-5 && tailEnergy < 100.0,
        "FDN impulse bounded and non-silent");
    fdn.clear();
    fdn.updateCoefficients(1.f, 0.f, 0.f);
    for (int sample = 0; sample < 4096; ++sample) {
        float left = 0.f;
        float right = 0.f;
        fdn.process(
            sample == 0 ? 1.f : 0.f, 1.f, left, right);
    }
    fdn.updateCoefficients(1.f, 0.f, 1.f);
    double freezeEnergy = 0.0;
    for (int sample = 0; sample < 480000; ++sample) {
        float left = 0.f;
        float right = 0.f;
        fdn.process(0.f, 1.f, left, right);
        require(
            std::isfinite(left) && std::isfinite(right),
            "FDN freeze finite");
        freezeEnergy += left * left + right * right;
    }
    require(
        freezeEnergy > 1.0e-6
            && freezeEnergy < 100000.0,
        "FDN freeze persists without growth");
    fdn.clear();
    float left = 1.f;
    float right = 1.f;
    fdn.process(0.f, 1.f, left, right);
    require(left == 0.f && right == 0.f, "FDN reset");
}

void testVelvet()
{
    float storage[64] {};
    creativereverb::VelvetTap taps[4] {
        { 2, 1.f }, { 7, -1.f },
        { 13, 1.f }, { 29, -1.f }
    };
    creativereverb::VelvetFIR filter;
    require(
        filter.configure(storage, 64, taps, 4),
        "velvet configure");
    int nonzero = 0;
    for (int sample = 0; sample < 64; ++sample) {
        const float output =
            filter.process(sample == 0 ? 1.f : 0.f, 1.f, 1.f);
        if (std::abs(output) > 1.0e-6f)
            ++nonzero;
    }
    require(nonzero == 4, "velvet tap timing/count");

    const int delays[8] { 3, 5, 7, 11, 13, 17, 19, 23 };
    constexpr int count = 8;
    constexpr int frequencies = 1024;
    auto complexHadamard = [](std::complex<double>* vector) {
        for (int span = 1; span < count; span *= 2) {
            for (int block = 0; block < count;
                 block += span * 2) {
                for (int offset = 0; offset < span; ++offset) {
                    const int first = block + offset;
                    const int second = first + span;
                    const auto a = vector[first];
                    const auto b = vector[second];
                    vector[first] = a + b;
                    vector[second] = a - b;
                }
            }
        }
        const double scale = 1.0 / std::sqrt(count);
        for (int index = 0; index < count; ++index)
            vector[index] *= scale;
    };
    double maximumError = 0.0;
    for (int frequency = 0; frequency < frequencies;
         ++frequency) {
        const double omega =
            creativereverb::kTwoPi * frequency / frequencies;
        std::complex<double> matrix[count][count] {};
        for (int column = 0; column < count; ++column) {
            std::complex<double> vector[count] {};
            vector[column] = 1.0;
            complexHadamard(vector);
            for (int line = 0; line < count; ++line)
                vector[line] *= std::exp(
                    std::complex<double>(
                        0.0, -omega * delays[line]));
            complexHadamard(vector);
            for (int row = 0; row < count; ++row)
                matrix[row][column] = vector[row];
        }
        for (int first = 0; first < count; ++first) {
            for (int second = 0; second < count; ++second) {
                std::complex<double> inner {};
                for (int row = 0; row < count; ++row)
                    inner += std::conj(matrix[row][first])
                        * matrix[row][second];
                const std::complex<double> expected =
                    first == second ? 1.0 : 0.0;
                maximumError = std::max(
                    maximumError,
                    std::abs(inner - expected));
            }
        }
    }
    require(
        maximumError < 1.0e-12,
        "velvet H-D(z)-H paraunitary dense-grid condition");
}

void testPartitionedConvolver()
{
    constexpr int partitionSize = 64;
    constexpr int fftSize = partitionSize * 2;
    constexpr int bins = partitionSize + 1;
    constexpr int partitions = 2;
    std::vector<creativereverb::ComplexValue> impulse(
        partitions * bins);
    std::vector<creativereverb::ComplexValue> history(
        partitions * bins);
    std::vector<float> input(partitionSize);
    std::vector<float> output(partitionSize);
    std::vector<float> overlap(partitionSize);
    std::vector<creativereverb::ComplexValue> fft(fftSize);
    std::vector<creativereverb::ComplexValue> accum(fftSize);
    creativereverb::UniformPartitionedConvolver convolver;
    require(
        convolver.configure(
            partitionSize, partitions, 1,
            impulse.data(), history.data(), input.data(),
            output.data(), overlap.data(), fft.data(),
            accum.data()),
        "partitioned convolver configure");
    float first[partitionSize] {};
    float second[partitionSize] {};
    first[0] = 1.f;
    first[5] = 0.5f;
    second[6] = -0.25f;
    convolver.setImpulsePartition(0, 0, first, partitionSize);
    convolver.setImpulsePartition(0, 1, second, partitionSize);
    std::vector<float> rendered(256);
    for (int sample = 0; sample < 256; ++sample) {
        float result[1] {};
        convolver.processSample(
            sample == 0 ? 1.f : 0.f, result);
        rendered[static_cast<std::size_t>(sample)] =
            result[0];
    }
    require(
        close(rendered[64], 1.0, 2.0e-4)
            && close(rendered[69], 0.5, 2.0e-4)
            && close(rendered[134], -0.25, 2.0e-4),
        "partitioned convolution values/latency");
}

void testModal()
{
    const double radius =
        creativereverb::amplitudeT60Radius(2.0, 48000.0);
    require(
        close(std::pow(radius, 96000.0), 0.001, 1.0e-9),
        "modal amplitude T60 convention");
    creativereverb::ModalSpec spec {
        1000.f, 1.f, 1.f, 1.f, -1.f, 0.f
    };
    float real[1] {};
    float imaginary[1] {};
    float coefficientReal[1] {};
    float coefficientImaginary[1] {};
    creativereverb::ModalBank bank;
    require(
        bank.configure(
            &spec, 1, real, imaginary,
            coefficientReal, coefficientImaginary, 48000.f),
        "modal configure");
    double sum = 0.0;
    for (int sample = 0; sample < 480000; ++sample) {
        float left = 0.f;
        float right = 0.f;
        bank.process(sample == 0 ? 1.f : 0.f, 0.f,
            left, right);
        require(
            std::isfinite(left) && std::isfinite(right),
            "modal finite");
        require(
            close(left, -right, 1.0e-7),
            "modal output residues");
        sum += left * left;
    }
    require(sum > 1.0, "modal non-silent");
    bank.clear();
    for (int sample = 0; sample < 1000; ++sample) {
        float left = 0.f;
        float right = 0.f;
        bank.process(sample == 0 ? 1.f : 0.f, 0.f,
            left, right);
    }
    bank.updateCoefficients(1.f, 1.f, 0.f, 0.f, 1.f);
    double frozen = 0.0;
    for (int sample = 0; sample < 480000; ++sample) {
        float left = 0.f;
        float right = 0.f;
        bank.process(1.f, 0.f, left, right);
        require(
            std::isfinite(left) && std::isfinite(right),
            "modal freeze finite");
        frozen += left * left;
    }
    require(
        frozen > 1.0e-5 && frozen < 1000000.0,
        "modal freeze suppresses excitation and stays bounded");
    bank.clear();
}

void testDarkAndPlate()
{
    creativereverb::DarkSegment segment;
    segment.startSeconds = 0.f;
    segment.endSeconds = 2.f;
    segment.startDb = 0.f;
    segment.endDb = -60.f;
    require(
        close(
            creativereverb::darkEnvelopeAmplitude(segment, 2.f),
            0.001, 1.0e-7),
        "dark envelope amplitude");
    require(
        creativereverb::dictionaryImpulse(
            0, 0, 48000.f) != 0.f,
        "dictionary impulse");

    const double frequency =
        creativereverb::simplySupportedPlateFrequency(
            1, 1, 2.4, 1.2, 0.0005, 7850.0,
            2.0e11, 0.3);
    require(
        frequency > 0.5 && frequency < 5.0,
        "plate fundamental physical range");
    require(
        std::abs(
            creativereverb::simplySupportedModeShape(
                2, 1, 0.5f, 0.5f))
            < 1.0e-6f,
        "plate nodal cancellation");
}

void testGeometry()
{
    const creativereverb::Vec3 room { 10.0, 8.0, 3.0 };
    const creativereverb::Vec3 source { 2.0, 3.0, 1.5 };
    const creativereverb::Vec3 listener { 6.0, 3.0, 1.5 };
    const auto path = creativereverb::shoeboxFirstOrderPath(
        source, listener, room, 0, 0.25);
    const double expectedDistance = 8.0;
    require(
        close(
            path.delaySeconds,
            expectedDistance / 343.0, 1.0e-6),
        "shoebox image-source timing");
    require(
        close(
            path.gain,
            std::sqrt(0.75) / expectedDistance,
            1.0e-6),
        "shoebox reflection gain");
}

} // namespace

int main()
{
    testPreparedModel();
    testDelayBank();
    testOrthogonalTransforms();
    testAttenuationAndFDN();
    testVelvet();
    testPartitionedConvolver();
    testModal();
    testDarkAndPlate();
    testGeometry();
    std::cout << "CREATIVE_REVERB_REFERENCE_TESTS_OK\n";
    return 0;
}
