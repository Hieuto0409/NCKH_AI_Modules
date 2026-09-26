#!/usr/bin/env python3
"""Compare a captured C replay CSV with an optional Python SQI CSV.

The capture tool writes a stable raw schema in both RAW_ONLY and FULL serial
modes. This script is diagnostic only: C uses causal filters while the Python
reference may use offline/zero-phase filtering, so exact sample equality is
not expected.
"""

import argparse
from pathlib import Path

import numpy as np
import pandas as pd


RAW_COLUMNS = {
    "timestamp_us", "sample_index", "ecg_raw", "red_raw", "ir_raw",
    "window_ready", "dropped_samples",
}


def load_c_csv(path: Path) -> pd.DataFrame:
    data = pd.read_csv(path)
    missing = RAW_COLUMNS - set(data.columns)
    if missing:
        raise ValueError(f"C CSV missing columns: {sorted(missing)}")
    return data


def estimate_fs(data: pd.DataFrame) -> float:
    timestamps = data["timestamp_us"].to_numpy(dtype=np.int64)
    diffs = np.diff(timestamps)
    valid = diffs[(diffs > 0) & (diffs < 4_000_000_000)]
    if valid.size == 0:
        return float("nan")
    return 1_000_000.0 / float(np.median(valid))


def print_c_report(data: pd.DataFrame) -> None:
    print(f"[C] samples         : {len(data)}")
    print(f"[C] fs_median_hz    : {estimate_fs(data):.3f}")
    print(f"[C] windows_ready   : {int(data['window_ready'].sum())}")
    for column in ("ecg_raw", "red_raw", "ir_raw"):
        values = data[column].to_numpy(dtype=float)
        print(f"[C] {column:8s} min={values.min():.3f} "
              f"max={values.max():.3f} std={values.std():.3f}")


def print_python_report(path: Path | None) -> None:
    if path is None:
        print("\n[Py] no Python CSV provided; C-only diagnostic")
        return
    if not path.exists():
        raise FileNotFoundError(path)
    data = pd.read_csv(path)
    print(f"\n[Py] rows           : {len(data)}")
    for column in ("ppg_red_perfusion", "ppg_ir_perfusion"):
        if column in data.columns:
            print(f"[Py] {column:20s} median="
                  f"{data[column].median():.6f}")
    if "ecg_quality" in data.columns:
        print(f"[Py] ecg_good_fraction: "
              f"{data['ecg_quality'].eq('good').mean():.4f}")
    if "ppg_quality" in data.columns:
        print(f"[Py] ppg_good_fraction: "
              f"{data['ppg_quality'].eq('good').mean():.4f}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--c-csv", required=True, type=Path)
    parser.add_argument("--python-csv", type=Path)
    args = parser.parse_args()
    if not args.c_csv.exists():
        parser.error(f"C CSV not found: {args.c_csv}")
    c_data = load_c_csv(args.c_csv)
    print_c_report(c_data)
    print_python_report(args.python_csv)
    print("\nNOTE: C filtering is causal; Python may be zero-phase. "
          "This is a diagnostic comparison, not equivalence or clinical "
          "validation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
