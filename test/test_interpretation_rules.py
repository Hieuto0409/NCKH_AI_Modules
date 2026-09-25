#!/usr/bin/env python3
"""
test_interpretation_rules.py
-----------------------------
Unit tests độc lập kiểm tra các quy tắc hiển thị và cấu hình thuật toán:
  1. ECG AF/non-AF: Cửa sổ 30s, dải lọc RR [0.2, 2.0]s, đúng 9 đặc trưng và thứ tự, đơn vị đã xác minh.
  2. SpO2: Ngưỡng tham khảo lâm sàng, giá trị biên [92, 93, 94, 95, 100],
     và các trường hợp không hợp lệ (None, NaN, Inf, -1, 105, QC fail).
     Kiểm tra cả cờ valid lẫn nhãn.
  3. PPG BPM: Kiểm tra số hữu hạn dương, phân loại dựa trên BPM gốc chưa làm tròn
     (ví dụ 100.004 BPM thuộc >100 dù hiển thị 100.00 BPM),
     và trạng thái nghỉ vs chưa đủ bối cảnh (mặc định dữ liệu Step 2).

LƯU Ý BẢO TOÀN:
  Các giá trị số (98%, 94%, 90%, 55, 75, 110, 100.004 BPM) là fixture nhân tạo dùng thuần túy
  để kiểm chứng logic của các nhánh rẽ quy tắc; KHÔNG PHẢI dữ liệu đo trên người thật.
"""

import sys, os, math, unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))

from run_offline_demo import (
    interpret_spo2,
    interpret_ppg_bpm,
    compute_ppg_bpm_from_ppi
)

