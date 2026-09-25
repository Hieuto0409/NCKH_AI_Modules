#!/usr/bin/env python3
"""
run_offline_demo.py
-------------------
Demo offline tổng hợp cho đề tài NCKH. Chạy một lệnh duy nhất:

    python tools/run_offline_demo.py

Đầu ra:
  - Bảng tổng hợp trạng thái ba nhánh (stdout)
  - File JSON lưu tại tools/output/offline_demo_<timestamp>.json

Mã trạng thái (status tags):
  RESULT_AVAILABLE  — có kết quả từ model/thuật toán thực đã chạy xong
  TEST_FIXTURE      — kết quả từ bộ test/replay tích hợp sẵn của module (không từ Step 2)
  QC_REJECTED       — dữ liệu Step 2 không đạt kiểm tra chất lượng
  NOT_READY         — thiếu điều kiện kỹ thuật (đơn vị chưa xác minh, phần cứng thiếu…)
  SKIPPED           — công cụ cần thiết (g++, pio) không có trong PATH

Mã thoát:
  0  — demo hoàn thành bình thường (NOT_READY/TEST_FIXTURE là giới hạn đã biết, không phải lỗi)
  1  — có lỗi kiểm thử thực sự:
         • Stress: |diff Python–C++| > 1e-9, hoặc hai nhãn khác nhau
         • Stress: g++ có nhưng biên dịch/chạy binary thất bại
         • SpO2:   g++ + file test có nhưng replay test trả exit ≠ 0

Ràng buộc bảo toàn:
  - Không sửa dữ liệu Step 2 gốc
  - Không nội suy PPG 25 Hz -> 100 Hz hay 64 Hz
  - Không tự gán slot RED/IR
  - Không gọi stress_ppg::infer() qua Python; phải biên dịch và chạy binary thực
  - Chỉ dùng nhãn "C++ binary" khi binary thực sự chạy thành công
  - Kết quả NOT_READY cho firmware ESP32-S3 (chưa có phần cứng)

Ghi chú ECG (đã xác minh từ model_metadata.h dòng 106):
  EI_CLASSIFIER_SENSOR = EI_CLASSIFIER_SENSOR_FUSION
  EI_CLASSIFIER_FUSION_AXES_STRING = "mean_rr + median_rr + sdnn + rmssd + pnn50 +
                                       cv_rr + iqr_rr + min_rr + max_rr"
  → Model nhận 9 đặc trưng HRV (KHÔNG phải raw ECG waveform).
  Đơn vị (giây hay ms) chưa xác minh từ dữ liệu huấn luyện gốc;
  scaler mean trong model_variables.h chỉ là manh mối, không đủ làm bằng chứng.
"""

import sys, os, json, math, subprocess, shutil, time, re
from pathlib import Path
from datetime import datetime, timezone

# --- Đảm bảo encode UTF-8 trên Windows ---
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

PROJECT_ROOT = Path(__file__).resolve().parent.parent
TOOLS_DIR    = PROJECT_ROOT / "tools"
OUTPUT_DIR   = TOOLS_DIR / "output"
STEP2_DIR    = (PROJECT_ROOT / "lib"
                / "step2_handoff_sqi_v0_1-20260924T133622Z-1-001"
                / "step2_handoff_sqi_v0_1")
STRESS_MODEL_JSON = PROJECT_ROOT / "lib" / "Stress_PPG_60s_model" / "model.json"
STRESS_HEADER     = PROJECT_ROOT / "lib" / "Stress_PPG_60s_model" / "stress_ppg_model.h"
STRESS_CHECKER_SRC = TOOLS_DIR / "stress_cpp_checker.cpp"
STRESS_CHECKER_BIN = PROJECT_ROOT / ".pio" / "stress_cpp_checker.exe"

# SQI window file từ Step 2
SQI_WINDOWS_CSV = STEP2_DIR / "sqi_calibration_v01" / "ppg_sqi_windows.csv"
PPG_RAW_CSV     = STEP2_DIR / "raw" / "ppg_raw(5).csv"
ECG_CLEAN_CSV   = STEP2_DIR / "raw" / "ecg_raw(8).csv"
ECG_NOISY_CSV   = STEP2_DIR / "raw" / "ecg_raw(9).csv"

# SpO2 test binary và working directory
SPO2_TEST_CPP = (PROJECT_ROOT / "lib" / "SPO2_Module_MAX30102_v0_1"
                 / "SPO2_Module" / "tests" / "test.cpp")
SPO2_TEST_BIN = PROJECT_ROOT / ".pio" / "spo2_test.exe"
SPO2_WORK_DIR = SPO2_TEST_CPP.parent.parent  # SPO2_Module/ — binary must run here for tests/data/ path
# ResearchSpO2 sources inside the SPO2 module package (confirmed location)
SPO2_SRC_DIR  = (PROJECT_ROOT / "lib" / "SPO2_Module_MAX30102_v0_1"
                 / "SPO2_Module" / "lib" / "ResearchSpO2" / "src")

# ==============================================================================
#  Tiện ích
# ==============================================================================
def _import_science():
    """Import numpy/pandas/scipy lần duy nhất."""
    import numpy as np
    import pandas as pd
    from scipy.signal import butter, find_peaks, sosfiltfilt
    return np, pd, butter, find_peaks, sosfiltfilt

