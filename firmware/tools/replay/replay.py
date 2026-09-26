#!/usr/bin/env python3
"""Integrity-first replay summary for CSV produced by tools/log_convert."""

from __future__ import annotations

import argparse
import csv
import pathlib
from collections import Counter


PPG = 1
ECG = 2


def stream_summary(rows: list[dict[str, str]], record_type: int) -> dict[str, int | float | None]:
    selected = [row for row in rows if int(row["record_type"]) == record_type]
    sequences = [int(row["sequence"]) for row in selected]
    timestamps = [int(row["timestamp_us"]) for row in selected]
    gaps = sum(max(0, right - left - 1) for left, right in zip(sequences, sequences[1:]))
    non_monotonic = sum(right <= left for left, right in zip(timestamps, timestamps[1:]))
    mean_period = None
    if len(timestamps) > 1:
        mean_period = (timestamps[-1] - timestamps[0]) / (len(timestamps) - 1)
    return {
        "samples": len(selected),
        "sequence_gaps": gaps,
        "non_monotonic_timestamps": non_monotonic,
        "mean_period_us": mean_period,
        "clipped": sum(bool(int(row["flags"]) & (1 << 1)) for row in selected),
    }


def window_summaries(rows: list[dict[str, str]]) -> list[dict]:
    names = {38: "ppg60", 39: "ecg30", 40: "spo2_latest_pairs"}
    windows = []
    for row in rows:
        kind = int(row["record_type"])
        if kind not in names:
            continue
        start = int(row["timestamp_us"])
        flags = int(row["flags"])
        windows.append({"branch": names[kind], "start_us": start,
                        "end_us": start + int(row["sequence"]),
                        "quality_reasons": int(row["value_a"]),
                        "received_samples": int(row["value_b"]),
                        "quality_status": flags & 255, "feature_or_estimator_status": flags >> 8,
                        "quality_scope": "ppg60_conservative" if kind == 40 else names[kind]})
    return windows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path, help="CSV from log_convert.py")
    args = parser.parse_args()
    with args.input.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    print("record_counts", dict(sorted(Counter(int(row["record_type"]) for row in rows).items())))
    print("ppg", stream_summary(rows, PPG))
    print("ecg", stream_summary(rows, ECG))
    for window in window_summaries(rows):
        print("window", window)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
