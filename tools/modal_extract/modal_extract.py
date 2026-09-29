#!/usr/bin/env python3
"""Extract a deterministic modal model from an integer-PCM impulse response."""

from __future__ import annotations

import argparse
import cmath
import math
import sys
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(TOOLS_ROOT))

from model_common import (  # noqa: E402
    radix2_fft,
    read_pcm_wav,
    schroeder_t60,
    split_low_high,
    write_model,
    write_pcm16_wav,
)

MAGIC = 444_005
VERSION = 1
HEADER_SIZE = 12
MODE_STRIDE = 6


def largest_power_of_two(value: int) -> int:
    if value < 2:
        raise ValueError("not enough samples for spectral extraction")
    return 1 << (value.bit_length() - 1)


def extract_modes(
    samples: list[float],
    sample_rate: int,
    count: int,
    maximum_fft: int,
    minimum_frequency: float,
    maximum_frequency: float,
    crossover: float,
) -> tuple[list[tuple[float, float, float, float]], dict[str, float]]:
    direct = max(range(len(samples)), key=lambda index: abs(samples[index]))
    available = samples[direct:]
    fft_size = largest_power_of_two(min(len(available), maximum_fft))
    if fft_size < 1024:
        raise ValueError("usable response is too short for modal extraction")
    windowed = [
        available[index]
        * (0.5 - 0.5 * math.cos(2.0 * math.pi * index / (fft_size - 1)))
        for index in range(fft_size)
    ]
    spectrum = radix2_fft([complex(value, 0.0) for value in windowed])
    low_bin = max(1, math.ceil(minimum_frequency * fft_size / sample_rate))
    high_bin = min(
        fft_size // 2 - 2,
        math.floor(maximum_frequency * fft_size / sample_rate),
    )
    candidates: list[tuple[float, int]] = []
    magnitudes = [abs(value) for value in spectrum]
    for bin_index in range(low_bin, high_bin + 1):
        magnitude = magnitudes[bin_index]
        if (
            magnitude >= magnitudes[bin_index - 1]
            and magnitude > magnitudes[bin_index + 1]
        ):
            candidates.append((magnitude, bin_index))
    if len(candidates) < count:
        present = {candidate[1] for candidate in candidates}
        candidates.extend(
            (magnitudes[index], index)
            for index in range(low_bin, high_bin + 1)
            if index not in present
        )
    selected = sorted(candidates, reverse=True)[:count]
    if not selected:
        raise ValueError("no spectral peaks found in requested range")

    low_signal, high_signal = split_low_high(
        available, sample_rate, crossover
    )
    broadband_t60, broadband_r2, _ = schroeder_t60(
        available, sample_rate
    )
    low_t60, low_r2, _ = schroeder_t60(low_signal, sample_rate)
    high_t60, high_r2, _ = schroeder_t60(high_signal, sample_rate)
    maximum_magnitude = max(candidate[0] for candidate in selected)
    raw_modes: list[tuple[float, float, float, float]] = []
    for magnitude, bin_index in selected:
        left = math.log(max(magnitudes[bin_index - 1], 1.0e-20))
        center = math.log(max(magnitude, 1.0e-20))
        right = math.log(max(magnitudes[bin_index + 1], 1.0e-20))
        denominator = left - 2.0 * center + right
        offset = 0.0
        if abs(denominator) > 1.0e-12:
            offset = max(-0.5, min(0.5, 0.5 * (left - right) / denominator))
        frequency = (bin_index + offset) * sample_rate / fft_size
        fraction = max(0.0, min(1.0, frequency / max(crossover * 2.0, 1.0)))
        t60 = low_t60 + (high_t60 - low_t60) * fraction
        gain = math.sqrt(max(magnitude / maximum_magnitude, 0.0))
        phase = cmath.phase(spectrum[bin_index])
        raw_modes.append((frequency, t60, gain, phase))
    gain_norm = math.sqrt(sum(mode[2] ** 2 for mode in raw_modes))
    modes = [
        (frequency, t60, gain / max(gain_norm, 1.0e-12), phase)
        for frequency, t60, gain, phase in raw_modes
    ]
    modes.sort(key=lambda mode: mode[0])
    diagnostics = {
        "direct_sample": float(direct),
        "fft_size": float(fft_size),
        "broadband_t60": broadband_t60,
        "broadband_r2": broadband_r2,
        "low_t60": low_t60,
        "low_r2": low_r2,
        "high_t60": high_t60,
        "high_r2": high_r2,
    }
    return modes, diagnostics