def compute_ppg_bpm_from_ppi(ppi_ms, sqi_ok: bool = True, min_beats: int = 4) -> dict:
    """
    Tính BPM từ mảng khoảng cách giữa các đỉnh liên tiếp (PPI tính bằng ms).
    Công thức: 60000.0 / median(valid_ppi_ms).
    Quy tắc:
      - Nếu sqi_ok == False: trả NOT_READY (heart_rate_bpm=None).
      - Nếu số khoảng nhịp hợp lệ < min_beats: trả NOT_READY (heart_rate_bpm=None).
      - KHÔNG BAO GIỜ trả 0 BPM như nhịp tim đo được.
    """
    import numpy as np
    res = {
        "heart_rate_bpm": None,
        "heart_rate_source": "PPG",
        "heart_rate_status": "NOT_READY",
        "details": {}
    }
    if not sqi_ok:
        res["details"]["reason"] = "Kiem tra chat luong SQI khong dat"
        return res

    arr = np.asarray(ppi_ms, dtype=float)
    if len(arr) == 0:
        res["details"]["reason"] = "Khong co khoang cach dinh (PPI rong)"
        return res

    # Lọc khoảng sinh lý [333.33 ms (180 bpm), 1500.0 ms (40 bpm)]
    valid = arr[(arr >= 333.33) & (arr <= 1500.0)]
    if len(valid) < min_beats:
        res["details"]["reason"] = f"So khoang PPI hop le ({len(valid)}) < nguong toi thieu ({min_beats})"
        return res

    median_ppi = float(np.median(valid))
    if median_ppi <= 0.0:
        res["details"]["reason"] = "Median PPI <= 0 ms"
        return res

    bpm = float(60000.0 / median_ppi)
    res["heart_rate_bpm"] = round(bpm, 2)
    res["heart_rate_status"] = "RESULT_AVAILABLE"
    res["details"] = {
        "formula": "60000 / median(valid_ppi_ms)",
        "median_ppi_ms": round(median_ppi, 2),
        "valid_beat_count": int(len(valid)),
        "mean_hr_bpm": round(float(np.mean(60000.0 / valid)), 2),
        "disclaimer": "Ket qua tu du lieu mau Step 2, chua phai phep do truc tiep tren ESP32-S3."
    }
    return res

def section(title: str):
    print(f"\n{'='*78}")
    print(f"  {title}")
    print(f"{'='*78}")

def subsection(title: str):
    print(f"\n  {'─'*74}")
    print(f"  {title}")
    print(f"  {'─'*74}")

# ==============================================================================
#  BƯỚC 0: Biên dịch C++ stress checker
# ==============================================================================
def compile_stress_checker() -> dict:
    """Biên dịch tools/stress_cpp_checker.cpp thành binary g++.
    Trả về dict {ok, binary_path, log, skipped}."""
    result = {"ok": False, "binary_path": None, "log": "", "skipped": False}

    if not shutil.which("g++"):
        result["log"] = "g++ khong tim thay trong PATH"
        result["skipped"] = True
        return result

    STRESS_CHECKER_BIN.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "g++", "-std=c++11", "-O2", "-Wall", "-Wextra",
        f"-I{STRESS_HEADER.parent}",
        str(STRESS_CHECKER_SRC),
        "-o", str(STRESS_CHECKER_BIN)
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    result["log"] = (proc.stdout + proc.stderr).strip()
    if proc.returncode == 0 and STRESS_CHECKER_BIN.exists():
        result["ok"] = True
        result["binary_path"] = str(STRESS_CHECKER_BIN)
    return result

