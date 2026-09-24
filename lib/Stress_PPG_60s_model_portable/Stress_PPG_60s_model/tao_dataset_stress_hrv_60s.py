#!/usr/bin/env python3
"""Tạo dataset HRV 60 giây cho bài toán Baseline–Stress từ WESAD.

Đặt file này trong thư mục WESAD, cạnh:

* Các thư mục S2/S2.pkl, S3/S3.pkl, ...
* ``subject_split_map.csv`` đã tạo ở bước chia Baseline–Stress 10 giây.

Chạy::

    python tao_dataset_stress_hrv_60s.py

Đầu ra nằm trong ``Stress_HRV_60s``. Script giữ nguyên split theo subject,
cắt cửa sổ 60 giây với stride 30 giây, phát hiện đỉnh BVP, tính đặc trưng
HR/HRV và cân bằng Baseline–Stress trong từng subject.

CẢNH BÁO: pickle có thể thực thi mã khi mở. Chỉ dùng file WESAD .pkl từ
nguồn tin cậy.
"""

from __future__ import annotations

from collections import defaultdict
from pathlib import Path
import pickle
import random

import numpy as np
import pandas as pd
from scipy.signal import butter, find_peaks, sosfiltfilt
from scipy.stats import kurtosis, skew


SCRIPT_DIR = Path(__file__).resolve().parent
SPLIT_MAP_CSV = SCRIPT_DIR / "subject_split_map.csv"
OUTPUT_DIR = SCRIPT_DIR / "Stress_HRV_60s"

BVP_FS = 64
LABEL_FS = 700
WINDOW_SEC = 60
STRIDE_SEC = 30
BVP_WINDOW_SAMPLES = BVP_FS * WINDOW_SEC

BASELINE_LABEL = 1
STRESS_LABEL = 2
LABEL_NAMES = {BASELINE_LABEL: "baseline", STRESS_LABEL: "stress"}

MIN_FINITE_RATIO = 0.98
MIN_BEATS = 40
# WESAD BVP ở cổ tay có nhiều nhiễu chuyển động, đặc biệt trong pha stress.
# Ngưỡng 0.80 làm mất hoàn toàn lớp stress của 4 subject. 0.65 vẫn yêu cầu
# phần lớn khoảng PP hợp lệ nhưng giữ đủ 15 subject cho đánh giá subject-wise.
MIN_VALID_RR_RATIO = 0.65
MIN_RR_SEC = 60.0 / 180.0  # tối đa 180 bpm
MAX_RR_SEC = 60.0 / 40.0   # tối thiểu 40 bpm
RR_MEDIAN_TOLERANCE = 0.30
RANDOM_SEED = 20260821
MAX_WINDOWS_PER_CLASS_PER_SUBJECT = 15

FEATURE_COLUMNS = [
    "mean_hr_bpm", "std_hr_bpm", "min_hr_bpm", "max_hr_bpm",
    "mean_pp_ms", "median_pp_ms", "sdnn_ms", "rmssd_ms", "sdsd_ms",
    "pnn20_pct", "pnn50_pct", "cvnn", "filtered_std", "signal_range",
    "signal_skewness", "signal_kurtosis", "mean_pulse_amplitude",
    "std_pulse_amplitude", "beat_count", "valid_rr_ratio",
]


def read_split_map() -> dict[str, str]:
    if not SPLIT_MAP_CSV.exists():
        raise FileNotFoundError(
            f"Thiếu {SPLIT_MAP_CSV.name}. Hãy copy file này từ "
            "Stress_Binary_Split vào thư mục WESAD."
        )
    frame = pd.read_csv(SPLIT_MAP_CSV)
    if not {"subject", "split"}.issubset(frame.columns):
        raise ValueError("subject_split_map.csv phải có hai cột subject và split")
    frame = frame[["subject", "split"]].drop_duplicates()
    frame["subject"] = frame.subject.astype(str).str.upper()
    frame["split"] = frame.split.astype(str).str.lower()
    allowed = {"train", "validation", "test"}
    if not set(frame.split).issubset(allowed):
        raise ValueError(f"Split ngoài {sorted(allowed)}")
    if frame.subject.duplicated().any():
        raise ValueError("Một subject xuất hiện ở nhiều split")
    return dict(zip(frame.subject, frame.split))


