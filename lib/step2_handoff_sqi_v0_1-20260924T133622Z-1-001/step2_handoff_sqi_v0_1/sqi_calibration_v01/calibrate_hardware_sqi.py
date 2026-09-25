#!/usr/bin/env python3
"""First-pass SQI calibration for the real ECG/PPG hardware CSV schema.

This is an offline calibration tool.  It never changes the raw CSV and does
not belong in the capture firmware.  ECG and PPG are processed on their own
sample-rate axes because the hardware capture is 500 Hz ECG and 25 Hz PPG.

The thresholds in this file are explicitly provisional hardware v0.1 values.
They are intended to separate the available clean and degraded captures, not
to make a medical-quality claim.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from collections import Counter
from pathlib import Path

import numpy as np
from scipy.signal import butter, find_peaks, periodogram, sosfiltfilt


WINDOW_SECONDS = 5.0
STEP_SECONDS = 2.5
ECG_FS_HZ = 500.0
PPG_FS_HZ = 25.0

# Hardware v0.1 provisional gates.  Slot names stay slot0/slot1 until the
# controlled finger-on/finger-off mapping test verifies Red versus IR.
THRESHOLDS = {
    "ecg_clean_fraction_min": 0.80,
    "ecg_adc_invalid_fraction_max": 0.005,
    "ecg_rail_fraction_max": 0.005,
    "ecg_flat_ratio_max": 0.05,
    "ppg_slot0_perfusion_min": 0.0010,
    "ppg_slot1_perfusion_min": 0.0024,
    "ppg_flat_ratio_max": 0.05,
    "ppg_min_peaks_per_window": 3,
    "ppg_fifo_valid_fraction_min": 1.0,
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def bandpass(x: np.ndarray, fs: float, low: float, high: float) -> np.ndarray:
    if len(x) < 12:
        return x - np.mean(x)
    sos = butter(3, [low, high], btype="bandpass", fs=fs, output="sos")
    padlen = min(3 * (2 * len(sos) + 1), len(x) - 2)
    if padlen <= 0:
        return x - np.mean(x)
    return sosfiltfilt(sos, x, padlen=padlen)


def flat_ratio(x: np.ndarray) -> float:
    if len(x) < 2:
        return 1.0
    # Raw ADC values are integer-valued.  Equal consecutive samples are a
    # useful first-pass flatline indicator for this capture hardware.
    return float(np.mean(np.diff(x) == 0))


def safe_skew(x: np.ndarray) -> float:
    y = x - np.mean(x)
    std = float(np.std(y))
    if std <= 1e-12:
        return 0.0
    return float(np.mean((y / std) ** 3))


def safe_kurtosis(x: np.ndarray) -> float:
    y = x - np.mean(x)
    std = float(np.std(y))
    if std <= 1e-12:
        return 0.0
    return float(np.mean((y / std) ** 4))


def window_bounds(n: int, fs: float) -> list[tuple[int, int]]:
    width = max(1, int(round(WINDOW_SECONDS * fs)))
    step = max(1, int(round(STEP_SECONDS * fs)))
    return [(start, start + width) for start in range(0, n - width + 1, step)]


def ecg_rows(path: Path, fs: float = ECG_FS_HZ) -> list[dict[str, object]]:
    rows = read_csv(path)
    x = np.asarray([int(row["ecg_raw"]) for row in rows], dtype=float)
    # Filter the complete record once, then slice windows.  Filtering each
    # short window independently would create artificial edge transients.
    morph_all = bandpass(x, fs, 0.5, 35.0)
    qrs_all = bandpass(x, fs, 5.0, 20.0)
    output: list[dict[str, object]] = []
    for window_id, (start, end) in enumerate(window_bounds(len(rows), fs)):
        raw = x[start:end]
        part = rows[start:end]
        morph = morph_all[start:end]
        qrs = qrs_all[start:end]
        freq, power = periodogram(qrs, fs=fs, detrend="constant")
        total = float(power[(freq >= 1.0) & (freq <= 40.0)].sum())
        qrs_band = float(power[(freq >= 5.0) & (freq <= 20.0)].sum())
        peaks, _ = find_peaks(
            qrs,
            distance=max(1, int(0.25 * fs)),
            prominence=max(1e-9, 0.25 * float(np.std(qrs))),
        )
        clean_fraction = float(
            np.mean([row["leads_off"] == "0" and row["adc_ok"] == "1" for row in part])
        )
        adc_invalid_fraction = float(np.mean([row["adc_ok"] == "0" for row in part]))
        rail_fraction = float(np.mean((raw <= 10.0) | (raw >= 4085.0)))
        row = {
            "file": path.name,
            "window_id": window_id,
            "start_sample": start,
            "end_sample": end,
            "start_s": start / fs,
            "end_s": end / fs,
            "clean_fraction_from_flags": clean_fraction,
            "adc_invalid_fraction": adc_invalid_fraction,
            "rail_fraction": rail_fraction,
            "flat_ratio": flat_ratio(raw),
            "ecg_morph_rms": float(np.sqrt(np.mean(morph * morph))),
            "ecg_qrs_rms": float(np.sqrt(np.mean(qrs * qrs))),
            "ecg_qrs_band_ratio": qrs_band / total if total > 1e-12 else 0.0,
            "ecg_kurtosis": safe_kurtosis(morph),
            "ecg_skew": safe_skew(morph),
            "qrs_peak_count": int(len(peaks)),
        }
        row["reference_good"] = bool(
            clean_fraction >= THRESHOLDS["ecg_clean_fraction_min"]
            and adc_invalid_fraction <= THRESHOLDS["ecg_adc_invalid_fraction_max"]
            and rail_fraction <= THRESHOLDS["ecg_rail_fraction_max"]
        )
        row["ecg_good_v01"] = bool(
            row["reference_good"] and row["flat_ratio"] <= THRESHOLDS["ecg_flat_ratio_max"]
        )
        output.append(row)
    return output


def ppg_rows(path: Path, fs: float = PPG_FS_HZ) -> list[dict[str, object]]:
    rows = read_csv(path)
    raw_all = {
        slot: np.asarray([int(row[slot]) for row in rows], dtype=float)
        for slot in ("slot0_raw", "slot1_raw")
    }
    ac_all = {slot: bandpass(raw_all[slot], fs, 0.5, 8.0) for slot in raw_all}
    output: list[dict[str, object]] = []
    bounds = window_bounds(len(rows), fs)
    for window_id, (start, end) in enumerate(bounds):
        part = rows[start:end]
        metrics: dict[str, object] = {
            "file": path.name,
            "window_id": window_id,
            "start_sample": start,
            "end_sample": end,
            "start_s": start / fs,
            "end_s": end / fs,
        }
        fifo_valid_fraction = float(np.mean([row["fifo_ok"] == "1" for row in part]))
        metrics["fifo_valid_fraction"] = fifo_valid_fraction
        metrics["fifo_overflow_max"] = max(int(row.get("fifo_overflow_count", "0")) for row in part)
        good = fifo_valid_fraction >= THRESHOLDS["ppg_fifo_valid_fraction_min"]
        for slot, threshold_key in (("slot0_raw", "ppg_slot0_perfusion_min"), ("slot1_raw", "ppg_slot1_perfusion_min")):
            raw = raw_all[slot][start:end]
            ac = ac_all[slot][start:end]
            dc = abs(float(np.mean(raw)))
            perfusion = float(np.std(ac) / (dc + 1e-9))
            peaks, _ = find_peaks(
                ac,
                distance=max(1, int(0.30 * fs)),
                prominence=max(1e-9, 0.25 * float(np.std(ac))),
            )
            rail = float(np.mean((raw <= 0.0) | (raw >= 262143.0)))
            flat = flat_ratio(raw)
            metrics[f"{slot}_perfusion"] = perfusion
            metrics[f"{slot}_flat_ratio"] = flat
            metrics[f"{slot}_rail_fraction"] = rail
            metrics[f"{slot}_peak_count"] = int(len(peaks))
            good = bool(
                good
                and perfusion >= THRESHOLDS[threshold_key]
                and flat <= THRESHOLDS["ppg_flat_ratio_max"]
                and rail == 0.0
                and len(peaks) >= THRESHOLDS["ppg_min_peaks_per_window"]
            )
        metrics["ppg_good_v01"] = good
        metrics["finger_contact_available"] = any(row.get("finger_contact") not in (None, "", "-1") for row in part)
        output.append(metrics)
    return output


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fields = list(rows[0].keys())
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ecg", action="append", type=Path, default=[], help="ECG CSV; repeat for multiple captures")
    parser.add_argument("--ppg", action="append", type=Path, default=[], help="PPG CSV; repeat for multiple captures")
    parser.add_argument("--out-dir", type=Path, default=Path("sqi_calibration"))
    args = parser.parse_args()
    if not args.ecg and not args.ppg:
        parser.error("provide at least one --ecg or --ppg CSV")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    ecg = [metric for path in args.ecg for metric in ecg_rows(path)]
    ppg = [metric for path in args.ppg for metric in ppg_rows(path)]
    write_csv(args.out_dir / "ecg_sqi_windows.csv", ecg)
    write_csv(args.out_dir / "ppg_sqi_windows.csv", ppg)

    summary = {
        "status": "provisional_hardware_sqi_v0.1",
        "window_seconds": WINDOW_SECONDS,
        "step_seconds": STEP_SECONDS,
        "ecg_fs_hz": ECG_FS_HZ,
        "ppg_fs_hz": PPG_FS_HZ,
        "thresholds": THRESHOLDS,
        "ecg_windows": len(ecg),
        "ecg_reference_good_windows": int(sum(row["reference_good"] for row in ecg)),
        "ecg_good_v01_windows": int(sum(row["ecg_good_v01"] for row in ecg)),
        "ppg_windows": len(ppg),
        "ppg_good_v01_windows": int(sum(row["ppg_good_v01"] for row in ppg)),
        "ppg_contact_flag_available": bool(any(row["finger_contact_available"] for row in ppg)),
        "limitations": [
            "ECG reference labels use capture hardware flags plus rail/invalid ratios.",
            "PPG set has no deliberate finger-off negative capture, so PPG thresholds are lower-quantile starting values only.",
            "slot0_raw and slot1_raw are not renamed Red/IR until a controlled mapping test.",
            "This is signal-quality gating, not a medical accuracy or diagnosis claim.",
        ],
    }
    (args.out_dir / "sqi_thresholds_v0_1.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"ECG windows: {len(ecg)} | v0.1 good: {summary['ecg_good_v01_windows']}")
    print(f"PPG windows: {len(ppg)} | v0.1 good: {summary['ppg_good_v01_windows']}")
    print(f"Output: {args.out_dir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