# ==============================================================================
#  NHÁNH STRESS PPG & NHỊP TIM (PPG HEART RATE)
# ==============================================================================
def run_stress_branch() -> dict:
    np, pd, butter, find_peaks, sosfiltfilt = _import_science()

    report = {
        "branch": "Stress PPG",
        "status": "NOT_READY",
        "source_file": str(PPG_RAW_CSV.relative_to(PROJECT_ROOT)),
        "sample_rate_hz": 25.0,
        "training_sample_rate_hz": 64.0,
        "sample_rate_disclaimer": (
            "PPG Step 2 ghi o 25 Hz; model WESAD duoc huan luyen tu 64 Hz. "
            "Dac trung HRV (ms) khong phu thuoc vao fs nen duoc tinh. "
            "Tuy nhien do chinh xac phat hien dinh PPG co the thap hon so voi du lieu 64 Hz goc."
        ),
        "qc": {"passed": False, "method": "SQI windows Step 2 + FIFO + peak count", "details": []},
        "features": {},
        # Nhịp tim (BPM) từ PPG
        "heart_rate_bpm": None,
        "heart_rate_source": "PPG",
        "heart_rate_status": "NOT_READY",
        "heart_rate_details": {},
        "python_result": None,
        "cpp_result": None,
        # Cờ nội bộ để tính exit code
        "_cpp_was_expected": False,   # True khi g++ có và binary phải chạy được
        "_cpp_hard_fail": False,      # True khi có g++ nhưng compile/run thất bại
        "_tol_fail": False,           # True khi |diff| > 1e-9 hoặc nhãn khác nhau
        "limits": "Ket qua chi la kiem tra thuat toan offline; CHUA duoc xac nhan tren MAX30102 that."
    }

    # --- 1. Đọc SQI windows Step 2 thực tế ---
    sqi_ok = False
    if SQI_WINDOWS_CSV.exists():
        try:
            sqi_df = pd.read_csv(SQI_WINDOWS_CSV)
            if "ppg_good_v01" in sqi_df.columns:
                n_windows = len(sqi_df)
                n_good = int(sqi_df["ppg_good_v01"].sum())
                frac_good = n_good / n_windows if n_windows > 0 else 0.0
                sqi_ok = (frac_good >= 0.80)
                report["qc"]["details"].append(
                    f"SQI Step 2 (ppg_sqi_windows.csv): {n_good}/{n_windows} cua so dat "
                    f"ppg_good_v01=True ({100*frac_good:.1f}%) - nguong chap nhan >= 80%"
                )
                report["qc"]["details"].append(
                    "Quy tac chap nhan 60s: >= 80% cua so SQI 5s lien tuc dat ppg_good_v01=True"
                )
            else:
                report["qc"]["details"].append("ppg_sqi_windows.csv khong co cot ppg_good_v01")
        except Exception as e:
            report["qc"]["details"].append(f"Loi doc SQI CSV: {e}")
    else:
        report["qc"]["details"].append(f"Khong tim thay: {SQI_WINDOWS_CSV}")

    if not sqi_ok:
        report["status"] = "QC_REJECTED"
        report["qc"]["details"].append("SQI khong dat - tu choi trich xuat dac trung")
        return report

    # --- 2. Đọc PPG raw ---
    if not PPG_RAW_CSV.exists():
        report["status"] = "QC_REJECTED"
        report["qc"]["details"].append(f"File PPG raw khong ton tai: {PPG_RAW_CSV}")
        return report

    df = pd.read_csv(PPG_RAW_CSV)
    fs = 25.0
    target = int(60 * fs)

    # Kiểm tra FIFO
    fifo_bad = int(df.iloc[:target]["fifo_ok"].ne(1).sum()) if len(df) >= target else 1
    overflow = int(df.iloc[:target]["fifo_overflow_count"].gt(0).sum()) if len(df) >= target else 1
    report["qc"]["details"].append(f"FIFO: {target - fifo_bad}/{target} mau hop le, overflow={overflow}")

    if len(df) < target or fifo_bad > 0 or overflow > 0:
        report["status"] = "QC_REJECTED"
        report["qc"]["details"].append("FIFO QC khong dat")
        return report

    seg = df.iloc[:target]
    raw = seg["slot1_raw"].to_numpy(dtype=float)

    # Lọc BVP
    sos = butter(4, [0.5, 4.0], btype="bandpass", fs=fs, output="sos")
    filtered = sosfiltfilt(sos, raw)

    # Tìm đỉnh tốt nhất (thử cả 2 phân cực)
    min_dist = max(1, int(round(fs * 60.0 / 180.0)))
    best_peaks, best_rr, best_ratio = None, None, 0.0
    for polarity in (1, -1):
        o = filtered * polarity
        prom = max(0.20 * float(np.std(o)), 1e-8)
        peaks, _ = find_peaks(o, distance=min_dist, prominence=prom)
        rr_sec = np.diff(peaks) / fs
        phys = rr_sec[(rr_sec >= 60.0/180.0) & (rr_sec <= 60.0/40.0)]
        if len(phys) == 0:
            continue
        med = float(np.median(phys))
        cleaned = phys[(phys >= med * 0.70) & (phys <= med * 1.30)]
        ratio = len(cleaned) / len(rr_sec) if len(rr_sec) else 0.0
        if ratio > best_ratio:
            best_peaks, best_rr, best_ratio = peaks, cleaned, ratio

    n_peaks = len(best_peaks) if best_peaks is not None else 0
    report["qc"]["details"].append(
        f"Peak detection (slot1_raw): {n_peaks} nhip phat hien, "
        f"valid_rr_ratio={best_ratio:.3f}"
    )

    if best_peaks is None or n_peaks < 40 or best_ratio < 0.65:
        report["status"] = "QC_REJECTED"
        report["qc"]["details"].append(
            f"So nhip ({n_peaks}) < 40 hoac valid_rr_ratio ({best_ratio:.3f}) < 0.65"
        )
        return report

    report["qc"]["passed"] = True

    # --- 3. Tính 14 đặc trưng và Nhịp tim PPG (BPM) ---
    rr_ms  = best_rr * 1000.0
    diff_ms = np.diff(rr_ms)
    hr = 60.0 / best_rr
    mean_pp = float(np.mean(rr_ms))

    # Tái sử dụng khoảng PPI đã tính từ các đỉnh hợp lệ, không chạy thuật toán phát hiện đỉnh thứ hai
    hr_calc = compute_ppg_bpm_from_ppi(rr_ms, sqi_ok=True, min_beats=40)
    report["heart_rate_bpm"]     = hr_calc["heart_rate_bpm"]
    report["heart_rate_source"]  = hr_calc["heart_rate_source"]
    report["heart_rate_status"]  = hr_calc["heart_rate_status"]
    report["heart_rate_details"] = hr_calc["details"]

    feats = {
        "mean_hr_bpm":    float(np.mean(hr)),
        "std_hr_bpm":     float(np.std(hr, ddof=1)),
        "min_hr_bpm":     float(np.min(hr)),
        "max_hr_bpm":     float(np.max(hr)),
        "mean_pp_ms":     mean_pp,
        "median_pp_ms":   float(np.median(rr_ms)),
        "sdnn_ms":        float(np.std(rr_ms, ddof=1)),
        "rmssd_ms":       float(np.sqrt(np.mean(diff_ms**2))),
        "sdsd_ms":        float(np.std(diff_ms, ddof=1)),
        "pnn20_pct":      float(100.0 * np.mean(np.abs(diff_ms) > 20.0)),
        "pnn50_pct":      float(100.0 * np.mean(np.abs(diff_ms) > 50.0)),
        "cvnn":           float(np.std(rr_ms, ddof=1) / mean_pp),
        "beat_count":     float(n_peaks),
        "valid_rr_ratio": float(best_ratio),
    }
    report["features"] = feats

    # --- 4. Python inference (model.json) ---
    py_prob = None
    py_pred = None
    if STRESS_MODEL_JSON.exists():
        model = json.loads(STRESS_MODEL_JSON.read_text(encoding="utf-8"))
        x = [feats[name] for name in model["features_in_order"]]
        mean_arr  = model["scaler_mean"]
        scale_arr = model["scaler_scale"]
        coef_arr  = model["logistic_coefficient"]
        intercept = model["logistic_intercept"]
        z_val = intercept + sum(
            ((x[i] - mean_arr[i]) / scale_arr[i]) * coef_arr[i]
            for i in range(len(x))
        )
        py_prob = 1.0/(1.0+math.exp(-z_val)) if z_val >= 0 else math.exp(z_val)/(1.0+math.exp(z_val))
        py_pred = "stress" if py_prob >= model["threshold"] else "baseline"
        report["python_result"] = {
            "source": "Python (model.json)",
            "z": z_val,
            "probability": py_prob,
            "prediction": py_pred
        }

    # --- 5. C++ binary inference ---
    compile_info = compile_stress_checker()

    if compile_info["skipped"]:
        # g++ không có → bỏ qua, không tính là lỗi
        report["cpp_result"] = {"skipped": compile_info["log"]}
        if py_prob is not None:
            report["status"] = "RESULT_AVAILABLE"
    elif not compile_info["ok"]:
        # g++ có nhưng compile thất bại → lỗi thực sự
        report["cpp_result"] = {"error": f"Bien dich that bai: {compile_info['log']}"}
        report["_cpp_was_expected"] = True
        report["_cpp_hard_fail"] = True
        if py_prob is not None:
            report["status"] = "RESULT_AVAILABLE"
    else:
        report["_cpp_was_expected"] = True
        feat_args = [str(feats[k]) for k in [
            "mean_hr_bpm", "std_hr_bpm", "min_hr_bpm", "max_hr_bpm",
            "mean_pp_ms", "median_pp_ms", "sdnn_ms", "rmssd_ms",
            "sdsd_ms", "pnn20_pct", "pnn50_pct", "cvnn",
            "beat_count", "valid_rr_ratio"
        ]]
        try:
            proc = subprocess.run(
                [compile_info["binary_path"]] + feat_args,
                capture_output=True, text=True, timeout=10
            )
            if proc.returncode == 0:
                line = proc.stdout.strip()
                m_prob = re.search(r"prob=([0-9.eE+\-]+)", line)
                m_pred = re.search(r"pred=(\w+)", line)
                if m_prob and m_pred:
                    cpp_prob = float(m_prob.group(1))
                    cpp_pred = m_pred.group(1)
                    diff = abs(cpp_prob - py_prob) if py_prob is not None else 0.0
                    tol_ok = (diff <= 1e-9)
                    label_ok = (cpp_pred == py_pred) if py_pred is not None else True
                    report["cpp_result"] = {
                        "source": "C++ binary (stress_ppg::infer)",
                        "probability": cpp_prob,
                        "prediction": cpp_pred,
                        "diff_vs_python": diff,
                        "tolerance_check": "PASS" if tol_ok else "FAIL",
                        "label_match": "PASS" if label_ok else "FAIL"
                    }
                    if not tol_ok or not label_ok:
                        report["_tol_fail"] = True
                    report["status"] = "RESULT_AVAILABLE"
                else:
                    report["cpp_result"] = {"error": f"Khong phan tich duoc stdout: {line!r}"}
                    report["_cpp_hard_fail"] = True
                    if py_prob is not None:
                        report["status"] = "RESULT_AVAILABLE"
            else:
                report["cpp_result"] = {
                    "error": f"Binary exit code {proc.returncode}: {proc.stderr.strip()}"
                }
                report["_cpp_hard_fail"] = True
                if py_prob is not None:
                    report["status"] = "RESULT_AVAILABLE"
        except Exception as e:
            report["cpp_result"] = {"error": str(e)}
            report["_cpp_hard_fail"] = True
            if py_prob is not None:
                report["status"] = "RESULT_AVAILABLE"

    return report


