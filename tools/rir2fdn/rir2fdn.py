#!/usr/bin/env python3
"""Deterministically fit a compact late-field RIRFDN prepared model."""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path
from typing import Sequence

TOOLS_ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(TOOLS_ROOT))

from model_common import (  # noqa: E402
    Lcg,
    next_prime,
    read_pcm_wav,
    schroeder_t60,
    split_low_high,
    write_model,
    write_pcm16_wav,
)

MAGIC = 444_004
VERSION = 1
HEADER_SIZE = 12
LINE_STRIDE = 8


def design_lines(
    count: int,
    sample_rate: int,
    low_t60: float,
    high_t60: float,
    seed: int,
) -> list[list[float]]:
    random = Lcg(seed)
    result: list[list[float]] = []
    scale = math.sqrt(count)
    for line in range(count):
        fraction = (line + random.next()) / count
        seconds = 0.031 + (0.109 - 0.031) * fraction
        delay = next_prime(round(seconds * sample_rate))
        pan = -1.0 if line % 2 == 0 else 1.0
        result.append(
            [
                float(delay),
                0.0,
                low_t60,
                high_t60,
                (1.0 if line % 2 == 0 else -1.0) / scale,
                (1.0 - pan * 0.35) / scale,
                (1.0 + pan * 0.35) / scale,
                3500.0 + random.next() * 4500.0,
            ]
        )
    return result


def delay_gain(delay: int, t60: float, sample_rate: int) -> float:
    return 10.0 ** (-3.0 * delay / (max(t60, 0.02) * sample_rate))


def render_verification(
    lines: Sequence[Sequence[float]],
    sample_rate: int,
    duration: float,
) -> tuple[list[float], list[float]]:
    lengths = [int(line[0]) for line in lines]
    storage = [[0.0] * length for length in lengths]
    positions = [0] * len(lines)
    low_state = [0.0] * len(lines)
    frame_count = max(1, round(duration * sample_rate))
    left = [0.0] * frame_count
    right = [0.0] * frame_count
    output_scale = 1.0 / math.sqrt(len(lines))
    for frame in range(frame_count):
        delayed = [
            storage[line][positions[line]]
            for line in range(len(lines))
        ]
        for line, value in enumerate(delayed):
            left[frame] += value * lines[line][5] * output_scale
            right[frame] += value * lines[line][6] * output_scale
        filtered: list[float] = []
        for line, value in enumerate(delayed):
            cutoff = lines[line][7]
            pole = math.exp(-2.0 * math.pi * cutoff / sample_rate)
            low_state[line] = (
                (1.0 - pole) * value + pole * low_state[line]
            )
            low_gain = delay_gain(
                lengths[line], lines[line][2], sample_rate
            )
            high_gain = delay_gain(
                lengths[line], lines[line][3], sample_rate
            )
            filtered.append(
                high_gain * value
                + (low_gain - high_gain) * low_state[line]
            )
        average = 2.0 * sum(filtered) / len(filtered)
        mixed = [value - average for value in filtered]
        excitation = 1.0 if frame == 0 else 0.0
        for line in range(len(lines)):
            storage[line][positions[line]] = (
                mixed[line] + excitation * lines[line][4]
            )
            positions[line] = (positions[line] + 1) % lengths[line]
    return left, right


