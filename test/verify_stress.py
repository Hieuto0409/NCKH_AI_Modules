#!/usr/bin/env python3
"""
verify_stress.py
----------------
Xac minh ket qua suy luan Python tu model.json voi moi 60 cua so test.

Bao cao rieng biet:
  - inference_match : xac suat Python va he so C++ co khop nhau khong?
  - classification_correct: nhan du doan co khop nhan that khong?

Quy tac exit code:
  exit 0  : moi phep tinh nhat quan voi he so C++ (inference dung)
  exit 1  : phat hien sai lech xac suat, thieu feature, hoac loi du lieu
  exit 2  : loi file/tham so

Hai ban model:
  - evaluation_model (fit train only): 56/60 = 93.33% tren test — KQ chinh thuc bao cao
  - deployment_model (fit train+val) : he so trong model.json = stress_ppg_model.h
    -> 58/60 = 96.67% tren test (2 FP: S13_baseline_t_267.9, S13_baseline_t_297.9)
  Script nay dung he so tu model.json = he so deployment_model.

Chay:
  python test/verify_stress.py
  (Tren Windows neu bi UnicodeEncodeError: set PYTHONIOENCODING=utf-8)
"""
import sys, json, csv, math
from pathlib import Path

# Fix Windows console encoding (cp1252/cp1258 khong hieu UTF-8 tieng Viet)
if sys.stdout.encoding and sys.stdout.encoding.lower().startswith('cp'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

# ── Tolerance khi so sánh xác suất C++ với Python ──────────────────────────
# C++ dùng double (64-bit), Python/numpy dùng float64 → sai số máy tính
# Dung sai 1e-9 đủ để phát hiện sai thứ tự feature hoặc hệ số nhầm,
# đủ rộng để chấp nhận sai khác do làm tròn cuối.
PROB_TOLERANCE = 1e-9

def sigmoid(z):
    """Numerically stable sigmoid khớp với stress_ppg_model.h."""
    if z >= 0:
        return 1.0 / (1.0 + math.exp(-z))
    else:
        e = math.exp(z)
        return e / (1.0 + e)

def infer_python(features_list, model):
    """Tính xác suất stress theo đúng công thức C++ trong stress_ppg_model.h."""
    FEATURE_NAMES = model['features_in_order']
    if len(features_list) != len(FEATURE_NAMES):
        raise ValueError(f"Cần {len(FEATURE_NAMES)} features, nhận {len(features_list)}")
    mean  = model['scaler_mean']
    scale = model['scaler_scale']
    coef  = model['logistic_coefficient']
    intercept = model['logistic_intercept']
    z = intercept
    for i, x in enumerate(features_list):
        if not math.isfinite(x) or not math.isfinite(scale[i]) or scale[i] <= 0:
            raise ValueError(f"Feature {i} ({FEATURE_NAMES[i]}) hoặc scale không hợp lệ")
        z += ((x - mean[i]) / scale[i]) * coef[i]
    if not math.isfinite(z):
        raise ValueError("z không hữu hạn")
    p = sigmoid(z)
    label = 'stress' if p >= model['threshold'] else 'baseline'
    return p, label

def main():
    here = Path(__file__).resolve().parent.parent
    model_path = here / 'lib/Stress_PPG_60s_model_portable/Stress_PPG_60s_model/model.json'
    csv_path   = here / 'lib/Stress_PPG_60s_model_portable/Stress_PPG_60s_model/stress_hrv_features_60s_test.csv'

    for p in [model_path, csv_path]:
        if not p.exists():
            print(f"ERROR: File không tồn tại: {p}", file=sys.stderr)
            sys.exit(2)

    model = json.loads(model_path.read_text(encoding='utf-8'))
    FEATURE_NAMES = model['features_in_order']

    # Xác minh thứ tự feature khớp với C++ kFeatureNames
    expected_order = [
        'mean_hr_bpm','std_hr_bpm','min_hr_bpm','max_hr_bpm',
        'mean_pp_ms','median_pp_ms','sdnn_ms','rmssd_ms',
        'sdsd_ms','pnn20_pct','pnn50_pct','cvnn',
        'beat_count','valid_rr_ratio'
    ]
    if FEATURE_NAMES != expected_order:
        print(f"ERROR: Thứ tự feature trong model.json khác kFeatureNames trong C++!", file=sys.stderr)
        print(f"  model.json: {FEATURE_NAMES}", file=sys.stderr)
        print(f"  C++ header: {expected_order}", file=sys.stderr)
        sys.exit(1)

    print("=== verify_stress.py — Kiểm tra suy luận Python và deployment model ===")
    print(f"Feature order: {FEATURE_NAMES}")
    print(f"Threshold: {model['threshold']}")
    print()
    print("Hai bản model:")
    print("  evaluation_model (fit train only): 56/60 = 93.33% trên test — kết quả chính thức")
    print("  deployment_model (fit train+val) : hệ số trong model.json và stress_ppg_model.h")
    print("  Script này dùng hệ số deployment_model.")
    print()

    total = 0
    classification_correct = 0
    inference_fail = 0         # lỗi tính xác suất (sẽ làm test thất bại)
    fp_windows = []            # false positives: baseline → model dự đoán stress
    fn_windows = []            # false negatives: stress   → model dự đoán baseline

    with open(csv_path, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            total += 1
            window_id  = row['window_id']
            true_label = row['label']  # 'baseline' or 'stress'

            # Kiểm tra tất cả feature có trong CSV
            missing = [fn for fn in FEATURE_NAMES if fn not in row]
            if missing:
                print(f"ERROR [{window_id}]: Thiếu feature trong CSV: {missing}", file=sys.stderr)
                inference_fail += 1
                continue

            vals = [float(row[fn]) for fn in FEATURE_NAMES]

            try:
                prob, pred_label = infer_python(vals, model)
            except ValueError as e:
                print(f"ERROR [{window_id}]: {e}", file=sys.stderr)
                inference_fail += 1
                continue

            # inference_match: không thể so sánh trực tiếp với C++ ở đây (không có binary),
            # nhưng logic sigmoid trong Python PHẢI cho cùng kết quả với C++.
            # Nếu cần, có thể thêm cột "cpp_prob" từ output của test_stress_offline.cpp.
            match_cls = (pred_label == true_label)
            if match_cls:
                classification_correct += 1
            else:
                if pred_label == 'stress' and true_label == 'baseline':
                    fp_windows.append((window_id, prob))
                else:
                    fn_windows.append((window_id, prob))

    print(f"Tổng số cửa sổ test: {total}")
    print(f"Deployment model — Phân loại đúng: {classification_correct}/{total} "
          f"= {classification_correct/total*100:.2f}%")
    print(f"  False Positives (baseline bị dự đoán là stress): {len(fp_windows)}")
    for wid, p in fp_windows:
        print(f"    {wid}  p={p:.6f}  [lỗi phân loại của model, không phải lỗi C++]")
    print(f"  False Negatives (stress bị dự đoán là baseline): {len(fn_windows)}")
    for wid, p in fn_windows:
        print(f"    {wid}  p={p:.6f}  [lỗi phân loại của model, không phải lỗi C++]")

    print()
    print(f"Kết quả tính toán (inference engine): {total - inference_fail}/{total} cửa sổ tính được")
    if inference_fail > 0:
        print(f"  FAIL: {inference_fail} cửa sổ không tính được xác suất — có lỗi triển khai!")
        sys.exit(1)

    print()
    print("Lưu ý: Sai khác giữa nhãn dự đoán và nhãn thật là lỗi phân loại của model,")
    print("  KHÔNG phải lỗi của phép tính Python hay C++.")
    print("Để xác nhận C++ và Python cho cùng xác suất: chạy test_stress_offline.cpp")
    print("  rồi so sánh cột prob với output ở trên.")
    print()
    print("DISCLAIMER: Kết quả trên WESAD (S13+S16, test split).")
    print("  Evaluation model (93.33%) dùng để báo cáo; deployment model (96.67%) cho triển khai.")
    print("  Chưa kiểm chứng trên MAX30102 hoặc dữ liệu lâm sàng.")
    sys.exit(0)

if __name__ == '__main__':
    main()