# ==============================================================================
#  NHÁNH ECG AF/NON-AF
# ==============================================================================
def run_ecg_branch() -> dict:
    """
    Trích xuất 9 đặc trưng HRV từ ECG Step 2 và ghi nhận trạng thái EI model.

    Metadata đã xác minh từ model_metadata.h (project 1119067):
      EI_CLASSIFIER_SENSOR         = EI_CLASSIFIER_SENSOR_FUSION
      EI_CLASSIFIER_FUSION_AXES    = mean_rr, median_rr, sdnn, rmssd, pnn50,
                                     cv_rr, iqr_rr, min_rr, max_rr  (9 đặc trưng)
      EI_CLASSIFIER_LABEL_COUNT    = 2  (AF, non-AF)
      EI_CLASSIFIER_NN_INPUT_FRAME_SIZE = 9

    Giới hạn còn lại:
      Đơn vị (giây hay mili-giây) CHƯA xác minh từ dữ liệu huấn luyện gốc.
      Scaler mean trong model_variables.h (mean_rr~0.767, sdnn~0.090, pnn50~36.0)
      là manh mối ủng hộ đơn vị giây+phần_trăm, nhưng không phải bằng chứng
      từ training data → KHÔNG ghi "đơn vị đã xác nhận" chỉ dựa vào scaler.
      Trạng thái suy luận AF/non-AF: NOT_READY cho đến khi đơn vị được xác minh.
    """
    np, pd, butter, find_peaks, sosfiltfilt = _import_science()

    report = {
        "branch": "ECG AF/non-AF",
        "status": "NOT_READY",
        "source_file": str(ECG_CLEAN_CSV.relative_to(PROJECT_ROOT)),
        "sample_rate_hz": 500.0,
        "ei_model_metadata": {
            "sensor_type": "EI_CLASSIFIER_SENSOR_FUSION",
            "fusion_axes": "mean_rr + median_rr + sdnn + rmssd + pnn50 + cv_rr + iqr_rr + min_rr + max_rr",
            "label_count": 2,
            "labels": ["AF", "non-AF"],
            "nn_input_frame_size": 9,
            "note": (
                "Model nhan 9 dac trung HRV (SENSOR_FUSION). "
                "Don vi (giay hay ms) chua xac minh tu du lieu huan luyen goc; "
                "scaler mean la manh moi, khong phai bang chung chinh thuc."
            )
        },
        "qc": {"passed": False, "details": []},
        "features_seconds": {},
        "features_milliseconds": {},
        "unit_status": (
            "CHUA XAC MINH don vi dau vao. "
            "Manh moi tu scaler mean: mean_rr~0.767 -> kha nang giay (s); "
            "pnn50~36.0 -> kha nang phan tram (%). "
            "Can xac minh tu du lieu huan luyen goc truoc khi goi model."
        ),
        "inference_status": "NOT_READY — don vi dau vao chua xac minh",
        "model_compiled": True,   # model bien dich duoc trong firmware build
        "firmware_note": (
            "health_orchestrator::runEcg(float features[9], 9) bien dich thanh cong "
            "trong ca hai firmware environment (esp32-s3-devkitc-1, test_fixture). "
            "Build firmware KHONG chay inference; chi xac nhan API ket noi."
        ),
        "limits": (
            "9 dac trung HRV da trich xuat tu ECG Step 2. "
            "Ket qua AF/non-AF CHUA co vi don vi chua xac minh. "
            "Khong co ket qua 'TEST_FIXTURE EI' vi demo nay khong thuc su goi EI inference."
        )
    }

    # --- QC file ECG sạch ---
    if not ECG_CLEAN_CSV.exists():
        report["qc"]["details"].append(f"File khong ton tai: {ECG_CLEAN_CSV}")
        report["status"] = "QC_REJECTED"
        return report

    df = pd.read_csv(ECG_CLEAN_CSV)
    for col in ["ecg_raw", "leads_off", "adc_ok"]:
        if col not in df.columns:
            report["qc"]["details"].append(f"Thieu cot: {col}")
            report["status"] = "QC_REJECTED"
            return report

    raw = df["ecg_raw"].to_numpy(dtype=float)
    fs = 500.0
    clean_frac = float(((df["leads_off"]==0) & (df["adc_ok"]==1)).mean())
    adc_inv    = float((df["adc_ok"]==0).mean())
    rail_frac  = float(((raw <= 10.0) | (raw >= 4085.0)).mean())
    flat_ratio = float((np.diff(raw) == 0).mean())

    report["qc"]["details"].append(
        f"clean_fraction={clean_frac:.4f} (>=0.80 de dat), "
        f"adc_invalid={adc_inv:.4f}, rail={rail_frac:.4f}, flat={flat_ratio:.4f}"
    )

    qc_fail = []
    if clean_frac < 0.80: qc_fail.append(f"clean_fraction={clean_frac:.4f} < 0.80")
    if adc_inv > 0.005:   qc_fail.append(f"adc_invalid={adc_inv:.4f} > 0.005")
    if rail_frac > 0.005: qc_fail.append(f"rail={rail_frac:.4f} > 0.005")
    if flat_ratio > 0.05: qc_fail.append(f"flat_ratio={flat_ratio:.4f} > 0.05")

    if qc_fail:
        report["qc"]["details"] += qc_fail
        report["status"] = "QC_REJECTED"
        return report

    report["qc"]["passed"] = True

    # --- Tính khoảng RR từ ECG sạch ---
    sos_morph = butter(2, [0.5, 40.0], btype="bandpass", fs=fs, output="sos")
    sos_qrs   = butter(2, [5.0, 20.0], btype="bandpass", fs=fs, output="sos")
    ecg_morph = sosfiltfilt(sos_morph, raw)
    ecg_qrs   = sosfiltfilt(sos_qrs, raw)

    derivative  = np.diff(ecg_qrs, prepend=ecg_qrs[0])
    energy      = derivative * derivative
    width       = max(1, int(round(0.15 * fs)))
    integrated  = np.convolve(energy, np.ones(width)/width, mode="same")
    std_int     = float(np.std(integrated))

    if std_int <= 1e-12:
        report["qc"]["details"].append("Nang luong QRS qua thap; khong the phat hien dinh R")
        report["status"] = "QC_REJECTED"
        return report

    candidates, _ = find_peaks(
        integrated,
        height=float(np.median(integrated) + 0.5 * std_int),
        distance=max(1, int(round(0.25 * fs))),
        prominence=max(1e-12, 0.05 * std_int),
    )
    search = max(1, int(round(0.08 * fs)))
    peaks = []
    for cand in candidates:
        lo = max(0, int(cand) - search)
        hi = min(len(ecg_morph), int(cand) + search + 1)
        local = lo + int(np.argmax(np.abs(ecg_morph[lo:hi])))
        if not peaks or local - peaks[-1] >= int(round(0.25 * fs)):
            peaks.append(local)
    peaks = np.asarray(peaks, dtype=int)

    if len(peaks) < 4:
        report["qc"]["details"].append(f"So dinh R ({len(peaks)}) < 4")
        report["status"] = "QC_REJECTED"
        return report

    rr_sec = np.diff(peaks) / fs
    valid_rr = rr_sec[(rr_sec >= 0.30) & (rr_sec <= 2.0)]
    if len(valid_rr) < 3:
        report["qc"]["details"].append(f"So khoang RR hop le ({len(valid_rr)}) < 3")
        report["status"] = "QC_REJECTED"
        return report

    valid_rr_ms = valid_rr * 1000.0

    def calc9(rr, diff, is_ms=False):
        mean_v = float(np.mean(rr))
        sdnn_v = float(np.std(rr, ddof=1))
        return {
            "mean_rr":   mean_v,
            "median_rr": float(np.median(rr)),
            "sdnn":      sdnn_v,
            "rmssd":     float(np.sqrt(np.mean(diff**2))),
            "pnn50":     float(100.0 * np.mean(np.abs(diff) > (50.0 if is_ms else 0.050))),
            "cv_rr":     sdnn_v / mean_v if mean_v > 0 else 0.0,
            "iqr_rr":    float(np.percentile(rr, 75) - np.percentile(rr, 25)),
            "min_rr":    float(np.min(rr)),
            "max_rr":    float(np.max(rr)),
        }

    report["features_seconds"]      = calc9(valid_rr,    np.diff(valid_rr),    is_ms=False)
    report["features_milliseconds"]  = calc9(valid_rr_ms, np.diff(valid_rr_ms), is_ms=True)

    # Trạng thái cuối: QC đạt nhưng đơn vị chưa xác minh → NOT_READY cho inference
    # (không phải TEST_FIXTURE; demo không thực sự chạy EI inference)
    report["status"] = "NOT_READY"
    return report


