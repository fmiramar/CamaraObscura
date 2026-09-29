#!/usr/bin/env python3
"""End-to-end deterministic tests for CamaraObscura offline tools."""

from __future__ import annotations

import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]
RIR_TOOL = PROJECT / "tools" / "rir2fdn" / "rir2fdn.py"
MODAL_TOOL = PROJECT / "tools" / "modal_extract" / "modal_extract.py"
INSPECT_TOOL = PROJECT / "tools" / "model_inspect" / "model_inspect.py"


def write_fixture(path: Path, sample_rate: int = 16000) -> None:
    frames = sample_rate * 2
    data = bytearray()
    for frame in range(frames):
        time = frame / sample_rate
        direct = 0.8 if frame == 0 else 0.0
        low = 0.35 * math.sin(2.0 * math.pi * 311.0 * time)
        high = 0.16 * math.sin(2.0 * math.pi * 3023.0 * time)
        value = direct + (
            low * math.exp(-6.907755 * time / 0.8)
            + high * math.exp(-6.907755 * time / 0.35)
        )
        value = max(-1.0, min(1.0, value))
        data.extend(struct.pack("<h", int(round(value * 32767.0))))
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(bytes(data))


def run(*arguments: object) -> str:
    completed = subprocess.run(
        [sys.executable, *(str(argument) for argument in arguments)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return completed.stdout


def read_header(path: Path) -> list[float]:
    lines = path.read_text(encoding="utf-8").splitlines()
    assert lines[0] == "CREATIVE_REVERBS_MODEL"
    values = [float(line) for line in lines[1:] if line]
    assert int(values[1]) == 1
    assert int(values[2]) == 12
    assert int(values[3]) == len(values)
    return values


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="creative_reverbs_tools_") as temp:
        directory = Path(temp)
        fixture = directory / "fixture.wav"
        write_fixture(fixture)

        first_rir = directory / "first.rfdn"
        second_rir = directory / "second.rfdn"
        run(
            RIR_TOOL,
            "analyze",
            fixture,
            first_rir,
            "--late-start",
            "0.02",
            "--delays",
            "8",
            "--sample-rate",
            "16000",
            "--render-seconds",
            "0.5",
        )
        run(
            RIR_TOOL,
            "analyze",
            fixture,
            second_rir,
            "--late-start",
            "0.02",
            "--delays",
            "8",
            "--sample-rate",
            "16000",
            "--render-seconds",
            "0.5",
        )
        assert first_rir.read_bytes() == second_rir.read_bytes()
        rir_values = read_header(first_rir)
        assert int(rir_values[0]) == 444004
        assert int(rir_values[8]) == 8
        assert 0.5 < rir_values[15] < 1.1
        assert 0.1 < rir_values[16] < 1.1
        assert rir_values[15] > rir_values[16]
        assert first_rir.with_suffix(".report.txt").exists()
        assert first_rir.with_suffix(".verification.wav").exists()

        modal = directory / "fixture.modal"
        run(
            MODAL_TOOL,
            "analyze",
            fixture,
            modal,
            "--modes",
            "16",
            "--maximum-fft",
            "16384",
        )
        modal_values = read_header(modal)
        assert int(modal_values[0]) == 444005
        assert int(modal_values[8]) == 16
        assert int(modal_values[9]) == 6
        frequencies = modal_values[12::6]
        assert min(abs(value - 311.0) for value in frequencies) < 4.0
        assert min(abs(value - 3023.0) for value in frequencies) < 4.0

        inspection = run(INSPECT_TOOL, modal)
        assert "family: ModalReverbModel" in inspection
        assert "payload_values: 96" in inspection
    print("CREATIVE_REVERBS_OFFLINE_TOOLS_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
