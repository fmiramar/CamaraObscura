#!/usr/bin/env python3
"""Inspect and validate a CamaraObscura prepared-model header."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(TOOLS_ROOT))

from model_common import read_model  # noqa: E402

FAMILIES = {
    444_001: "DarkVelvetProfile",
    444_002: "GroupedFDNModel",
    444_003: "VelvetFDNModel",
    444_004: "RIRFDNModel",
    444_005: "ModalReverbModel",
    444_006: "ModalPlateModel",
    444_007: "GeometryReverbScene",
}


def main() -> int:
    parser = argparse.ArgumentParser(prog="model_inspect")
    parser.add_argument("model", type=Path)
    arguments = parser.parse_args()
    try:
        values = read_model(arguments.model)
    except (OSError, ValueError) as error:
        print(f"model_inspect: {error}", file=sys.stderr)
        return 2
    magic = int(values[0])
    print(f"family: {FAMILIES.get(magic, 'unknown')}")
    print(f"magic: {magic}")
    print(f"version: {int(values[1])}")
    print(f"total_values: {int(values[3])}")
    print(f"sample_rate: {values[4]:.9g}")
    print(f"channels: {int(values[5])}")
    print(f"flags: {int(values[6])}")
    print(f"seed: {int(values[7])}")
    print(
        "fields: "
        + ", ".join(str(int(value)) for value in values[8:12])
    )
    print(f"payload_values: {len(values) - 12}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