def find_subject_files(subjects: list[str]) -> dict[str, Path]:
    """Mỗi subject lấy đúng một file, tránh các bản sao S2–S4."""
    grouped: dict[str, list[Path]] = defaultdict(list)
    for path in sorted(SCRIPT_DIR.rglob("S*.pkl")):
        grouped[path.stem.upper()].append(path)

    result = {}
    for subject in subjects:
        matches = grouped.get(subject, [])
        if not matches:
            raise FileNotFoundError(f"Không tìm thấy {subject}.pkl")
        matches.sort(
            key=lambda path: (
                path.parent.name.upper() != subject,
                len(path.parts),
                str(path),
            )
        )
        result[subject] = matches[0]
        if len(matches) > 1:
            print(f"[TRÙNG] {subject}: có {len(matches)} bản, dùng {matches[0]}")
    return result


def load_wesad(path: Path) -> tuple[np.ndarray, np.ndarray]:
    with path.open("rb") as handle:
        data = pickle.load(handle, encoding="latin1")
    try:
        bvp = np.asarray(data["signal"]["wrist"]["BVP"], dtype=float).reshape(-1)
        labels = np.asarray(data["label"]).reshape(-1).astype(int)
    except Exception as exc:
        raise ValueError(f"Cấu trúc WESAD không hợp lệ: {exc}") from exc
    expected = LABEL_FS / BVP_FS
    actual = len(labels) / max(len(bvp), 1)
    if abs(actual - expected) > 0.1:
        raise ValueError(f"Tỷ lệ label/BVP={actual:.4f}, không gần {expected:.4f}")
    return bvp, labels


def contiguous_ranges(mask: np.ndarray) -> list[tuple[int, int]]:
    padded = np.concatenate(([False], mask.astype(bool), [False])).astype(int)
    changes = np.diff(padded)
    starts = np.where(changes == 1)[0]
    ends = np.where(changes == -1)[0]
    return list(zip(starts.tolist(), ends.tolist()))


def interpolate_small_gaps(signal: np.ndarray) -> tuple[np.ndarray, float]:
    finite = np.isfinite(signal)
    ratio = float(finite.mean()) if len(signal) else 0.0
    if ratio < MIN_FINITE_RATIO:
        raise ValueError(f"finite_ratio={ratio:.3f} < {MIN_FINITE_RATIO}")
    if not finite.all():
        indices = np.arange(len(signal))
        signal = np.interp(indices, indices[finite], signal[finite])
    return signal, ratio


def filter_bvp(signal: np.ndarray) -> np.ndarray:
    sos = butter(4, [0.5, 4.0], btype="bandpass", fs=BVP_FS, output="sos")
    return sosfiltfilt(sos, signal)


def clean_rr(peaks: np.ndarray) -> tuple[np.ndarray, float]:
    raw_rr = np.diff(peaks) / BVP_FS
    if len(raw_rr) == 0:
        return np.asarray([]), 0.0
    physiological = raw_rr[(raw_rr >= MIN_RR_SEC) & (raw_rr <= MAX_RR_SEC)]
    if len(physiological) == 0:
        return np.asarray([]), 0.0
    median = float(np.median(physiological))
    lower = median * (1.0 - RR_MEDIAN_TOLERANCE)
    upper = median * (1.0 + RR_MEDIAN_TOLERANCE)
    cleaned = physiological[(physiological >= lower) & (physiological <= upper)]
    return cleaned, float(len(cleaned) / len(raw_rr))


def peak_candidate(filtered: np.ndarray, polarity: int) -> tuple[np.ndarray, np.ndarray, float]:
    oriented = filtered * polarity
    prominence = max(0.20 * float(np.std(oriented)), 1e-8)
    peaks, properties = find_peaks(
        oriented,
        distance=int(BVP_FS * MIN_RR_SEC),
        prominence=prominence,
    )
    rr, valid_ratio = clean_rr(peaks)
    return peaks, rr, valid_ratio


def choose_peaks(filtered: np.ndarray) -> tuple[np.ndarray, np.ndarray, float, int]:
    candidates = []
    for polarity in (1, -1):
        peaks, rr, ratio = peak_candidate(filtered, polarity)
        plausible_count = MIN_BEATS <= len(peaks) <= int(WINDOW_SEC * 3.1)
        score = ratio + (1.0 if plausible_count else 0.0) + min(len(rr), 100) / 1000
        candidates.append((score, peaks, rr, ratio, polarity))
    _, peaks, rr, ratio, polarity = max(candidates, key=lambda item: item[0])
    if len(peaks) < MIN_BEATS:
        raise ValueError(f"beat_count={len(peaks)} < {MIN_BEATS}")
    if ratio < MIN_VALID_RR_RATIO:
        raise ValueError(f"valid_rr_ratio={ratio:.3f} < {MIN_VALID_RR_RATIO}")
    return peaks, rr, ratio, polarity


