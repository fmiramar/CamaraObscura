#!/usr/bin/env python3
"""Dependency-free utilities shared by CamaraObscura offline tools."""

from __future__ import annotations

import cmath
import math
import struct
import wave
from pathlib import Path
from typing import Iterable, Sequence

MODEL_PREAMBLE = "CREATIVE_REVERBS_MODEL"


def read_pcm_wav(path: Path) -> tuple[list[float], int, int]:
    """Read integer PCM WAV and return mono samples, rate, source channels."""
    with wave.open(str(path), "rb") as source:
        channels = source.getnchannels()
        sample_rate = source.getframerate()
        width = source.getsampwidth()
        frame_count = source.getnframes()
        compression = source.getcomptype()
        raw = source.readframes(frame_count)
    if compression != "NONE":
        raise ValueError("compressed WAV files are not supported")
    if channels < 1 or sample_rate < 8000:
        raise ValueError("invalid channel count or sample rate")
    if width not in (1, 2, 3, 4):
        raise ValueError("only 8-, 16-, 24-, and 32-bit integer PCM is supported")

    scale = float(1 << (width * 8 - 1))
    values: list[float] = []
    if width == 1:
        values = [(value - 128) / 128.0 for value in raw]
    elif width == 2:
        count = len(raw) // 2
        values = [
            value / scale
            for value in struct.unpack(f"<{count}h", raw)
        ]
    elif width == 3:
        for offset in range(0, len(raw), 3):
            value = int.from_bytes(
                raw[offset : offset + 3], "little", signed=True
            )
            values.append(value / scale)
    else:
        count = len(raw) // 4
        values = [
            value / scale
            for value in struct.unpack(f"<{count}i", raw)
        ]

    mono = []
    for frame in range(len(values) // channels):
        start = frame * channels
        mono.append(sum(values[start : start + channels]) / channels)
    if not mono or max(abs(value) for value in mono) < 1.0e-12:
        raise ValueError("WAV contains no usable signal")
    return mono, sample_rate, channels


def write_pcm16_wav(
    path: Path, channels: Sequence[Sequence[float]], sample_rate: int
) -> None:
    if not channels or not channels[0]:
        raise ValueError("cannot write an empty WAV")
    frame_count = min(len(channel) for channel in channels)
    peak = max(
        abs(value)
        for channel in channels
        for value in channel[:frame_count]
    )
    scale = 0.98 / max(peak, 1.0)
    interleaved = bytearray()
    for frame in range(frame_count):
        for channel in channels:
            value = max(-1.0, min(1.0, channel[frame] * scale))
            interleaved.extend(
                struct.pack("<h", int(round(value * 32767.0)))
            )
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as destination:
        destination.setnchannels(len(channels))
        destination.setsampwidth(2)
        destination.setframerate(sample_rate)
        destination.writeframes(bytes(interleaved))


def write_model(path: Path, values: Iterable[float]) -> None:
    data = list(values)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(f"{MODEL_PREAMBLE}\n")
        for value in data:
            output.write(f"{float(value):.17g}\n")


def read_model(path: Path) -> list[float]:
    lines = [
        line.strip()
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if not lines or lines[0] != MODEL_PREAMBLE:
        raise ValueError("not a CamaraObscura prepared-model file")
    values = [float(line) for line in lines[1:]]
    if len(values) < 12:
        raise ValueError("prepared model is shorter than its header")
    if int(values[1]) != 1 or int(values[2]) != 12:
        raise ValueError("unsupported model version or header size")
    if int(values[3]) != len(values):
        raise ValueError("prepared model total-size field does not match")
    if any(not math.isfinite(value) for value in values):
        raise ValueError("prepared model contains a non-finite value")
    return values


def next_prime(value: int) -> int:
    candidate = max(2, int(value))
    if candidate == 2:
        return 2
    if candidate % 2 == 0:
        candidate += 1
    while True:
        limit = int(math.sqrt(candidate))
        if all(candidate % divisor for divisor in range(3, limit + 1, 2)):
            return candidate
        candidate += 2


class Lcg:
    """Matches the deterministic 24-bit generator used by sclang models."""

    def __init__(self, seed: int) -> None:
        self.state = abs(int(seed)) % 16_777_216
        if self.state == 0:
            self.state = 10_317

    def next(self) -> float:
        self.state = (
            self.state * 1_664_525 + 1_013_904_223
        ) % 16_777_216
        return self.state / 16_777_216.0


def schroeder_t60(
    samples: Sequence[float], sample_rate: int
) -> tuple[float, float, int]:
    """Return extrapolated energy T60, regression R², and fit point count."""
    if len(samples) < 64:
        return 1.0, 0.0, 0
    integrated = [0.0] * len(samples)
    running = 0.0
    for index in range(len(samples) - 1, -1, -1):
        running += float(samples[index]) ** 2
        integrated[index] = running
    reference = integrated[0]
    if reference <= 1.0e-24:
        return 1.0, 0.0, 0
    decibels = [
        10.0 * math.log10(max(value / reference, 1.0e-15))
        for value in integrated
    ]
    points = [
        (index / sample_rate, value)
        for index, value in enumerate(decibels)
        if -35.0 <= value <= -5.0
    ]
    if len(points) < 32:
        points = [
            (index / sample_rate, value)
            for index, value in enumerate(decibels)
            if -25.0 <= value <= -5.0
        ]
    if len(points) < 8:
        return 1.0, 0.0, len(points)
    mean_x = sum(point[0] for point in points) / len(points)
    mean_y = sum(point[1] for point in points) / len(points)
    covariance = sum(
        (x - mean_x) * (y - mean_y) for x, y in points
    )
    variance_x = sum((x - mean_x) ** 2 for x, _ in points)
    slope = covariance / max(variance_x, 1.0e-24)
    if slope >= -0.01:
        return 120.0, 0.0, len(points)
    intercept = mean_y - slope * mean_x
    residual = sum(
        (y - (intercept + slope * x)) ** 2 for x, y in points
    )
    total = sum((y - mean_y) ** 2 for _, y in points)
    r_squared = 1.0 - residual / max(total, 1.0e-24)
    return max(0.05, min(120.0, -60.0 / slope)), r_squared, len(points)


def split_low_high(
    samples: Sequence[float], sample_rate: int, crossover: float = 2000.0
) -> tuple[list[float], list[float]]:
    pole = math.exp(-2.0 * math.pi * crossover / sample_rate)
    state = 0.0
    low: list[float] = []
    high: list[float] = []
    for sample in samples:
        state = (1.0 - pole) * sample + pole * state
        low.append(state)
        high.append(sample - state)
    return low, high


def radix2_fft(values: Sequence[complex]) -> list[complex]:
    size = len(values)
    if size < 2 or size & (size - 1):
        raise ValueError("FFT size must be a power of two")
    data = list(values)
    reverse = 0
    for index in range(1, size):
        bit = size >> 1
        while reverse & bit:
            reverse ^= bit
            bit >>= 1
        reverse ^= bit
        if index < reverse:
            data[index], data[reverse] = data[reverse], data[index]
    length = 2
    while length <= size:
        step = cmath.exp(-2j * math.pi / length)
        half = length // 2
        for start in range(0, size, length):
            weight = 1.0 + 0.0j
            for offset in range(half):
                even = data[start + offset]
                odd = data[start + offset + half] * weight
                data[start + offset] = even + odd
                data[start + offset + half] = even - odd
                weight *= step
        length *= 2
    return data