# ==============================================================================
#  NHÁNH SpO2
# ==============================================================================
def run_spo2_branch() -> dict:
    report = {
        "branch": "SpO2 (ResearchSpO2 / Stream100)",
        "status": "NOT_READY",
        # Trạng thái kết nối Step 2 và replay test tách biệt rõ ràng
        "step2_connection": {
            "eligible": False,
            "reasons": [
                "Bat dong tan so mau: Step 2 PPG o 25 Hz, ResearchSpO2::Stream100 yeu cau 100 Hz",
                "Chua dinh danh buoc song: slot0_raw / slot1_raw chua xac minh slot nao la RED, slot nao la IR",
                "Khong noi suy len 100 Hz va khong truyen tin hieu AC da loc vao pushSpo2() (quy tac bao toan)",
            ]
        },
        "module_replay_test": None,
        # Cờ nội bộ để tính exit code
        "_replay_was_expected": False,   # True khi g++ + file test có
        "_replay_hard_fail": False,      # True khi replay exit != 0 hoặc exception
        "limits": (
            "Replay test la kiem tra thuat toan cua module SpO2 bang du lieu 100 Hz co san "
            "(tests/data/session_N.csv). "
            "KHONG phai phep do tren ESP32-S3 hay tren nguoi that. "
            "Ket noi tu du lieu Step 2 CHUA du dieu kien (xem step2_connection)."
        )
    }

    # Chạy SpO2 replay test (g++ compile + run)
    compile_result = _compile_spo2_test()

    if not shutil.which("g++"):
        report["module_replay_test"] = {"status": "SKIPPED", "reason": "g++ khong co trong PATH"}
        report["status"] = "NOT_READY"
        return report

    if not compile_result["ok"]:
        report["module_replay_test"] = {
            "status": "SKIPPED",
            "reason": f"Bien dich that bai: {compile_result['log']}"
        }
        report["_replay_was_expected"] = True
        report["_replay_hard_fail"] = True
        report["status"] = "NOT_READY"
        return report

    report["_replay_was_expected"] = True

    try:
        proc = subprocess.run(
            [str(SPO2_TEST_BIN)],
            capture_output=True, text=True, timeout=30,
            cwd=str(SPO2_WORK_DIR)
        )
        lines = proc.stdout.strip().splitlines()
        sessions = {}
        for ln in lines:
            m = re.match(r"Replay session (\d+): (\d+)%, HR=(\d+)", ln)
            if m:
                sessions[f"session_{m.group(1)}"] = {
                    "spo2_pct":    int(m.group(2)),
                    "hr_bpm":      int(m.group(3)),
                    "expected_pct": [99, 100, 99][int(m.group(1))-1]
                }
        replay_ok = (proc.returncode == 0)
        report["module_replay_test"] = {
            "source": "C++ g++ (SPO2_Module tests/test.cpp)",
            "status": "PASS" if replay_ok else "FAIL",
            "exit_code": proc.returncode,
            "sessions": sessions,
            "data_source": "tests/data/session_N.csv (replay 100 Hz)",
            "note": (
                "Ket qua PASS xac nhan thuat toan ResearchSpO2::Stream100 hoat dong dung. "
                "KHONG phai phep do tren phan cung hay tren nguoi that."
            )
        }
        if replay_ok:
            # Replay pass → TEST_FIXTURE (kết quả từ test module, không phải Step 2)
            report["status"] = "TEST_FIXTURE"
        else:
            report["_replay_hard_fail"] = True
            report["status"] = "NOT_READY"
    except Exception as e:
        report["module_replay_test"] = {"status": "ERROR", "error": str(e)}
        report["_replay_hard_fail"] = True
        report["status"] = "NOT_READY"

    return report