def pulse_amplitudes(filtered: np.ndarray, peaks: np.ndarray, polarity: int) -> np.ndarray:
    oriented = filtered * polarity
    amplitudes = []
    for left_peak, peak in zip(peaks[:-1], peaks[1:]):
        if peak <= left_peak + 1:
            continue
        trough = float(np.min(oriented[left_peak:peak]))
        amplitudes.append(float(oriented[peak] - trough))
    return np.asarray(amplitudes, dtype=float)


def extract_features(raw: np.ndarray) -> tuple[dict, dict]:
    signal = np.asarray(raw, dtype=float).reshape(-1)
    if len(signal) != BVP_WINDOW_SAMPLES:
        raise ValueError(f"samples={len(signal)}, cần {BVP_WINDOW_SAMPLES}")
    signal, finite_ratio = interpolate_small_gaps(signal)
    if float(np.std(signal)) <= 1e-8 or float(np.ptp(signal)) <= 1e-8:
        raise ValueError("Tín hiệu phẳng")

    filtered = filter_bvp(signal)
    peaks, rr_sec, valid_ratio, polarity = choose_peaks(filtered)
    rr_ms = rr_sec * 1000.0
    diff_ms = np.diff(rr_ms)
    if len(diff_ms) < 2:
        raise ValueError("Không đủ sai phân PP để tính HRV")
    hr = 60.0 / rr_sec
    amplitudes = pulse_amplitudes(filtered, peaks, polarity)
    if len(amplitudes) == 0:
        raise ValueError("Không tính được biên độ xung")

    mean_pp = float(np.mean(rr_ms))
    features = {
        "mean_hr_bpm": float(np.mean(hr)),
        "std_hr_bpm": float(np.std(hr, ddof=1)),
        "min_hr_bpm": float(np.min(hr)),
        "max_hr_bpm": float(np.max(hr)),
        "mean_pp_ms": mean_pp,
        "median_pp_ms": float(np.median(rr_ms)),
        "sdnn_ms": float(np.std(rr_ms, ddof=1)),
        "rmssd_ms": float(np.sqrt(np.mean(diff_ms ** 2))),
        "sdsd_ms": float(np.std(diff_ms, ddof=1)),
        "pnn20_pct": float(100.0 * np.mean(np.abs(diff_ms) > 20.0)),
        "pnn50_pct": float(100.0 * np.mean(np.abs(diff_ms) > 50.0)),
        "cvnn": float(np.std(rr_ms, ddof=1) / mean_pp),
        "filtered_std": float(np.std(filtered)),
        "signal_range": float(np.ptp(filtered)),
        "signal_skewness": float(skew(filtered, bias=False)),
        "signal_kurtosis": float(kurtosis(filtered, fisher=True, bias=False)),
        "mean_pulse_amplitude": float(np.mean(amplitudes)),
        "std_pulse_amplitude": float(np.std(amplitudes, ddof=1)) if len(amplitudes) > 1 else 0.0,
        "beat_count": int(len(peaks)),
        "valid_rr_ratio": valid_ratio,
    }
    qc = {
        "finite_ratio": finite_ratio,
        "peak_polarity": polarity,
        "raw_peak_count": int(len(peaks)),
        "clean_rr_count": int(len(rr_sec)),
        "valid_rr_ratio": valid_ratio,
    }
    return features, qc


def candidate_windows(
    subject: str,
    split: str,
    path: Path,
    bvp: np.ndarray,
    labels: np.ndarray,
) -> tuple[list[dict], list[dict]]:
    passed = []
    report = []
    stride_samples = STRIDE_SEC * BVP_FS

    for original_label, label_name in LABEL_NAMES.items():
        ranges = contiguous_ranges(labels == original_label)
        for label_start, label_end in ranges:
            bvp_start = int(np.ceil(label_start * BVP_FS / LABEL_FS))
            bvp_end = int(np.floor(label_end * BVP_FS / LABEL_FS))
            for start in range(
                bvp_start,
                bvp_end - BVP_WINDOW_SAMPLES + 1,
                stride_samples,
            ):
                end = start + BVP_WINDOW_SAMPLES
                check_start = int(np.floor(start * LABEL_FS / BVP_FS))
                check_end = int(np.ceil(end * LABEL_FS / BVP_FS))
                label_window = labels[check_start:check_end]
                label_ratio = float(np.mean(label_window == original_label))
                window_id = f"{subject}_{label_name}_t_{start / BVP_FS:.1f}"
                base = {
                    "window_id": window_id,
                    "subject": subject,
                    "split": split,
                    "label": label_name,
                    "original_label": original_label,
                    "source_file": str(path),
                    "start_sec": float(start / BVP_FS),
                    "end_sec": float(end / BVP_FS),
                    "label_ratio": label_ratio,
                }
                if label_ratio != 1.0:
                    report.append({**base, "qc_status": "REJECT", "reason": "label_ratio < 1.0"})
                    continue
                try:
                    features, qc = extract_features(bvp[start:end])
                    row = {**base, **features}
                    passed.append(row)
                    report.append({**base, **qc, "qc_status": "PASS", "reason": ""})
                except Exception as exc:
                    report.append({**base, "qc_status": "REJECT", "reason": str(exc)})
    return passed, report


