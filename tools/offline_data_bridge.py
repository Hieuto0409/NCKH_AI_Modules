#!/usr/bin/env python3
"""
offline_data_bridge.py
----------------------
Cầu nối dữ liệu offline từ gói Step 2 của nhóm trưởng (đặt trong lib/)
đến các giao diện của ba module:
  1. Stress PPG (14 đặc trưng double)
  2. ECG AF/non-AF (9 đặc trưng float)
  3. SpO2 (cặp IR/RED thô 100 Hz)

QUY TẮC BẢO TOÀN VÀ ĐIỀU KIỆN CHẶT CHẼ:
  - Chỉ đọc file trong lib/step2_handoff_sqi_v0_1-20260924T133622Z-1-001/step2_handoff_sqi_v0_1/
  - Không nội suy PPG 25 Hz -> 64 Hz (Stress) hay 100 Hz (SpO2) rồi tuyên bố tương đương.
  - Không tự gán slot0/slot1 là RED hay IR khi chưa có tài liệu mapping.
  - Không đưa tín hiệu đã lọc/AC vào hàm pushSpo2.
  - Kiểm tra QC nghiêm ngặt: nếu không đạt QC, từ chối và ghi rõ lý do, không điền số 0 để gọi model.
  - Đánh dấu rõ trạng thái xác minh đơn vị và độ chính xác phần cứng.
"""

import sys, os, json, math
from pathlib import Path
import numpy as np
import pandas as pd
from scipy.signal import butter, find_peaks, sosfiltfilt