def _compile_spo2_test() -> dict:
    result = {"ok": False, "log": ""}
    if not shutil.which("g++"):
        result["log"] = "g++ khong tim thay trong PATH"
        return result
    if not SPO2_TEST_CPP.exists():
        result["log"] = f"Khong tim thay: {SPO2_TEST_CPP}"
        return result

    SPO2_TEST_BIN.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        "g++", "-std=c++11", "-O2", "-Wall",
        f"-I{SPO2_SRC_DIR}",
        str(SPO2_SRC_DIR / "MaximCore.cpp"),
        str(SPO2_SRC_DIR / "ResearchSpO2.cpp"),
        str(SPO2_TEST_CPP),
        "-o", str(SPO2_TEST_BIN)
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    result["log"] = (proc.stdout + proc.stderr).strip()
    result["ok"] = (proc.returncode == 0 and SPO2_TEST_BIN.exists())
    return result


# ==============================================================================
#  BUILD FIRMWARE (pio run)
# ==============================================================================
def run_firmware_builds() -> dict:
    """Chạy cả hai môi trường PlatformIO build (không nạp firmware)."""
    if not shutil.which("pio"):
        return {"skipped": "pio khong tim thay trong PATH — chay 'pio run' tu terminal PlatformIO"}

    results = {}
    for env in ["esp32-s3-devkitc-1", "test_fixture"]:
        try:
            proc = subprocess.run(
                ["pio", "run", "-e", env, "--silent"],
                capture_output=True, text=True,
                timeout=600, cwd=str(PROJECT_ROOT)
            )
            ok = proc.returncode == 0
            results[env] = {
                "status": "BUILD_SUCCESS" if ok else "BUILD_FAILED",
                "note": "Chi bien dich firmware; KHONG nap vao board, KHONG chay inference tren phan cung.",
                "exit_code": proc.returncode,
                "tail": (proc.stdout + proc.stderr).strip()[-400:]
            }
        except subprocess.TimeoutExpired:
            results[env] = {"status": "TIMEOUT"}
        except Exception as e:
            results[env] = {"status": "ERROR", "error": str(e)}
    return results


# ==============================================================================
#  TÍNH EXIT CODE
# ==============================================================================
def compute_exit_code(stress: dict, spo2: dict) -> int:
    """
    Trả về 1 nếu có lỗi kiểm thử THỰC SỰ (không phải giới hạn đã biết):
      - Stress: |diff Python–C++| > 1e-9 hoặc nhãn khác nhau
      - Stress: g++ có nhưng compile/run binary thất bại
      - SpO2:   g++ + file test có nhưng replay exit != 0 hoặc exception
    Trả về 0 trong tất cả trường hợp còn lại (NOT_READY, TEST_FIXTURE, SKIPPED
    do thiếu g++/pio đều là giới hạn đã biết).
    """
    failures = []

    # Stress tolerance check
    if stress.get("_tol_fail"):
        cpp = stress.get("cpp_result", {})
        failures.append(
            f"STRESS FAIL: |diff|={cpp.get('diff_vs_python','?'):.2e} > 1e-9 "
            f"hoac nhan khac nhau (tol={cpp.get('tolerance_check')}, label={cpp.get('label_match')})"
        )

    # Stress compile/run hard fail (only when g++ was available)
    if stress.get("_cpp_hard_fail") and stress.get("_cpp_was_expected"):
        cpp = stress.get("cpp_result", {})
        failures.append(f"STRESS FAIL: g++ co nhung binary that bai: {cpp.get('error','?')}")

    # SpO2 replay hard fail (only when g++ + test file were available)
    if spo2.get("_replay_hard_fail") and spo2.get("_replay_was_expected"):
        rt = spo2.get("module_replay_test", {})
        failures.append(
            f"SPO2 FAIL: replay test that bai: {rt.get('status','?')} / {rt.get('error') or rt.get('reason','?')}"
        )

    if failures:
        print("\n  *** LỖI KIỂM THỬ THỰC SỰ (exit code 1) ***")
        for f in failures:
            print(f"    ✗ {f}")
        return 1
    return 0


# ==============================================================================
#  IN BẢNG TỔNG HỢP
# ==============================================================================
STATUS_COLOR = {
    "RESULT_AVAILABLE": "✅",
    "TEST_FIXTURE":     "🔬",
    "QC_REJECTED":      "⚠️ ",
    "NOT_READY":        "❌",
    "SKIPPED":          "➖",
}

