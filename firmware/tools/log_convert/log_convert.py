#!/usr/bin/env python3
"""Convert PPGFW fixed-size binary frames to a lossless CSV table."""

from __future__ import annotations

import argparse
import csv
import pathlib
import struct
import sys


FRAME = struct.Struct("<HBBQIiiHH")
SYNC = 0xA55A


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def records(raw: bytes):
    for offset in range(0, len(raw) - FRAME.size + 1, FRAME.size):
        frame = raw[offset : offset + FRAME.size]
        fields = FRAME.unpack(frame)
        if fields[0] != SYNC:
            raise ValueError(f"sync mismatch at byte {offset}")
        if crc16_ccitt(frame[:-2]) != fields[-1]:
            raise ValueError(f"CRC mismatch at byte {offset}")
        yield fields
    if len(raw) % FRAME.size:
        raise ValueError(f"trailing {len(raw) % FRAME.size} byte(s)")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    with args.output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            ["sync", "schema", "record_type", "timestamp_us", "sequence", "value_a", "value_b", "flags", "checksum"]
        )
        writer.writerows(records(args.input.read_bytes()))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as error:
        print(error, file=sys.stderr)
        raise SystemExit(2)