def analyze(arguments: argparse.Namespace) -> None:
    samples, source_rate, source_channels = read_pcm_wav(arguments.input)
    direct_index = max(range(len(samples)), key=lambda index: abs(samples[index]))
    late_index = direct_index + round(arguments.late_start * source_rate)
    if late_index >= len(samples) - 64:
        raise ValueError("late start lies beyond the usable impulse response")
    late = samples[late_index:]
    low, high = split_low_high(late, source_rate, arguments.crossover)
    broadband_t60, broadband_r2, broadband_points = schroeder_t60(
        late, source_rate
    )
    low_t60, low_r2, low_points = schroeder_t60(low, source_rate)
    high_t60, high_r2, high_points = schroeder_t60(high, source_rate)
    target_rate = arguments.sample_rate or source_rate
    if not 8000 <= target_rate <= 768000:
        raise ValueError("target sample rate is outside 8000..768000 Hz")
    if not 2 <= arguments.delays <= 32:
        raise ValueError("delay count is outside 2..32")
    lines = design_lines(
        arguments.delays,
        target_rate,
        low_t60,
        high_t60,
        arguments.seed,
    )
    payload = [math.pi / 4.0]
    for line in lines:
        payload.extend(line)
    total_size = HEADER_SIZE + len(payload)
    model = [
        MAGIC,
        VERSION,
        HEADER_SIZE,
        total_size,
        target_rate,
        2,
        1,
        arguments.seed % 16_777_216,
        arguments.delays,
        1,
        arguments.delays,
        LINE_STRIDE,
        *payload,
    ]
    write_model(arguments.output, model)

    verification_path = arguments.verification or arguments.output.with_suffix(
        ".verification.wav"
    )
    report_path = arguments.report or arguments.output.with_suffix(".report.txt")
    render_seconds = min(
        arguments.render_seconds,
        max(1.0, broadband_t60 * 1.25),
    )
    verification = render_verification(lines, target_rate, render_seconds)
    write_pcm16_wav(verification_path, verification, target_rate)

    original_bytes = len(samples) * source_channels * 4
    model_bytes = len(model) * 4
    report = f"""CamaraObscura deterministic RIR-to-FDN analysis

Input: {arguments.input}
Source format: {source_rate} Hz, {source_channels} channel(s)
Direct-arrival sample: {direct_index}
Late start: {arguments.late_start:.6f} s after detected peak
Late samples analyzed: {len(late)}
Target renderer rate: {target_rate} Hz
Delay lines: {arguments.delays}
Seed: {arguments.seed % 16_777_216}
Crossover: {arguments.crossover:.3f} Hz

Broadband energy T60: {broadband_t60:.6f} s
Broadband regression R^2: {broadband_r2:.6f} ({broadband_points} points)
Low-band energy T60: {low_t60:.6f} s
Low-band regression R^2: {low_r2:.6f} ({low_points} points)
High-band energy T60: {high_t60:.6f} s
High-band regression R^2: {high_r2:.6f} ({high_points} points)

Prepared model: {arguments.output}
Verification render: {verification_path}
Approximate float32 source memory: {original_bytes} bytes
Prepared model memory: {model_bytes} bytes
Compression ratio: {original_bytes / max(model_bytes, 1):.3f}:1

Method and limits:
- Direct arrival is the largest absolute mono sample.
- T60 values are linear regressions of Schroeder energy decay, normally
  over -5 to -35 dB, extrapolated to -60 dB.
- Low/high estimates use a deterministic one-pole split at the requested
  crossover. This first fitter matches broad two-band coloration; it does
  not reproduce early reflections, fine comb structure, or directional cues.
- The verification WAV is peak-normalized only while writing PCM16. The
  prepared model values are not modified by that normalization.
"""
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(report, encoding="utf-8")
    print(report, end="")


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        prog="rir2fdn",
        description="Fit a deterministic late-only RIRFDN prepared model.",
    )
    subparsers = result.add_subparsers(dest="command", required=True)
    command = subparsers.add_parser("analyze")
    command.add_argument("input", type=Path)
    command.add_argument("output", type=Path)
    command.add_argument("--late-start", type=float, default=0.08)
    command.add_argument("--delays", type=int, default=8)
    command.add_argument("--sample-rate", type=int)
    command.add_argument("--seed", type=int, default=0)
    command.add_argument("--crossover", type=float, default=2000.0)
    command.add_argument("--render-seconds", type=float, default=8.0)
    command.add_argument("--verification", type=Path)
    command.add_argument("--report", type=Path)
    command.set_defaults(function=analyze)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        arguments.function(arguments)
    except (OSError, ValueError) as error:
        print(f"rir2fdn: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