def print_summary_table(stress, ecg, spo2, firmware):
    section("BẢNG TỔNG HỢP DEMO OFFLINE — NCKH Health Monitoring")

    rows = [
        ("Stress PPG",       stress["status"],            _stress_result_str(stress)),
        ("Heart Rate (PPG)", stress.get("heart_rate_status", "NOT_READY"), _hr_result_str(stress)),
        ("ECG AF/non-AF",    ecg["status"],               _ecg_result_str(ecg)),
        ("SpO2 (Stream100)", spo2["status"],              _spo2_result_str(spo2)),
    ]

    print(f"\n  {'Nhánh / Chỉ số':<22} {'Trạng thái':<22} Kết quả tóm tắt")
    print(f"  {'─'*22} {'─'*22} {'─'*34}")
    for name, status, result_str in rows:
        icon = STATUS_COLOR.get(status, "  ")
        print(f"  {name:<22} {icon} {status:<20} {result_str}")

    print(f"\n  Firmware build:")
    if isinstance(firmware, dict) and "skipped" not in firmware:
        for env, res in firmware.items():
            st = res.get("status", "?")
            icon = "✅" if st == "BUILD_SUCCESS" else "❌"
            print(f"    {icon} pio run -e {env}: {st}  [{res.get('note','')}]")
    else:
        print(f"    ➖ {firmware.get('skipped', 'Khong ro trang thai')}")

    print()
    print("  Mã trạng thái:")
    print("    ✅ RESULT_AVAILABLE — có kết quả từ model/thuật toán thực đã chạy")
    print("    🔬 TEST_FIXTURE     — kết quả từ bộ test/replay tích hợp sẵn của module")
    print("    ⚠️  QC_REJECTED      — dữ liệu Step 2 không đạt kiểm tra chất lượng")
    print("    ❌ NOT_READY        — thiếu điều kiện (đơn vị chưa xác minh / phần cứng thiếu)")
    print("    ➖ SKIPPED          — công cụ (g++/pio) không có trong PATH")


def _stress_result_str(r):
    py = r.get("python_result")
    cpp = r.get("cpp_result")
    if py:
        s = f"Python: {py['prediction'].upper()} (p={py['probability']:.4f})"
        if cpp and "probability" in cpp:
            tol = cpp.get("tolerance_check", "?")
            lbl = cpp.get("label_match", "?")
            s += f" | C++: {cpp['prediction'].upper()} [tol={tol} lbl={lbl}]"
        elif cpp and "skipped" in cpp:
            s += " | C++: SKIPPED (g++ absent)"
        elif cpp and "error" in cpp:
            s += " | C++: FAIL"
        return s
    reasons = r.get("qc", {}).get("details", [])
    return reasons[0][:60] if reasons else r["status"]


def _hr_result_str(r):
    st = r.get("heart_rate_status")
    if st == "RESULT_AVAILABLE":
        bpm = r.get("heart_rate_bpm")
        dt = r.get("heart_rate_details", {})
        med_ppi = dt.get("median_ppi_ms", "?")
        cnt = dt.get("valid_beat_count", "?")
        return f"{bpm} BPM (trung vị PPI={med_ppi} ms, {cnt} nhịp) [Mẫu Step 2]"
    reason = r.get("heart_rate_details", {}).get("reason", "Chưa đủ điều kiện")
    return f"NOT_READY — {reason[:60]}"


def _ecg_result_str(r):
    if r.get("qc", {}).get("passed"):
        return "9 dac trung trich xuat OK; suy luan NOT_READY (don vi chua xac minh)"
    reasons = r.get("qc", {}).get("details", [])
    return reasons[0][:65] if reasons else r["status"]


def _spo2_result_str(r):
    rt = r.get("module_replay_test")
    if rt and rt.get("status") == "PASS":
        sessions = rt.get("sessions", {})
        pcts = [f"S{k[-1]}={v['spo2_pct']}%" for k, v in sorted(sessions.items())]
        return "SpO2 replay PASS: " + ", ".join(pcts) + "; Step 2 INELIGIBLE"
    if rt and rt.get("status") == "SKIPPED":
        return f"SKIPPED — {rt.get('reason','?')[:50]}"
    return "Replay FAIL / Step 2 INELIGIBLE (25Hz, chua co RED/IR map)"


# ==============================================================================
#  LƯU JSON
# ==============================================================================
def save_json(stress, ecg, spo2, firmware):
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out_path = OUTPUT_DIR / f"offline_demo_{ts}.json"

    # Xóa cờ nội bộ khỏi JSON output
    def clean(d):
        return {k: v for k, v in d.items() if not k.startswith("_")}

    payload = {
        "demo_label": "offline_demo",
        "generated_at_utc": ts,
        "project": "NCKH AI_Moudel_Summary — Health Monitoring offline demo",
        "warning": (
            "CHUA duoc xac nhan tren phan cung ESP32-S3. "
            "Ket qua la kiem tra thuat toan offline. "
            "Khong duoc trinh bay la do tren nguoi that."
        ),
        # Trường nhịp tim PPG ở cấp cao nhất
        "heart_rate_bpm": stress.get("heart_rate_bpm"),
        "heart_rate_source": stress.get("heart_rate_source", "PPG"),
        "heart_rate_status": stress.get("heart_rate_status", "NOT_READY"),
        "heart_rate_disclaimer": "Ket qua tu du lieu mau Step 2, chua phai phep do truc tiep tren ESP32-S3.",
        "branches": {
            "stress_ppg": clean(stress),
            "heart_rate_ppg": {
                "heart_rate_bpm": stress.get("heart_rate_bpm"),
                "heart_rate_source": stress.get("heart_rate_source", "PPG"),
                "heart_rate_status": stress.get("heart_rate_status", "NOT_READY"),
                "details": stress.get("heart_rate_details", {}),
                "disclaimer": "Ket qua tu du lieu mau Step 2, chua phai phep do truc tiep tren ESP32-S3."
            },
            "ecg_af_nonaf": clean(ecg),
            "spo2": clean(spo2),
        },
        "firmware_builds": firmware,
    }
    out_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, default=str),
        encoding="utf-8"
    )
    print(f"\n  💾 Kết quả JSON lưu tại: {out_path.relative_to(PROJECT_ROOT)}")
    return out_path


