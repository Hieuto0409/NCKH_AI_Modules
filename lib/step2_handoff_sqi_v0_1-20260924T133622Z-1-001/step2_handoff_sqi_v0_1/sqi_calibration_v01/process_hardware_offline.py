#!/usr/bin/env python3
"""Offline processing runner for the frozen hardware ECG/PPG captures.

This runner intentionally keeps the two hardware sample-rate axes separate:
ECG is processed at 500 Hz and PPG at 25 Hz.  It never overwrites raw CSVs.
The SQI windows use the frozen v0.1 calibration rules from
``calibrate_hardware_sqi.py``.
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import Any

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy.signal import butter, find_peaks, sosfiltfilt


TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from calibrate_hardware_sqi import (  # noqa: E402
    ECG_FS_HZ,
    PPG_FS_HZ,
    THRESHOLDS,
    ecg_rows,
    ppg_rows,
    window_bounds,
)


def safe_filter(x: np.ndarray, fs: float, low: float, high: float, order: int = 2) -> np.ndarray:
    """Zero-phase Butterworth band-pass with a short-input fallback."""
    y = np.asarray(x, dtype=float)
    if len(y) < 12:
        return y - np.mean(y) if len(y) else y.copy()
    nyquist = fs / 2.0
    low = max(float(low), 1e-6)
    high = min(float(high), 0.98 * nyquist)
    if low >= high:
        raise ValueError(f"invalid band {low}..{high} Hz for fs={fs}")
    sos = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")
    padlen = min(3 * (2 * len(sos) + 1), len(y) - 2)
    return sosfiltfilt(sos, y, padlen=padlen) if padlen > 0 else y - np.mean(y)


def safe_lowpass(x: np.ndarray, fs: float, cutoff: float, order: int = 2) -> np.ndarray:
    """Zero-phase low-pass with a short-input fallback."""
    y = np.asarray(x, dtype=float)
    if len(y) < 12:
        return y - np.mean(y) if len(y) else y.copy()
    cutoff = min(max(float(cutoff), 1e-6), 0.98 * fs / 2.0)
    sos = butter(order, cutoff, btype="lowpass", fs=fs, output="sos")
    padlen = min(3 * (2 * len(sos) + 1), len(y) - 2)
    return sosfiltfilt(sos, y, padlen=padlen) if padlen > 0 else y - np.mean(y)


def ecg_r_peaks(ecg_morph: np.ndarray, ecg_qrs: np.ndarray, fs: float) -> np.ndarray:
    """Deterministic Pan-Tompkins-style R-peak baseline."""
    if len(ecg_morph) != len(ecg_qrs) or len(ecg_qrs) < 4:
        return np.asarray([], dtype=int)
    derivative = np.diff(ecg_qrs, prepend=ecg_qrs[0])
    energy = derivative * derivative
    width = max(1, int(round(0.15 * fs)))
    integrated = np.convolve(energy, np.ones(width) / width, mode="same")
    std = float(np.std(integrated))
    if std <= 1e-12:
        return np.asarray([], dtype=int)
    candidates, _ = find_peaks(
        integrated,
        height=float(np.median(integrated) + 0.5 * std),
        distance=max(1, int(round(0.25 * fs))),
        prominence=max(1e-12, 0.05 * std),
    )
    search = max(1, int(round(0.08 * fs)))
    peaks: list[int] = []
    for candidate in candidates:
        lo = max(0, int(candidate) - search)
        hi = min(len(ecg_morph), int(candidate) + search + 1)
        local = lo + int(np.argmax(np.abs(ecg_morph[lo:hi])))
        if not peaks or local - peaks[-1] >= int(round(0.25 * fs)):
            peaks.append(local)
    return np.asarray(peaks, dtype=int)


def ppg_peaks(ac: np.ndarray, fs: float) -> np.ndarray:
    """Pulse-peak detector suitable for the 25-Hz baseline."""
    if len(ac) < 4 or float(np.std(ac)) <= 1e-12:
        return np.asarray([], dtype=int)
    return find_peaks(
        ac,
        distance=max(1, int(round(0.30 * fs))),
        prominence=max(1e-9, 0.25 * float(np.std(ac))),
    )[0]


def rr_features(peaks: np.ndarray, fs: float) -> dict[str, float | int | None]:
    rr = np.diff(peaks) / fs if len(peaks) >= 2 else np.asarray([], dtype=float)
    valid = rr[(rr >= 0.30) & (rr <= 2.0)]
    if len(valid) == 0:
        return {
            "peak_count": int(len(peaks)),
            "rr_valid_count": 0,
            "bpm_median": None,
            "rr_sdnn_ms": None,
            "rr_rmssd_ms": None,
        }
    return {
        "peak_count": int(len(peaks)),
        "rr_valid_count": int(len(valid)),
        "bpm_median": float(60.0 / np.median(valid)),
        "rr_sdnn_ms": float(np.std(valid, ddof=1) * 1000.0) if len(valid) > 1 else 0.0,
        "rr_rmssd_ms": float(np.sqrt(np.mean(np.diff(valid) ** 2)) * 1000.0) if len(valid) > 2 else None,
    }


def validate_rows(df: pd.DataFrame, required: list[str], name: str) -> dict[str, Any]:
    missing = [col for col in required if col not in df.columns]
    if missing:
        raise ValueError(f"{name}: missing columns: {', '.join(missing)}")
    idx = df["sample_index"].to_numpy(dtype=np.int64)
    ts = df["timestamp_us"].to_numpy(dtype=np.int64)
    d_idx = np.diff(idx)
    d_ts = np.diff(ts)
    positive_dt = d_ts[d_ts > 0]
    dt_median = float(np.median(positive_dt)) if len(positive_dt) else None
    return {
        "rows": int(len(df)),
        "sample_index_first": int(idx[0]) if len(idx) else None,
        "sample_index_last": int(idx[-1]) if len(idx) else None,
        "missing_samples": int(max(0, (idx[-1] - idx[0] + 1) - len(set(idx)))) if len(idx) else 0,
        "index_regressions": int(np.sum(d_idx <= 0)) if len(d_idx) else 0,
        "timestamp_regressions": int(np.sum(d_ts <= 0)) if len(d_ts) else 0,
        "duration_s": float((ts[-1] - ts[0]) / 1_000_000.0) if len(ts) > 1 else 0.0,
        "dt_median_us": dt_median,
        "dt_max_us": int(np.max(positive_dt)) if len(positive_dt) else None,
        "fs_median_hz": float(1_000_000.0 / dt_median) if dt_median else None,
    }


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fields = list(rows[0].keys())
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def process_ecg(path: Path, out_dir: Path, label: str) -> dict[str, Any]:
    df = pd.read_csv(path)
    validation = validate_rows(df, ["sample_index", "timestamp_us", "ecg_raw", "leads_off", "adc_ok"], label)
    raw = df["ecg_raw"].to_numpy(dtype=float)
    morph = safe_filter(raw, ECG_FS_HZ, 0.5, 40.0)
    qrs = safe_filter(raw, ECG_FS_HZ, 5.0, 20.0)
    peaks = ecg_r_peaks(morph, qrs, ECG_FS_HZ)
    time_s = (df["timestamp_us"].to_numpy(dtype=np.int64) - int(df["timestamp_us"].iloc[0])) / 1_000_000.0

    processed = df.copy()
    processed.insert(0, "time_s", time_s)
    processed["ecg_morph"] = morph
    processed["ecg_qrs"] = qrs
    processed["r_peak"] = 0
    processed.loc[peaks, "r_peak"] = 1
    processed.to_csv(out_dir / f"{label}_ecg_processed.csv", index=False)

    sqi = ecg_rows(path)
    bounds = window_bounds(len(df), ECG_FS_HZ)
    window_rows: list[dict[str, Any]] = []
    for i, (start, end) in enumerate(bounds):
        p = peaks[(peaks >= start) & (peaks < end)] - start
        feats = rr_features(p, ECG_FS_HZ)
        base = dict(sqi[i]) if i < len(sqi) else {"window_id": i}
        base.update({"record_label": label, "r_peak_count_detected": feats["peak_count"], "bpm_median": feats["bpm_median"], "rr_valid_count": feats["rr_valid_count"], "rr_sdnn_ms": feats["rr_sdnn_ms"], "rr_rmssd_ms": feats["rr_rmssd_ms"]})
        window_rows.append(base)
    write_csv(out_dir / f"{label}_ecg_windows.csv", window_rows)

    record = {
        "record_label": label,
        "source": str(path),
        "channel": "ECG",
        "fs_hz_configured": ECG_FS_HZ,
        **validation,
        **rr_features(peaks, ECG_FS_HZ),
        "windows": len(window_rows),
        "sqi_good_windows": int(sum(bool(row.get("ecg_good_v01", False)) for row in window_rows)),
    }
    return record


def process_ppg(path: Path, out_dir: Path) -> dict[str, Any]:
    df = pd.read_csv(path)
    validation = validate_rows(df, ["sample_index", "timestamp_us", "slot0_raw", "slot1_raw", "fifo_ok"], "ppg")
    slot0 = df["slot0_raw"].to_numpy(dtype=float)
    slot1 = df["slot1_raw"].to_numpy(dtype=float)
    slot0_dc = safe_lowpass(slot0, PPG_FS_HZ, 0.5)
    slot1_dc = safe_lowpass(slot1, PPG_FS_HZ, 0.5)
    slot0_ac = safe_filter(slot0, PPG_FS_HZ, 0.5, 8.0)
    slot1_ac = safe_filter(slot1, PPG_FS_HZ, 0.5, 8.0)
    peaks0 = ppg_peaks(slot0_ac, PPG_FS_HZ)
    peaks1 = ppg_peaks(slot1_ac, PPG_FS_HZ)
    time_s = (df["timestamp_us"].to_numpy(dtype=np.int64) - int(df["timestamp_us"].iloc[0])) / 1_000_000.0

    processed = df.copy()
    processed.insert(0, "time_s", time_s)
    processed["slot0_dc"] = slot0_dc
    processed["slot0_ac"] = slot0_ac
    processed["slot1_dc"] = slot1_dc
    processed["slot1_ac"] = slot1_ac
    processed["slot0_peak"] = 0
    processed["slot1_peak"] = 0
    processed.loc[peaks0, "slot0_peak"] = 1
    processed.loc[peaks1, "slot1_peak"] = 1
    processed.to_csv(out_dir / "ppg_processed.csv", index=False)

    sqi = ppg_rows(path)
    bounds = window_bounds(len(df), PPG_FS_HZ)
    window_rows: list[dict[str, Any]] = []
    for i, (start, end) in enumerate(bounds):
        p0 = peaks0[(peaks0 >= start) & (peaks0 < end)] - start
        p1 = peaks1[(peaks1 >= start) & (peaks1 < end)] - start
        f0 = rr_features(p0, PPG_FS_HZ)
        f1 = rr_features(p1, PPG_FS_HZ)
        base = dict(sqi[i]) if i < len(sqi) else {"window_id": i}
        base.update({"record_label": "ppg_reference", "slot0_bpm_median": f0["bpm_median"], "slot1_bpm_median": f1["bpm_median"], "slot0_rr_valid_count": f0["rr_valid_count"], "slot1_rr_valid_count": f1["rr_valid_count"]})
        window_rows.append(base)
    write_csv(out_dir / "ppg_windows.csv", window_rows)

    return {
        "record_label": "ppg_reference",
        "source": str(path),
        "channel": "PPG",
        "fs_hz_configured": PPG_FS_HZ,
        **validation,
        "slot0_peak_count": int(len(peaks0)),
        "slot1_peak_count": int(len(peaks1)),
        "slot0_bpm_median": rr_features(peaks0, PPG_FS_HZ)["bpm_median"],
        "slot1_bpm_median": rr_features(peaks1, PPG_FS_HZ)["bpm_median"],
        "windows": len(window_rows),
        "sqi_good_windows": int(sum(bool(row.get("ppg_good_v01", False)) for row in window_rows)),
        "fifo_valid_fraction": float(np.mean(df["fifo_ok"].to_numpy(dtype=int) == 1)),
        "finger_contact_available": bool("finger_contact" in df and np.any(df["finger_contact"].to_numpy(dtype=int) != -1)),
    }


def make_preview(ecg_path: Path, ppg_path: Path, out_dir: Path) -> None:
    """Create a compact first-10-second preview for visual QA."""
    ecg = pd.read_csv(ecg_path).iloc[: int(10 * ECG_FS_HZ)]
    ppg = pd.read_csv(ppg_path).iloc[: int(10 * PPG_FS_HZ)]
    fig, ax = plt.subplots(3, 1, figsize=(14, 9), constrained_layout=True)
    te = (ecg["timestamp_us"].to_numpy() - int(ecg["timestamp_us"].iloc[0])) / 1_000_000.0
    tp = (ppg["timestamp_us"].to_numpy() - int(ppg["timestamp_us"].iloc[0])) / 1_000_000.0
    ax[0].plot(te, ecg["ecg_raw"], lw=0.5, label="ECG raw")
    ax[0].set_ylabel("ADC")
    ax[0].legend(loc="upper right")
    ax[1].plot(tp, ppg["slot0_raw"], lw=0.8, label="slot0 raw")
    ax[1].set_ylabel("slot0")
    ax[1].legend(loc="upper right")
    ax[2].plot(tp, ppg["slot1_raw"], lw=0.8, label="slot1 raw")
    ax[2].set_ylabel("slot1")
    ax[2].set_xlabel("relative time (s)")
    ax[2].legend(loc="upper right")
    fig.savefig(out_dir / "preview_first_10s.png", dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ecg-clean-long", required=True, type=Path)
    parser.add_argument("--ecg-noisy", required=True, type=Path)
    parser.add_argument("--ecg-clean-short", required=True, type=Path)
    parser.add_argument("--ppg", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    records = [
        process_ecg(args.ecg_clean_long, args.out_dir, "ecg_clean_long"),
        process_ecg(args.ecg_noisy, args.out_dir, "ecg_noisy"),
        process_ecg(args.ecg_clean_short, args.out_dir, "ecg_clean_short"),
        process_ppg(args.ppg, args.out_dir),
    ]
    make_preview(args.ecg_clean_long, args.ppg, args.out_dir)
    summary = {
        "status": "offline_hardware_processing_v0.1",
        "raw_unchanged": True,
        "window_seconds": 5.0,
        "step_seconds": 2.5,
        "ecg_fs_hz": ECG_FS_HZ,
        "ppg_fs_hz": PPG_FS_HZ,
        "thresholds": THRESHOLDS,
        "records": records,
        "next_use": [
            "AI: use processed signals, SQI labels and valid feature rows.",
            "Firmware: port the frozen thresholds and replay the same window decisions.",
        ],
    }
    (args.out_dir / "offline_summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(summary, ensure_ascii=False, indent=2, allow_nan=False))
    print(f"Output directory: {args.out_dir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

