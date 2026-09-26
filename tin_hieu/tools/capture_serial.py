#!/usr/bin/env python3
"""Capture RAW_ONLY (S,) or FULL (SAMPLE,) rows into a replay CSV."""

import argparse
import csv
import time

import serial


RAW_HEADER = [
    "timestamp_us", "sample_index", "ecg_raw", "red_raw", "ir_raw",
    "window_ready", "dropped_samples",
]
FULL_HEADER = [
    "timestamp_us", "sample_index", "ecg_raw", "red_raw", "ir_raw",
    "ecg_filtered", "ecg_qrs", "red_ac", "ir_ac", "red_dc", "ir_dc",
    "window_ready", "ecg_good", "ppg_good", "both_good",
    "quality_reason_mask", "window_gap_count", "gap_count",
    "dropped_samples",
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="Example: COM13")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--out", default="raw_hw.csv")
    parser.add_argument("--seconds", type=float, default=120.0)
    args = parser.parse_args()

    deadline = time.monotonic() + args.seconds
    rows = 0

    with serial.Serial(args.port, args.baud, timeout=1.0) as port:
        time.sleep(2.0)  # allow board reset after opening the port
        with open(args.out, "w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(RAW_HEADER)

            while time.monotonic() < deadline:
                line = port.readline().decode("utf-8", errors="ignore").strip()
                fields = line.split(",")
                if line.startswith("S,") and len(fields) == len(RAW_HEADER) + 1:
                    writer.writerow(fields[1:])
                    rows += 1
                    continue
                if line.startswith("SAMPLE,") and len(fields) == len(FULL_HEADER) + 1:
                    # Keep one stable raw replay schema. Full filtered fields
                    # are preserved only when the user captures debug mode.
                    writer.writerow([
                        fields[1], fields[2], fields[3], fields[4], fields[5],
                        fields[12], fields[19],
                    ])
                    rows += 1
                    continue

    print(f"Saved {rows} samples to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