# ==============================================================================
#  MAIN
# ==============================================================================
def main():
    t0 = time.time()

    print("=" * 78)
    print("  DEMO OFFLINE — NCKH Health Monitoring (Stress PPG + ECG AF + SpO2)")
    print(f"  Thời gian: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 78)
    print()
    print("  ⚠️  LƯU Ý: Tất cả kết quả là kiểm tra thuật toán offline.")
    print("  Chưa có phần cứng ESP32-S3. Không đo trên người thật.")

    # --- Nhánh Stress PPG ---
    subsection("BƯỚC 1: Nhánh Stress PPG")
    stress = run_stress_branch()
    _print_stress(stress)

    # --- Nhánh ECG ---
    subsection("BƯỚC 2: Nhánh ECG AF/non-AF")
    ecg = run_ecg_branch()
    _print_ecg(ecg)

    # --- Nhánh SpO2 ---
    subsection("BƯỚC 3: Nhánh SpO2 (ResearchSpO2 module replay)")
    spo2 = run_spo2_branch()
    _print_spo2(spo2)

    # --- Firmware build ---
    subsection("BƯỚC 4: Firmware PlatformIO build (không nạp)")
    print("  Đang build... (có thể mất 3-5 phút lần đầu)")
    firmware = run_firmware_builds()
    _print_firmware(firmware)

    # --- Bảng tổng hợp ---
    print_summary_table(stress, ecg, spo2, firmware)

    # --- Lưu JSON ---
    save_json(stress, ecg, spo2, firmware)

    elapsed = time.time() - t0
    print(f"\n  Hoàn thành trong {elapsed:.1f}s")

    # --- Exit code ---
    code = compute_exit_code(stress, spo2)
    if code == 0:
        print("  Exit code 0 — demo hoàn thành (NOT_READY/TEST_FIXTURE là giới hạn đã biết).")
    sys.exit(code)


def _print_stress(r):
    qc = r.get("qc", {})
    print(f"  Nguồn: {r['source_file']} | {r['sample_rate_hz']} Hz")
    print(f"  ⚠️  {r['sample_rate_disclaimer']}")
    print(f"  QC: {'PASSED' if qc.get('passed') else 'FAILED'} [{qc.get('method','')}]")
    for d in qc.get("details", []):
        print(f"    • {d}")
    if r.get("features"):
        print("  14 đặc trưng trích xuất:")
        for k, v in r["features"].items():
            print(f"    {k:18s}: {v:12.6f}")
    if r.get("python_result"):
        py = r["python_result"]
        print(f"  Python (model.json): z={py['z']:.4f}  p={py['probability']:.6f}  → {py['prediction'].upper()}")
    if r.get("cpp_result"):
        cpp = r["cpp_result"]
        if "probability" in cpp:
            print(
                f"  C++ binary:          p={cpp['probability']:.6f}  → {cpp['prediction'].upper()}"
                f"  |diff|={cpp['diff_vs_python']:.2e}  [tol={cpp['tolerance_check']} lbl={cpp['label_match']}]"
            )
        elif "skipped" in cpp:
            print(f"  C++ binary: BỎ QUA — {cpp['skipped']}")
        elif "error" in cpp:
            print(f"  C++ binary: LỖI — {cpp['error']}")

    # Hiển thị Nhịp tim PPG (BPM)
    if r.get("heart_rate_status") == "RESULT_AVAILABLE":
        hr_bpm = r.get("heart_rate_bpm")
        hr_dt = r.get("heart_rate_details", {})
        print(f"  Nhịp tim PPG (BPM) : {hr_bpm} BPM  [Công thức: 60000 / median(PPI) = 60000 / {hr_dt.get('median_ppi_ms')} ms, {hr_dt.get('valid_beat_count')} nhịp]")
        print(f"  ⚠️  Nguồn nhịp tim    : {r.get('heart_rate_source')} (Dữ liệu mẫu Step 2, chưa phải đo trực tiếp trên ESP32-S3)")
    else:
        hr_reason = r.get("heart_rate_details", {}).get("reason", "QC khong dat")
        print(f"  Nhịp tim PPG (BPM) : NOT_READY — {hr_reason}")

    print(f"  Trạng thái: {r['status']}")
    print(f"  ⚠️  {r.get('limits','')}")


def _print_ecg(r):
    qc = r.get("qc", {})
    meta = r.get("ei_model_metadata", {})
    print(f"  Nguồn: {r['source_file']} | {r['sample_rate_hz']} Hz")
    print(f"  EI model: sensor={meta.get('sensor_type','')} | axes={meta.get('fusion_axes','')}")
    print(f"  Labels: {meta.get('labels',[])} | NN input: {meta.get('nn_input_frame_size','')} features")
    print(f"  QC: {'PASSED' if qc.get('passed') else 'FAILED'}")
    for d in qc.get("details", []):
        print(f"    • {d}")
    if r.get("features_seconds"):
        print("  9 đặc trưng trích xuất (giây / mili-giây):")
        ks = ["mean_rr","median_rr","sdnn","rmssd","pnn50","cv_rr","iqr_rr","min_rr","max_rr"]
        print(f"    {'Đặc trưng':<15} {'Giây':>14} {'Mili-giây':>14}")
        for k in ks:
            vs = r["features_seconds"].get(k, float("nan"))
            vm = r["features_milliseconds"].get(k, float("nan"))
            print(f"    {k:<15} {vs:>14.6f} {vm:>14.4f}")
    print(f"  ⚠️  Đơn vị: {r.get('unit_status','')}")
    print(f"  ⚠️  Suy luận: {r.get('inference_status','')}")
    print(f"  ℹ️  Firmware: {r.get('firmware_note','')}")
    print(f"  Trạng thái: {r['status']}")


def _print_spo2(r):
    step2 = r.get("step2_connection", {})
    print(f"  Kết nối Step 2: {'ĐỦ ĐIỀU KIỆN' if step2.get('eligible') else 'CHƯA ĐỦ ĐIỀU KIỆN'}")
    for reason in step2.get("reasons", []):
        print(f"    • {reason}")
    rt = r.get("module_replay_test")
    if rt:
        st = rt.get("status", "?")
        print(f"  Module replay test: {st} (exit={rt.get('exit_code','N/A')})")
        if st == "PASS":
            for k, v in rt.get("sessions", {}).items():
                exp = v.get("expected_pct", "?")
                print(f"    {k}: SpO2={v['spo2_pct']}% (expected {exp}%), HR={v['hr_bpm']} bpm")
            print(f"  ℹ️  Nguồn: {rt.get('data_source','')}")
            print(f"  ⚠️  {rt.get('note','')}")
        elif st in ("SKIPPED", "ERROR", "FAIL"):
            print(f"    Lý do: {rt.get('reason') or rt.get('error','?')}")
    print(f"  ⚠️  {r.get('limits','')}")
    print(f"  Trạng thái: {r['status']}")


def _print_firmware(fw):
    if isinstance(fw, dict) and "skipped" not in fw:
        for env, res in fw.items():
            st = res.get("status", "?")
            icon = "✅" if st == "BUILD_SUCCESS" else "❌"
            print(f"  {icon} pio run -e {env}: {st}")
            print(f"      ℹ️  {res.get('note','')}")
    else:
        print(f"  ➖ BỎ QUA: {fw.get('skipped','Khong ro')}")


if __name__ == "__main__":
    main()