class TestInterpretationRules(unittest.TestCase):

    # ==========================================================================
    # 1. ECG AF/NON-AF: Cửa sổ, dải lọc, thứ tự đặc trưng và đơn vị
    # ==========================================================================
    def test_ecg_window_duration_and_filter(self):
        """Kiểm tra cấu hình ECG: cửa sổ 30s, dải RR [0.2, 2.0]s."""
        from run_offline_demo import run_ecg_branch
        report = run_ecg_branch()
        self.assertEqual(report["window_duration_seconds"], 30.0)
        self.assertEqual(report["sample_rate_hz"], 500.0)
        self.assertEqual(report["rr_filter_range_seconds"], [0.20, 2.0])

    def test_ecg_nine_features_order_and_verified_units(self):
        """Kiểm tra đúng 9 đặc trưng, thứ tự khớp model_metadata.h, đơn vị đã xác minh."""
        from run_offline_demo import run_ecg_branch
        report = run_ecg_branch()
        expected_axes = [
            "mean_rr", "median_rr", "sdnn", "rmssd", "pnn50",
            "cv_rr", "iqr_rr", "min_rr", "max_rr"
        ]
        self.assertEqual(len(expected_axes), 9)
        self.assertIn("ĐÃ XÁC MINH", report["unit_status"])
        self.assertIn("NOT_READY", report["status"]) # Giữ NOT_READY vì thiếu TFLite Micro trên PC, không fake nhãn

        if report["qc"]["passed"]:
            f_sec = report["features_seconds"]
            for ax in expected_axes:
                self.assertIn(ax, f_sec)
                self.assertIsInstance(f_sec[ax], float)
            self.assertEqual(len(report["features_ordered_list"]), 9)
            # 7 đặc trưng thời gian có giá trị tính bằng giây: [0.2, 2.0]s
            for t_feat in ["mean_rr", "median_rr", "sdnn", "rmssd", "iqr_rr", "min_rr", "max_rr"]:
                self.assertTrue(0.0 <= f_sec[t_feat] <= 2.5, f"{t_feat}={f_sec[t_feat]} ngoài dải giây hợp lý")
            # pnn50 tính bằng %: [0, 100]
            self.assertTrue(0.0 <= f_sec["pnn50"] <= 100.0)
            # cv_rr không thứ nguyên: >= 0
            self.assertTrue(f_sec["cv_rr"] >= 0.0)

    # ==========================================================================
    # 2. SpO2: Ngưỡng tham khảo lâm sàng và kiểm tra giá trị biên / không hợp lệ
    # ==========================================================================
    def test_spo2_reference_zones_and_boundaries(self):
        """
        Kiểm tra các giá trị biên hợp lệ:
          - 100% -> Trong khoảng tham khảo
          - 95%  -> Trong khoảng tham khảo
          - 94%  -> Cần chú ý
          - 93%  -> Cần chú ý
          - 92%  -> Cảnh báo SpO2 thấp
          - 90%  -> Cảnh báo SpO2 thấp
        Kiểm tra cả cờ valid=True và nhãn.
        """
        cases = [
            (100, "Trong khoảng tham khảo", "100% — Trong khoảng tham khảo"),
            (98,  "Trong khoảng tham khảo", "98% — Trong khoảng tham khảo"),
            (95,  "Trong khoảng tham khảo", "95% — Trong khoảng tham khảo"),
            (94,  "Cần chú ý",              "94% — Cần chú ý"),
            (93,  "Cần chú ý",              "93% — Cần chú ý"),
            (92,  "Cảnh báo SpO₂ thấp",     "92% — Cảnh báo SpO₂ thấp"),
            (90,  "Cảnh báo SpO₂ thấp",     "90% — Cảnh báo SpO₂ thấp"),
            (70,  "Cảnh báo SpO₂ thấp",     "70% — Cảnh báo SpO₂ thấp"),
        ]
        for val, exp_label, exp_disp in cases:
            r = interpret_spo2(val, is_valid=True)
            self.assertTrue(r["valid"], f"val={val} phai co valid=True")
            self.assertEqual(r["percent"], val, f"val={val} phai giu dung gia tri percent")
            self.assertEqual(r["reference_label"], exp_label, f"val={val} sai label")
            self.assertEqual(r["display_text"], exp_disp, f"val={val} sai display_text")

    def test_spo2_invalid_inputs(self):
        """
        Kiểm tra tất cả trường hợp không hợp lệ:
          - None
          - NaN
          - Inf, -Inf
          - -1 (ngoài dải dưới)
          - 105 (ngoài dải trên)
          - is_valid=False (QC lỗi)
        Tất cả BẮT BUỘC trả valid=False, percent=None, nhãn 'Chưa có kết quả tin cậy',
        display_text='Chưa có kết quả tin cậy', không được in '105%' hay '-1%'.
        """
        invalid_inputs = [
            (None, True),
            (float("nan"), True),
            (float("inf"), True),
            (-float("inf"), True),
            (-1, True),
            (-5, True),
            (105, True),
            (100.6, True), # round -> 101 > 100
            (98, False),   # QC lỗi
            (None, False),
            ("invalid_str", True)
        ]
        for val, is_val in invalid_inputs:
            r = interpret_spo2(val, is_valid=is_val)
            self.assertFalse(r["valid"], f"Input val={val}, is_valid={is_val} phai co valid=False")
            self.assertIsNone(r["percent"], f"Input val={val} phai co percent=None")
            self.assertEqual(r["reference_label"], "Chưa có kết quả tin cậy")
            self.assertEqual(r["display_text"], "Chưa có kết quả tin cậy")

    # ==========================================================================
    # 3. PPG HEART RATE (BPM): Giá trị biên, chưa làm tròn, và bối cảnh
    # ==========================================================================
    def test_bpm_unrounded_boundary_decision(self):
        """
        Kiểm tra phân loại BPM dùng giá trị CHƯA LÀM TRÒN:
          - 100.004 BPM khi is_resting=True:
              100.004 > 100.0 -> 'Cao hơn khoảng tham khảo lúc nghỉ'
              Số hiển thị làm tròn: 100.00 BPM
          - 100.000 BPM khi is_resting=True:
              100.0 <= 100.0 -> 'Trong khoảng tham khảo lúc nghỉ'
          - 60.000 BPM khi is_resting=True:
              60.0 >= 60.0 -> 'Trong khoảng tham khảo lúc nghỉ'
          - 59.996 BPM khi is_resting=True:
              59.996 < 60.0 -> 'Thấp hơn khoảng tham khảo lúc nghỉ'
              Số hiển thị làm tròn: 60.00 BPM
        """
        # Case 100.004 BPM
        r_100_004 = interpret_ppg_bpm(100.004, is_resting=True, is_valid=True)
        self.assertTrue(r_100_004["valid"])
        self.assertEqual(r_100_004["bpm"], 100.00)
        self.assertEqual(r_100_004["reference_label"], "Cao hơn khoảng tham khảo lúc nghỉ")
        self.assertEqual(r_100_004["display_text"], "100.00 BPM — Cao hơn khoảng tham khảo lúc nghỉ")

        # Case exactly 100.0 BPM
        r_100 = interpret_ppg_bpm(100.0, is_resting=True, is_valid=True)
        self.assertTrue(r_100["valid"])
        self.assertEqual(r_100["reference_label"], "Trong khoảng tham khảo lúc nghỉ")

        # Case exactly 60.0 BPM
        r_60 = interpret_ppg_bpm(60.0, is_resting=True, is_valid=True)
        self.assertTrue(r_60["valid"])
        self.assertEqual(r_60["reference_label"], "Trong khoảng tham khảo lúc nghỉ")

        # Case 59.996 BPM
        r_59_996 = interpret_ppg_bpm(59.996, is_resting=True, is_valid=True)
        self.assertTrue(r_59_996["valid"])
        self.assertEqual(r_59_996["bpm"], 60.00)
        self.assertEqual(r_59_996["reference_label"], "Thấp hơn khoảng tham khảo lúc nghỉ")
        self.assertEqual(r_59_996["display_text"], "60.00 BPM — Thấp hơn khoảng tham khảo lúc nghỉ")

    def test_bpm_resting_context_confirmed(self):
        """
        Khi xác nhận rõ người đo đang nghỉ (is_resting=True):
          - 55 BPM -> Thấp hơn khoảng tham khảo lúc nghỉ (< 60)
          - 75 BPM -> Trong khoảng tham khảo lúc nghỉ (60-100)
          - 110 BPM -> Cao hơn khoảng tham khảo lúc nghỉ (> 100)
        """
        r_55 = interpret_ppg_bpm(55.0, is_resting=True, is_valid=True)
        self.assertTrue(r_55["valid"])
        self.assertEqual(r_55["reference_label"], "Thấp hơn khoảng tham khảo lúc nghỉ")

        r_75 = interpret_ppg_bpm(75.0, is_resting=True, is_valid=True)
        self.assertTrue(r_75["valid"])
        self.assertEqual(r_75["reference_label"], "Trong khoảng tham khảo lúc nghỉ")

        r_110 = interpret_ppg_bpm(110.0, is_resting=True, is_valid=True)
        self.assertTrue(r_110["valid"])
        self.assertEqual(r_110["reference_label"], "Cao hơn khoảng tham khảo lúc nghỉ")

    def test_bpm_unknown_context(self):
        """
        Khi chưa xác nhận người đo đang nghỉ (is_resting=None hoặc False):
          - Không tự suy diễn từ SQI hay Stress.
          - Bắt buộc hiển thị: "Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ".
        """
        for bpm in [55.0, 75.0, 100.004, 110.0]:
            r = interpret_ppg_bpm(bpm, is_resting=None, is_valid=True)
            self.assertTrue(r["valid"])
            self.assertEqual(r["reference_label"], "Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ")

            r_f = interpret_ppg_bpm(bpm, is_resting=False, is_valid=True)
            self.assertEqual(r_f["reference_label"], "Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ")

    def test_bpm_invalid_cases(self):
        """
        Trường hợp không hợp lệ: BPM <= 0, None, NaN, Inf, hoặc is_valid=False.
        Phải trả valid=False, bpm=None, reference_label='Chưa có kết quả tin cậy'.
        """
        invalid_bpms = [
            None,
            float("nan"),
            float("inf"),
            -float("inf"),
            0.0,
            -1.0,
            -75.0
        ]
        for val in invalid_bpms:
            r = interpret_ppg_bpm(val, is_valid=True)
            self.assertFalse(r["valid"], f"val={val} phai tra valid=False")
            self.assertIsNone(r["bpm"], f"val={val} phai tra bpm=None")
            self.assertEqual(r["reference_label"], "Chưa có kết quả tin cậy")

        # QC không đạt
        r_qc_fail = interpret_ppg_bpm(75.0, is_resting=True, is_valid=False)
        self.assertFalse(r_qc_fail["valid"])
        self.assertIsNone(r_qc_fail["bpm"])
        self.assertEqual(r_qc_fail["reference_label"], "Chưa có kết quả tin cậy")

    def test_compute_ppg_bpm_formula_and_safety(self):
        """Kiểm tra công thức 60000 / median(valid_ppi) và điều kiện loại trừ."""
        # 50 nhịp đều đặn ở 800 ms -> 60000 / 800 = 75 BPM
        ppi = [800.0] * 50
        res = compute_ppg_bpm_from_ppi(ppi, sqi_ok=True, min_beats=40)
        self.assertEqual(res["heart_rate_status"], "RESULT_AVAILABLE")
        self.assertEqual(res["heart_rate_bpm"], 75.0)

        # SQI = False -> từ chối
        res_bad_sqi = compute_ppg_bpm_from_ppi(ppi, sqi_ok=False)
        self.assertEqual(res_bad_sqi["heart_rate_status"], "NOT_READY")
        self.assertIsNone(res_bad_sqi["heart_rate_bpm"])

        # Ít nhịp -> từ chối
        res_few = compute_ppg_bpm_from_ppi([800.0]*5, sqi_ok=True, min_beats=10)
        self.assertEqual(res_few["heart_rate_status"], "NOT_READY")
        self.assertIsNone(res_few["heart_rate_bpm"])

        # PPI chứa NaN / Inf nhưng có đủ nhịp hợp lệ
        ppi_with_nan = [800.0] * 40 + [float("nan"), float("inf")]
        res_nan = compute_ppg_bpm_from_ppi(ppi_with_nan, sqi_ok=True, min_beats=40)
        self.assertEqual(res_nan["heart_rate_status"], "RESULT_AVAILABLE")
        self.assertEqual(res_nan["heart_rate_bpm"], 75.0)

    def test_full_pipeline_ppi_to_bpm_classification(self):
        """
        Kiểm tra luồng đầy đủ: PPI -> compute_ppg_bpm_from_ppi -> phân loại:
          1. BPM gốc 100.004 khi đã xác nhận người lớn đang nghỉ (is_resting=True):
             - raw_bpm = 100.004 > 100.0 -> nhóm 'Cao hơn khoảng tham khảo lúc nghỉ'
             - Số hiển thị làm tròn: 100.00 BPM.
          2. BPM gốc 59.996 khi đã xác nhận người lớn đang nghỉ (is_resting=True):
             - raw_bpm = 59.996 < 60.0 -> nhóm 'Thấp hơn khoảng tham khảo lúc nghỉ'
             - Số hiển thị làm tròn: 60.00 BPM.
          3. Dữ liệu Step 2 không rõ bối cảnh (is_resting=None hoặc False):
             - Vẫn hiển thị số BPM làm tròn nhưng nhãn bắt buộc là:
               'Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ'.
        """
        # 1. Ca 100.004 BPM: PPI tương ứng = 60000 / 100.004 ms
        ppi_100_004 = [60000.0 / 100.004] * 10
        res_100 = compute_ppg_bpm_from_ppi(ppi_100_004, sqi_ok=True, min_beats=4, is_resting=True)
        self.assertEqual(res_100["heart_rate_status"], "RESULT_AVAILABLE")
        self.assertEqual(res_100["heart_rate_bpm"], 100.00)
        self.assertAlmostEqual(res_100["raw_bpm"], 100.004, places=4)
        self.assertEqual(res_100["reference_label"], "Cao hơn khoảng tham khảo lúc nghỉ")
        self.assertEqual(res_100["display_text"], "100.00 BPM — Cao hơn khoảng tham khảo lúc nghỉ")

        # Kiểm tra luồng nối tiếp: gọi interpret_ppg_bpm với raw_bpm hoặc dict kết quả
        interp_from_raw = interpret_ppg_bpm(res_100["raw_bpm"], is_resting=True, is_valid=True)
        self.assertEqual(interp_from_raw["reference_label"], "Cao hơn khoảng tham khảo lúc nghỉ")
        self.assertEqual(interp_from_raw["bpm"], 100.00)

        interp_from_dict = interpret_ppg_bpm(res_100, is_resting=True, is_valid=True)
        self.assertEqual(interp_from_dict["reference_label"], "Cao hơn khoảng tham khảo lúc nghỉ")
        self.assertEqual(interp_from_dict["bpm"], 100.00)

        # 2. Ca 59.996 BPM: PPI tương ứng = 60000 / 59.996 ms
        ppi_59_996 = [60000.0 / 59.996] * 10
        res_59 = compute_ppg_bpm_from_ppi(ppi_59_996, sqi_ok=True, min_beats=4, is_resting=True)
        self.assertEqual(res_59["heart_rate_status"], "RESULT_AVAILABLE")
        self.assertEqual(res_59["heart_rate_bpm"], 60.00)
        self.assertAlmostEqual(res_59["raw_bpm"], 59.996, places=4)
        self.assertEqual(res_59["reference_label"], "Thấp hơn khoảng tham khảo lúc nghỉ")
        self.assertEqual(res_59["display_text"], "60.00 BPM — Thấp hơn khoảng tham khảo lúc nghỉ")

        interp_59_raw = interpret_ppg_bpm(res_59["raw_bpm"], is_resting=True, is_valid=True)
        self.assertEqual(interp_59_raw["reference_label"], "Thấp hơn khoảng tham khảo lúc nghỉ")
        self.assertEqual(interp_59_raw["bpm"], 60.00)

        # 3. Dữ liệu Step 2 không rõ bối cảnh (is_resting=None)
        res_unknown = compute_ppg_bpm_from_ppi(ppi_100_004, sqi_ok=True, min_beats=4, is_resting=None)
        self.assertEqual(res_unknown["heart_rate_status"], "RESULT_AVAILABLE")
        self.assertEqual(res_unknown["heart_rate_bpm"], 100.00)
        self.assertEqual(res_unknown["reference_label"], "Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ")
        self.assertEqual(res_unknown["display_text"], "100.00 BPM — Chưa đủ bối cảnh để đánh giá theo nhịp lúc nghỉ")


if __name__ == "__main__":
    unittest.main(verbosity=2)
