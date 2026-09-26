#!/usr/bin/env python3
"""Basic sampling-rate and gap report for a captured SAMPLE CSV."""

import argparse

import numpy as np
import pandas as pd


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path")
    args = parser.parse_args()

    data = pd.read_csv(args.csv_path)
    if len(data) < 2:
        print("Need at least two samples.")
        return 1

    ts = data["timestamp_us"].to_numpy(dtype=np.int64)
    dt = np.diff(ts)
    valid = dt[dt > 0]
    fs = 1_000_000.0 / float(np.median(valid))

    print(f"samples: {len(data)}")
    print(f"duration_s: {(ts[-1] - ts[0]) / 1_000_000.0:.3f}")
    print(f"fs_median_hz: {fs:.3f}")
    print(f"dt_min_us: {int(valid.min())}")
    print(f"dt_median_us: {int(np.median(valid))}")
    print(f"dt_max_us: {int(valid.max())}")
    if "gap_count" in data.columns:
        gaps = int((data["gap_count"].diff() > 0).sum())
    else:
        expected = float(np.median(valid))
        gaps = int((dt > 2.0 * expected).sum())
    print(f"timestamp_gaps: {gaps}")
    for column in ("ecg_raw", "red_raw", "ir_raw"):
        print(f"{column}: min={data[column].min():.3f}, max={data[column].max():.3f}, std={data[column].std():.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