def balance_per_subject(features: pd.DataFrame) -> pd.DataFrame:
    rng = random.Random(RANDOM_SEED)
    selected_parts = []
    for subject, subject_frame in features.groupby("subject"):
        groups = {
            label: group.index.tolist()
            for label, group in subject_frame.groupby("label")
        }
        if not {"baseline", "stress"}.issubset(groups):
            print(f"[LOẠI] {subject}: thiếu một trong hai lớp sau QC")
            continue
        # Cân bằng hai lớp trong từng subject và không để subject có nhiều
        # cửa sổ lấn át các subject còn lại.
        target = min(
            len(groups["baseline"]),
            len(groups["stress"]),
            MAX_WINDOWS_PER_CLASS_PER_SUBJECT,
        )
        for label in ("baseline", "stress"):
            indices = groups[label]
            rng.shuffle(indices)
            selected_parts.append(features.loc[indices[:target]])
    if not selected_parts:
        raise RuntimeError("Không còn dữ liệu sau khi cân bằng theo subject")
    selected = pd.concat(selected_parts, ignore_index=True)
    return selected.sort_values(["split", "subject", "label", "start_sec"]).reset_index(drop=True)


def verify_no_leakage(frame: pd.DataFrame) -> None:
    leakage = frame.groupby("subject").split.nunique()
    leakage = leakage[leakage > 1]
    if len(leakage):
        raise RuntimeError(f"SUBJECT LEAKAGE: {leakage.index.tolist()}")
    if frame[FEATURE_COLUMNS].isna().any().any():
        raise RuntimeError("Bảng đặc trưng còn NaN")
    if not np.isfinite(frame[FEATURE_COLUMNS].to_numpy(dtype=float)).all():
        raise RuntimeError("Bảng đặc trưng còn Inf")


def write_outputs(features: pd.DataFrame, qc: pd.DataFrame) -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    features.to_csv(OUTPUT_DIR / "stress_hrv_features_60s_all.csv", index=False)
    qc.to_csv(OUTPUT_DIR / "stress_hrv_qc_report.csv", index=False)
    for split in ("train", "validation", "test"):
        features[features.split == split].to_csv(
            OUTPUT_DIR / f"stress_hrv_features_60s_{split}.csv", index=False
        )
    summary = (
        features.groupby(["split", "label"])
        .agg(windows=("window_id", "size"), subjects=("subject", "nunique"))
        .reset_index()
    )
    summary.to_csv(OUTPUT_DIR / "stress_hrv_summary.csv", index=False)


def main() -> None:
    split_map = read_split_map()
    subject_files = find_subject_files(sorted(split_map))
    all_features = []
    all_reports = []

    print(f"Tìm thấy {len(subject_files)} subject theo split map")
    for subject, path in subject_files.items():
        print(f"[XỬ LÝ] {subject} ({split_map[subject]}): {path}")
        bvp, labels = load_wesad(path)
        features, report = candidate_windows(
            subject, split_map[subject], path, bvp, labels
        )
        all_features.extend(features)
        all_reports.extend(report)

    feature_frame = pd.DataFrame(all_features)
    qc_frame = pd.DataFrame(all_reports)
    if feature_frame.empty:
        raise RuntimeError("Không có cửa sổ nào đạt QC")

    selected = balance_per_subject(feature_frame)
    verify_no_leakage(selected)
    write_outputs(selected, qc_frame)

    print("\n===== KẾT QUẢ DATASET STRESS HRV 60 GIÂY =====")
    print(
        selected.groupby(["split", "label"]).size().unstack(fill_value=0).to_string()
    )
    print("\nSố subject theo tập:")
    print(selected.groupby("split").subject.nunique().to_string())
    print(f"\nQC PASS: {(qc_frame.qc_status == 'PASS').sum()}")
    print(f"QC REJECT: {(qc_frame.qc_status == 'REJECT').sum()}")
    print("KIỂM TRA SUBJECT LEAKAGE: PASS")
    print(f"Kết quả: {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