# Thiết lập encoding cho console Windows
if sys.stdout.encoding and sys.stdout.encoding.lower().startswith('cp'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PROJECT_ROOT = Path(__file__).resolve().parent.parent
STEP2_DIR = PROJECT_ROOT / "lib" / "step2_handoff_sqi_v0_1-20260924T133622Z-1-001" / "step2_handoff_sqi_v0_1"
STRESS_MODEL_JSON = PROJECT_ROOT / "lib" / "Stress_PPG_60s_model" / "model.json"

# ==============================================================================
# 1. NHÁNH STRESS PPG: Trích xuất 14 đặc trưng từ 60 giây PPG thô Step 2
# ==============================================================================
def process_stress_ppg_60s(csv_path: Path, channel_name: str = "slot1_raw"):
    """
    Trích xuất đúng 14 đặc trưng HR/HRV theo chuẩn WESAD từ cửa sổ 60s liên tục.
    Tham chiếu: lib/Stress_PPG_60s_model/tao_dataset_stress_hrv_60s.py
    """
    report = {
        "branch": "Stress PPG",
        "file": str(csv_path.relative_to(PROJECT_ROOT)),
        "channel": channel_name,
        "fs_hz": 25.0,
        "duration_s": 60.0,
        "qc_passed": False,
        "rejection_reasons": [],
        "features": {},
        "heart_rate_bpm": None,
        "heart_rate_source": "PPG",
        "heart_rate_status": "NOT_READY",
        "heart_rate_disclaimer": "Ket qua tu du lieu mau Step 2, chua phai phep do truc tiep tren ESP32-S3.",
        "model_invoked": False,
        "model_result": None
    }

    if not csv_path.exists():
        report["rejection_reasons"].append(f"File khong ton tai: {csv_path}")
        return report

    df = pd.read_csv(csv_path)
    required_cols = ["sample_index", "timestamp_us", channel_name, "fifo_ok", "fifo_overflow_count"]
    for col in required_cols:
        if col not in df.columns:
            report["rejection_reasons"].append(f"Thieu cot du lieu bat buoc: {col}")
            return report

    # 1. Kiểm tra 60 giây liên tục (ở 25 Hz, 60s = 1500 mẫu)
    fs = 25.0
    target_samples = int(60.0 * fs) # 1500
    if len(df) < target_samples:
        report["rejection_reasons"].append(f"So mau ({len(df)}) < 60 giay ({target_samples} mau)")
        return report

    seg = df.iloc[:target_samples].copy()
    idx = seg["sample_index"].to_numpy(dtype=np.int64)
    ts = seg["timestamp_us"].to_numpy(dtype=np.int64)
    raw = seg[channel_name].to_numpy(dtype=float)

    # Kiểm tra sample continuity
    missing_samples = int((idx[-1] - idx[0] + 1) - len(idx))
    if missing_samples > 0:
        report["rejection_reasons"].append(f"Phat hien {missing_samples} mau bi mat (sample_index regression/gap)")

    d_ts = np.diff(ts)
    if np.any(d_ts <= 0):
        report["rejection_reasons"].append("Timestamp_us bi giam hoac dung yen (regression)")

    dt_median = float(np.median(d_ts))
    if abs(dt_median - 40000.0) > 1000.0:
        report["rejection_reasons"].append(f"dt trung vi = {dt_median} us khong phu hop 25 Hz (ky vong 40000 us)")

    # Kiểm tra cờ FIFO
    fifo_invalid = int(np.sum(seg["fifo_ok"] != 1))
    if fifo_invalid > 0:
        report["rejection_reasons"].append(f"Co {fifo_invalid} mau co fifo_ok != 1")

    fifo_overflow = int(np.sum(seg["fifo_overflow_count"] > 0))
    if fifo_overflow > 0:
        report["rejection_reasons"].append(f"Phat hien {fifo_overflow} mau bi tran FIFO")

    # Kiểm tra tính hữu hạn & flatline
    if not np.all(np.isfinite(raw)):
        report["rejection_reasons"].append("Tin hieu raw chua NaN hoac Inf")
    if float(np.std(raw)) <= 1e-6 or float(np.ptp(raw)) <= 1e-6:
        report["rejection_reasons"].append("Tin hieu raw bi phang hoan toan (flatline)")

    if report["rejection_reasons"]:
        return report

    # 2. Lọc dải thông BVP [0.5, 4.0] Hz
    sos = butter(4, [0.5, 4.0], btype="bandpass", fs=fs, output="sos")
    filtered = sosfiltfilt(sos, raw)

    # 3. Tìm đỉnh (thử cả 2 chiều phân cực theo WESAD pipeline)
    min_dist = max(1, int(round(fs * (60.0 / 180.0)))) # 8 mẫu (~180 bpm)
    candidates = []
    for polarity in (1, -1):
        oriented = filtered * polarity
        prominence = max(0.20 * float(np.std(oriented)), 1e-8)
        peaks, _ = find_peaks(oriented, distance=min_dist, prominence=prominence)
        raw_rr = np.diff(peaks) / fs
        phys = raw_rr[(raw_rr >= 60.0 / 180.0) & (raw_rr <= 60.0 / 40.0)]
        if len(phys) > 0:
            med = float(np.median(phys))
            cleaned = phys[(phys >= med * 0.70) & (phys <= med * 1.30)]
            ratio = float(len(cleaned) / len(raw_rr)) if len(raw_rr) else 0.0
        else:
            cleaned = np.array([])
            ratio = 0.0
        score = ratio + (1.0 if 40 <= len(peaks) <= 186 else 0.0)
        candidates.append((score, peaks, cleaned, ratio, polarity))

    _, peaks, rr_sec, valid_ratio, polarity = max(candidates, key=lambda x: x[0])

    # Kiểm tra ngưỡng chấp nhận theo tao_dataset_stress_hrv_60s.py
    if len(peaks) < 40:
        report["rejection_reasons"].append(f"So nhip phat hien ({len(peaks)}) < 40 nhip (nguong WESAD MIN_BEATS)")
    if valid_ratio < 0.65:
        report["rejection_reasons"].append(f"Ty le khoang PP hop le ({valid_ratio:.3f}) < 0.65 (MIN_VALID_RR_RATIO)")

    if report["rejection_reasons"]:
        return report

    # 4. Tính toán đủ 14 đặc trưng đúng đơn vị
    rr_ms = rr_sec * 1000.0
    diff_ms = np.diff(rr_ms)
    hr = 60.0 / rr_sec
    mean_pp = float(np.mean(rr_ms))

    feats = {
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
        "beat_count": float(len(peaks)),
        "valid_rr_ratio": float(valid_ratio),
    }

    report["qc_passed"] = True
    report["features"] = feats
    report["heart_rate_bpm"] = round(float(60000.0 / feats["median_pp_ms"]), 2)
    report["heart_rate_status"] = "RESULT_AVAILABLE"

    # 5. Gọi thử nghiệm mô hình triển khai (deployment model)
    if STRESS_MODEL_JSON.exists():
        model = json.loads(STRESS_MODEL_JSON.read_text(encoding="utf-8"))
        features_ordered = [feats[name] for name in model["features_in_order"]]
        x = np.array(features_ordered, dtype=float)
        mean = np.array(model["scaler_mean"], dtype=float)
        scale = np.array(model["scaler_scale"], dtype=float)
        coef = np.array(model["logistic_coefficient"], dtype=float)
        z = float(model["logistic_intercept"] + np.dot((x - mean) / scale, coef))
        p = 1.0 / (1.0 + np.exp(-z)) if z >= 0 else np.exp(z) / (1.0 + np.exp(z))
        pred = "stress" if p >= model["threshold"] else "baseline"
        report["model_invoked"] = True
        report["model_result"] = {
            "z": z,
            "probability": p,
            "prediction": pred,
            "disclaimer": "Kiem tra ket noi phan mem offline; CHUA XAC NHAN do chinh xac tren MAX30102 vi tan so goc 25 Hz khac 64 Hz WESAD"
        }

    return report


# ==============================================================================
# 2. NHÁNH ECG AF/NON-AF: Trích xuất 9 đặc trưng từ ECG Step 2
# ==============================================================================
def process_ecg_offline(csv_path: Path):
    """
    Trích xuất 9 đặc trưng từ pipeline Pan-Tompkins của Step 2 và đánh giá QC.
    """
    report = {
        "branch": "ECG AF/non-AF",
        "file": str(csv_path.relative_to(PROJECT_ROOT)),
        "fs_hz": 500.0,
        "qc_passed": False,
        "rejection_reasons": [],
        "features_seconds": {},
        "features_milliseconds": {},
        "model_invoked": False,
        "model_refusal_reason": None
    }

    if not csv_path.exists():
        report["rejection_reasons"].append(f"File khong ton tai: {csv_path}")
        return report

    df = pd.read_csv(csv_path)
    for col in ["sample_index", "timestamp_us", "ecg_raw", "leads_off", "adc_ok"]:
        if col not in df.columns:
            report["rejection_reasons"].append(f"Thieu cot: {col}")
            return report

    fs = 500.0
    target_samples = int(30.0 * fs) # 15000 mẫu cho cửa sổ 30s theo tập huấn luyện
    if len(df) < target_samples:
        report["rejection_reasons"].append(f"Số mẫu ({len(df)}) < {target_samples} cho cửa sổ 30s")
        return report

    # Quét các ứng viên cửa sổ 30s (bước nhảy 5s) để tìm đoạn sạch đạt QC và có nhiều khoảng RR hợp lệ nhất
    step_samples = int(5.0 * fs)
    candidate_starts = list(range(0, len(df) - target_samples + 1, step_samples))
    if candidate_starts[-1] != (len(df) - target_samples):
        candidate_starts.append(len(df) - target_samples)

    best_candidate = None
    rejection_notes = []

    for start_idx in candidate_starts:
        seg_df = df.iloc[start_idx : start_idx + target_samples]
        raw = seg_df["ecg_raw"].to_numpy(dtype=float)

        clean_fraction = float(np.mean((seg_df["leads_off"] == 0) & (seg_df["adc_ok"] == 1)))
        adc_invalid = float(np.mean(seg_df["adc_ok"] == 0))
        rail_fraction = float(np.mean((raw <= 10.0) | (raw >= 4085.0)))
        flat_ratio = float(np.mean(np.diff(raw) == 0))

        if clean_fraction < 0.80 or adc_invalid > 0.005 or rail_fraction > 0.005 or flat_ratio > 0.05:
            rejection_notes.append(f"Cửa sổ {start_idx/fs:.1f}s: QC phần cứng không đạt (clean={clean_fraction:.2f})")
            continue

        sos_morph = butter(2, [0.5, 40.0], btype="bandpass", fs=fs, output="sos")
        sos_qrs = butter(2, [5.0, 20.0], btype="bandpass", fs=fs, output="sos")
        ecg_morph = sosfiltfilt(sos_morph, raw)
        ecg_qrs = sosfiltfilt(sos_qrs, raw)

        derivative = np.diff(ecg_qrs, prepend=ecg_qrs[0])
        energy = derivative * derivative
        width = max(1, int(round(0.15 * fs)))
        integrated = np.convolve(energy, np.ones(width) / width, mode="same")
        std_int = float(np.std(integrated))

        if std_int <= 1e-12:
            rejection_notes.append(f"Cửa sổ {start_idx/fs:.1f}s: Năng lượng QRS quá thấp")
            continue

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
            rejection_notes.append(f"Cửa sổ {start_idx/fs:.1f}s: Số đỉnh R ({len(peaks)}) < 4")
            continue

        rr_sec = np.diff(peaks) / fs
        valid_rr_sec = rr_sec[(rr_sec >= 0.20) & (rr_sec <= 2.0)]
        if len(valid_rr_sec) < 3:
            rejection_notes.append(f"Cửa sổ {start_idx/fs:.1f}s: Số khoảng RR hợp lệ ({len(valid_rr_sec)}) < 3")
            continue

        if best_candidate is None or len(valid_rr_sec) > len(best_candidate["valid_rr_sec"]):
            best_candidate = {
                "start_idx": start_idx,
                "start_second": start_idx / fs,
                "clean_fraction": clean_fraction,
                "adc_invalid": adc_invalid,
                "rail_fraction": rail_fraction,
                "flat_ratio": flat_ratio,
                "peaks": peaks,
                "valid_rr_sec": valid_rr_sec,
            }

    if best_candidate is None:
        report["rejection_reasons"] += rejection_notes[:5]
        report["model_refusal_reason"] = "QC phan cung khong dat -> TU CHOI goi model Edge Impulse de tranh suy luan sai"
        return report

    valid_rr_sec = best_candidate["valid_rr_sec"]
    valid_rr_ms = valid_rr_sec * 1000.0

    # 4. Tính toán 9 đặc trưng (đơn vị huấn luyện: giây, %, 1)
    diff_sec = np.diff(valid_rr_sec)
    diff_ms = np.diff(valid_rr_ms)

    def calc_9(rr, diff, is_ms=False):
        mean_v = float(np.mean(rr))
        med_v = float(np.median(rr))
        sdnn_v = float(np.std(rr, ddof=1))
        rmssd_v = float(np.sqrt(np.mean(diff ** 2)))
        pnn50_v = float(100.0 * np.mean(np.abs(diff) > (50.0 if is_ms else 0.050)))
        cv_rr_v = sdnn_v / mean_v if mean_v > 0 else 0.0
        q75, q25 = np.percentile(rr, [75, 25])
        iqr_v = float(q75 - q25)
        min_v = float(np.min(rr))
        max_v = float(np.max(rr))
        return {
            "mean_rr": mean_v,
            "median_rr": med_v,
            "sdnn": sdnn_v,
            "rmssd": rmssd_v,
            "pnn50": pnn50_v,
            "cv_rr": cv_rr_v,
            "iqr_rr": iqr_v,
            "min_rr": min_v,
            "max_rr": max_v
        }

    report["qc_passed"] = True
    report["features_seconds"] = calc_9(valid_rr_sec, diff_sec, is_ms=False)
    report["features_milliseconds"] = calc_9(valid_rr_ms, diff_ms, is_ms=True)
    report["unit_status"] = (
        "ĐÃ XÁC MINH từ tập huấn luyện (ECG.rar bản _B): 7 đặc trưng thời gian dùng giây (s); "
        "pnn50 dùng %; cv_rr không thứ nguyên. Khớp tham số scaler_mean của model."
    )

    # 5. Đánh giá khả năng gọi mô hình Edge Impulse
    report["model_invoked"] = False
    report["model_refusal_reason"] = (
        "NOT_READY — Model Edge Impulse được xuất dưới dạng thư viện MCU/Arduino nhắm đến ESP32-S3 "
        "(đã biên dịch thành công trong firmware PlatformIO). "
        "Môi trường PC x86 thiếu runtime TFLite Micro tương thích nên chưa thể thực thi inference offline trực tiếp. "
        "Giữ NOT_READY theo đúng quy định, không tạo nhãn giả lập hoặc quy tắc tự viết."
    )
    return report


# ==============================================================================
# 3. NHÁNH SpO2: Kiểm tra tính tương thích dữ liệu Step 2
# ==============================================================================
def inspect_spo2_bridge(csv_path: Path):
    """
    Kiểm tra điều kiện kết nối từ dữ liệu Step 2 vào ResearchSpO2 (Stream100).
    """
    report = {
        "branch": "SpO2 (MaximCore / Stream100)",
        "file": str(csv_path.relative_to(PROJECT_ROOT)) if csv_path.exists() else str(csv_path),
        "fs_hz": 25.0,
        "eligible_to_connect": False,
        "rejection_reasons": [],
        "note": ""
    }

    if not csv_path.exists():
        report["rejection_reasons"].append("File khong ton tai")
        return report

    df = pd.read_csv(csv_path)
    # Lý do 1: Tần số mẫu
    report["rejection_reasons"].append(
        "Bat dong tan so mau: Step 2 PPG co dt = 40.000 us (25 Hz), "
        "trong khi ResearchSpO2::Stream100 yeu cau luong mau thuc tai dung 100 Hz."
    )
    # Lý do 2: Chưa mapping bước sóng
    report["rejection_reasons"].append(
        "Chua dinh danh buoc song: File chi ghi slot0_raw va slot1_raw, "
        "chua xac minh slot nao la RED, slot nao la IR. Dao nguoc slot se lam dao nguoc ty so R."
    )
    # Lý do 3: Tín hiệu AC/DC
    report["rejection_reasons"].append(
        "Quy tac bao toan: Khong noi suy len 100 Hz va khong truyen tin hieu AC da loc vao pushSpo2(). "
        "Thuật toan Maxim yeu cau mau thô de tu phan tach AC va DC."
    )

    report["note"] = (
        "Nhanh SpO2 tu Step 2 duoc danh dau la: CHUA DU DIEU KIEN NOI. "
        "Module SpO2 duoc kiem tra bang bo test replay 100 Hz doc lap rieng co san."
    )
    return report


def main():
    print("================================================================================")
    print("=== CONG CU CAU NOI DU LIEU OFFLINE STEP 2 -> HEALTH MONITORING MODULES ===")
    print("================================================================================")
    print(f"Thu muc Step 2 su dung:\n  {STEP2_DIR}\n")

    # 1. Stress PPG
    ppg_raw_file = STEP2_DIR / "raw" / "ppg_raw(5).csv"
    print("--------------------------------------------------------------------------------")
    print("1. KHAO SAT & TRICH XUAT NHANH STRESS PPG (Cua so 60 giay lien tuc)")
    print("--------------------------------------------------------------------------------")
    res_ppg_s1 = process_stress_ppg_60s(ppg_raw_file, channel_name="slot1_raw")
    res_ppg_s0 = process_stress_ppg_60s(ppg_raw_file, channel_name="slot0_raw")

    print(f"Nguon: {res_ppg_s1['file']} | Kich thuoc: 60 giay | Tan so: {res_ppg_s1['fs_hz']} Hz")
    print("Ly do chon kenh: 'slot1_raw' co bien do dong (perfusion index ~ 0.042) lon hon")
    print("  so voi 'slot0_raw' (perfusion ~ 0.013). Cung kiem tra tren ca hai kenh.\n")

    for r in [res_ppg_s1, res_ppg_s0]:
        ch = r["channel"]
        print(f"[*] Ket qua kiem tra chat luong (QC) cho {ch}:")
        if r["qc_passed"]:
            print("    QC PASSED (fifo_ok=100%, 0 overflow, >40 nhip, valid_rr_ratio >= 0.65)")
            print("    14 dac trung trich xuat dung chuan stress_ppg::kFeatureNames:")
            for k, v in r["features"].items():
                print(f"      - {k:16s}: {v:12.6f}")
            print(f"    Nhịp tim PPG (BPM) : {r.get('heart_rate_bpm')} BPM (60000 / {r['features']['median_pp_ms']:.1f} ms)")
            print(f"    Nguồn nhịp tim     : {r.get('heart_rate_source')} [{r.get('heart_rate_disclaimer')}]")
            if r["model_invoked"]:
                mr = r["model_result"]
                print(f"    Goi thu nghiem model Stress C++ (deployment):")
                print(f"      z = {mr['z']:.4f} | prob = {mr['probability']:.4f} | Du doan = {mr['prediction'].upper()}")
                print(f"      CẢNH BÁO: {mr['disclaimer']}")
        else:
            print("    QC REJECTED:")
            for reason in r["rejection_reasons"]:
                print(f"      x {reason}")
        print()

    # 2. ECG AF/non-AF
    print("--------------------------------------------------------------------------------")
    print("2. KHAO SAT & TRICH XUAT NHANH ECG AF/NON-AF (Pan-Tompkins + 9 dac trung)")
    print("--------------------------------------------------------------------------------")
    ecg_clean_file = STEP2_DIR / "raw" / "ecg_raw(8).csv"
    ecg_noisy_file = STEP2_DIR / "raw" / "ecg_raw(9).csv"

    for f, desc in [(ecg_clean_file, "ECG SACH (ecg_raw(8).csv)"), (ecg_noisy_file, "ECG NHIEU (ecg_raw(9).csv)")]:
        print(f"[*] Thu nghiem tren file: {desc}")
        r_ecg = process_ecg_offline(f)
        if r_ecg["qc_passed"]:
            print("    QC PASSED: clean_fraction >= 0.80, rail <= 0.005, flat <= 0.05")
            print("    Bang 9 dac trung theo 2 phong doan don vi:")
            print("    +--------------------+----------------------+----------------------+")
            print("    | Dac trung          | Gia tri (Giay - s)   | Gia tri (Mili-giay)  |")
            print("    +--------------------+----------------------+----------------------+")
            for k in ["mean_rr", "median_rr", "sdnn", "rmssd", "pnn50", "cv_rr", "iqr_rr", "min_rr", "max_rr"]:
                val_s = r_ecg["features_seconds"][k]
                val_ms = r_ecg["features_milliseconds"][k]
                print(f"    | {k:18s} | {val_s:20.6f} | {val_ms:20.6f} |")
            print("    +--------------------+----------------------+----------------------+")
            print(f"    TRANG THAI MODEL: {r_ecg['model_refusal_reason']}\n")
        else:
            print("    QC REJECTED:")
            for reason in r_ecg["rejection_reasons"]:
                print(f"      x {reason}")
            print(f"    TRANG THAI MODEL: {r_ecg['model_refusal_reason']}\n")

    # 3. SpO2
    print("--------------------------------------------------------------------------------")
    print("3. KHAO SAT NHANH SpO2 (Kiem tra tinh tuong thich luong du lieu)")
    print("--------------------------------------------------------------------------------")
    r_spo2 = inspect_spo2_bridge(ppg_raw_file)
    print(f"Nguon du lieu khao sat: {r_spo2['file']}")
    print(f"Trang thai ket noi: {'DU DIEU KIEN' if r_spo2['eligible_to_connect'] else 'CHUA DU DIEU KIEN NOI'}")
    print("Cac ly do ky thuat ngan chan:")
    for reason in r_spo2["rejection_reasons"]:
        print(f"  - {reason}")
    print(f"\nKET LUAN: {r_spo2['note']}")
    print("================================================================================\n")

if __name__ == "__main__":
    main()