def render_modes(
    modes: list[tuple[float, float, float, float]],
    sample_rate: int,
    seconds: float,
) -> tuple[list[float], list[float]]:
    real = [0.0] * len(modes)
    imaginary = [0.0] * len(modes)
    coefficients = [
        (
            10.0 ** (-3.0 / (mode[1] * sample_rate))
            * cmath.exp(2j * math.pi * mode[0] / sample_rate)
        )
        for mode in modes
    ]
    frames = max(1, round(seconds * sample_rate))
    left = [0.0] * frames
    right = [0.0] * frames
    scale = 1.0 / math.sqrt(len(modes))
    for frame in range(frames):
        excitation = 1.0 if frame == 0 else 0.0
        left_sum = 0.0
        right_sum = 0.0
        for index, mode in enumerate(modes):
            old = complex(real[index], imaginary[index])
            state = coefficients[index] * old + excitation
            real[index] = state.real
            imaginary[index] = state.imag
            output = (
                state.real * math.cos(mode[3])
                - state.imag * math.sin(mode[3])
            )
            left_sum += output * mode[2]
            right_sum += output * mode[2] * (
                1.0 if index % 2 == 0 else -1.0
            )
        left[frame] = left_sum * scale
        right[frame] = right_sum * scale
    return left, right


def analyze(arguments: argparse.Namespace) -> None:
    samples, sample_rate, channels = read_pcm_wav(arguments.input)
    maximum_frequency = arguments.maximum_frequency or sample_rate * 0.49
    if not 1 <= arguments.modes <= 4096:
        raise ValueError("mode count is outside 1..4096")
    if not arguments.minimum_frequency < maximum_frequency < sample_rate * 0.5:
        raise ValueError("invalid modal frequency range")
    modes, diagnostics = extract_modes(
        samples,
        sample_rate,
        arguments.modes,
        arguments.maximum_fft,
        arguments.minimum_frequency,
        maximum_frequency,
        arguments.crossover,
    )
    payload: list[float] = []
    for index, (frequency, t60, gain, phase) in enumerate(modes):
        payload.extend(
            [
                frequency,
                t60,
                1.0,
                gain,
                gain if index % 2 == 0 else -gain,
                phase,
            ]
        )
    model = [
        MAGIC,
        VERSION,
        HEADER_SIZE,
        HEADER_SIZE + len(payload),
        sample_rate,
        2,
        0,
        0,
        len(modes),
        MODE_STRIDE,
        0,
        0,
        *payload,
    ]
    write_model(arguments.output, model)

    report_path = arguments.report or arguments.output.with_suffix(".report.txt")
    report = f"""CamaraObscura deterministic modal extraction

Input: {arguments.input}
Source format: {sample_rate} Hz, {channels} channel(s)
Detected direct sample: {int(diagnostics['direct_sample'])}
FFT size: {int(diagnostics['fft_size'])}
Extracted modes: {len(modes)}
Frequency range: {modes[0][0]:.6f} .. {modes[-1][0]:.6f} Hz
Broadband T60: {diagnostics['broadband_t60']:.6f} s (R^2 {diagnostics['broadband_r2']:.6f})
Low-band T60: {diagnostics['low_t60']:.6f} s (R^2 {diagnostics['low_r2']:.6f})
High-band T60: {diagnostics['high_t60']:.6f} s (R^2 {diagnostics['high_r2']:.6f})
Prepared model: {arguments.output}

Method and limits:
- Frequencies and phases come from parabolically refined Hann-window FFT peaks.
- Residues are square-root magnitude weighted and energy-normalized.
- Decays interpolate between deterministic two-band Schroeder fits. This is
  a compact modal approximation, not a matrix-pencil or ESPRIT estimator.
- Closely spaced modes below one FFT bin may merge; noisy peaks may be kept.
"""
    report_path.write_text(report, encoding="utf-8")
    if arguments.verification:
        verification = render_modes(
            modes, sample_rate, arguments.render_seconds
        )
        write_pcm16_wav(
            arguments.verification, verification, sample_rate
        )
    print(report, end="")


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        prog="modal_extract",
        description="Extract a versioned ModalReverbModel from an impulse response.",
    )
    subparsers = result.add_subparsers(dest="command", required=True)
    command = subparsers.add_parser("analyze")
    command.add_argument("input", type=Path)
    command.add_argument("output", type=Path)
    command.add_argument("--modes", type=int, default=512)
    command.add_argument("--minimum-frequency", type=float, default=40.0)
    command.add_argument("--maximum-frequency", type=float)
    command.add_argument("--maximum-fft", type=int, default=131072)
    command.add_argument("--crossover", type=float, default=2000.0)
    command.add_argument("--report", type=Path)
    command.add_argument("--verification", type=Path)
    command.add_argument("--render-seconds", type=float, default=2.0)
    command.set_defaults(function=analyze)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        arguments.function(arguments)
    except (OSError, ValueError) as error:
        print(f"modal_extract: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
