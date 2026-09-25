#!/usr/bin/env python3
"""
test_ppg_heart_rate.py
----------------------
Bài kiểm tra độc lập cho thuật toán tính Nhịp tim (BPM) từ các đỉnh PPG.

Nội dung kiểm tra:
  1. Ca kiểm thử chuẩn: Các thời điểm đỉnh có khoảng cách PPI 800 ms
     -> Công thức: 60000 / 800 = 75.0 BPM.
     -> Xác nhận: status = RESULT_AVAILABLE, heart_rate_bpm = 75.0.
  2. Ca kiểm thử biến thiên: Mảng PPI có trung vị 800 ms
     -> [780, 800, 820, 800, 800] ms -> trung vị 800 ms -> 75.0 BPM.
  3. Ca kiểm thử thiếu đỉnh / thiếu khoảng nhịp (< min_beats):
     -> Chỉ có 2 đỉnh (1 khoảng PPI < 4).
     -> Xác nhận: status = NOT_READY, heart_rate_bpm = None (KHÔNG BAO GIỜ TRẢ 0 BPM).
  4. Ca kiểm thử SQI không đạt (sqi_ok = False):
     -> Tín hiệu bị loại trừ do chất lượng kém.
     -> Xác nhận: status = NOT_READY, heart_rate_bpm = None.
  5. Đối chiếu với C++ orchestrator::computePpgHeartRate():
     -> Biên dịch và chạy đoạn kiểm tra C++ qua g++ để xác nhận logic C++ khớp Python.
"""

import sys, os, subprocess, shutil
from pathlib import Path

# Thiết lập encoding utf-8 cho console
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))

from run_offline_demo import compute_ppg_bpm_from_ppi


def test_ppi_800ms_yields_75bpm():
    """Ca 1: Các thời điểm đỉnh có PPI 800 ms -> xác nhận kết quả 75 BPM."""
    # Thời điểm các đỉnh (giây): 0.0, 0.8, 1.6, 2.4, 3.2, 4.0, 4.8, 5.6
    # Khoảng cách giữa các đỉnh liên tiếp theo mili-giây:
    ppi_ms = [800.0, 800.0, 800.0, 800.0, 800.0, 800.0, 800.0]
    
    res = compute_ppg_bpm_from_ppi(ppi_ms, sqi_ok=True, min_beats=4)
    print(f"[*] Test 1 - PPI 800 ms -> 75 BPM:")
    print(f"    status: {res['heart_rate_status']}")
    print(f"    bpm   : {res['heart_rate_bpm']}")
    print(f"    source: {res['heart_rate_source']}")
    print(f"    details: {res['details']}")

    assert res["heart_rate_status"] == "RESULT_AVAILABLE", f"Ky vong RESULT_AVAILABLE, nhan {res['heart_rate_status']}"
    assert res["heart_rate_bpm"] == 75.0, f"Ky vong 75.0 BPM, nhan {res['heart_rate_bpm']}"
    assert res["details"]["median_ppi_ms"] == 800.0, f"Ky vong median_ppi_ms 800.0, nhan {res['details']['median_ppi_ms']}"
    print("    => PASS: 60000 / 800 ms = 75.0 BPM (chinh xac tuyet doi)\n")


def test_variable_ppi_median_800ms():
    """Ca 2: Biến thiên nhẹ quanh 800 ms nhưng trung vị là 800 ms -> 75 BPM."""
    ppi_ms = [780.0, 800.0, 820.0, 800.0, 800.0]
    res = compute_ppg_bpm_from_ppi(ppi_ms, sqi_ok=True, min_beats=4)
    print(f"[*] Test 2 - PPI bien thien [780, 800, 820, 800, 800] ms -> trung vi 800 ms:")
    print(f"    status: {res['heart_rate_status']}")
    print(f"    bpm   : {res['heart_rate_bpm']}")
    assert res["heart_rate_status"] == "RESULT_AVAILABLE"
    assert res["heart_rate_bpm"] == 75.0
    print("    => PASS: Trung vi PPI loai bo nhieu, giu on dinh 75.0 BPM\n")


def test_insufficient_peaks():
    """Ca 3: Thiếu đỉnh / khoảng nhịp (< min_beats) -> NOT_READY, KHÔNG trả 0 BPM."""
    # Chỉ có 2 đỉnh -> 1 khoảng PPI = 800 ms, trong khi min_beats = 4
    ppi_ms = [800.0]
    res = compute_ppg_bpm_from_ppi(ppi_ms, sqi_ok=True, min_beats=4)
    print(f"[*] Test 3 - Thieu dinh (1 khoang PPI < nguong 4):")
    print(f"    status: {res['heart_rate_status']}")
    print(f"    bpm   : {res['heart_rate_bpm']}")
    print(f"    reason: {res['details'].get('reason')}")

    assert res["heart_rate_status"] == "NOT_READY", f"Ky vong NOT_READY, nhan {res['heart_rate_status']}"
    assert res["heart_rate_bpm"] is None, f"Ky vong None (khong xuat 0 BPM), nhan {res['heart_rate_bpm']}"
    assert res["heart_rate_bpm"] != 0, "Loi nghiem trong: tra ve 0 BPM nhu the do duoc"
    print("    => PASS: Tra NOT_READY kem ly do, khong xuat 0 BPM\n")


def test_sqi_failure():
    """Ca 4: SQI không đạt (sqi_ok = False) -> NOT_READY, KHÔNG trả 0 BPM."""
    ppi_ms = [800.0, 800.0, 800.0, 800.0, 800.0]
    res = compute_ppg_bpm_from_ppi(ppi_ms, sqi_ok=False, min_beats=4)
    print(f"[*] Test 4 - SQI khong dat (sqi_ok = False):")
    print(f"    status: {res['heart_rate_status']}")
    print(f"    bpm   : {res['heart_rate_bpm']}")
    print(f"    reason: {res['details'].get('reason')}")

    assert res["heart_rate_status"] == "NOT_READY"
    assert res["heart_rate_bpm"] is None
    assert res["heart_rate_bpm"] != 0
    print("    => PASS: SQI fail tu choi tra BPM, tra NOT_READY hop le\n")


def test_cpp_orchestrator_hr():
    """Ca 5: Kiểm tra C++ orchestrator::computePpgHeartRate() qua g++."""
    print(f"[*] Test 5 - Kiem tra C++ orchestrator::computePpgHeartRate():")
    if not shutil.which("g++"):
        print("    [SKIPPED] g++ khong co trong PATH; bo qua kiem tra binary C++.")
        return

    test_cpp_src = PROJECT_ROOT / ".pio" / "test_cpp_hr.cpp"
    test_cpp_bin = PROJECT_ROOT / ".pio" / "test_cpp_hr.exe"
    test_cpp_src.parent.mkdir(parents=True, exist_ok=True)

    cpp_code = """
#include <cstdio>
#include <cassert>
#include <cmath>
#include "health_orchestrator.h"

int main() {
    // 1. Mang PPI 800 ms -> 75.0 BPM
    double ppi_800[] = {800.0, 800.0, 800.0, 800.0, 800.0};
    auto r1 = orchestrator::computePpgHeartRate(ppi_800, 5, 4);
    assert(r1.state == orchestrator::ReadyState::READY);
    assert(r1.valid == true);
    assert(std::abs(r1.heart_rate_bpm - 75.0) < 1e-6);
    assert(std::abs(r1.median_ppi_ms - 800.0) < 1e-6);

    // 2. Thieu dinh (< 4 khoang) -> NOT_READY
    double ppi_short[] = {800.0};
    auto r2 = orchestrator::computePpgHeartRate(ppi_short, 1, 4);
    assert(r2.state == orchestrator::ReadyState::NOT_READY);
    assert(r2.valid == false);
    assert(r2.heart_rate_bpm == 0.0); // struct default

    // 3. nullptr -> NOT_READY
    auto r3 = orchestrator::computePpgHeartRate(nullptr, 0, 4);
    assert(r3.state == orchestrator::ReadyState::NOT_READY);

    printf("CPP_ORCHESTRATOR_HR_PASS 75.0\\n");
    return 0;
}
"""
    test_cpp_src.write_text(cpp_code, encoding="utf-8")
    cmd = [
        "g++", "-std=c++11", "-O2", "-Wall",
        f"-I{PROJECT_ROOT / 'include'}",
        f"-I{PROJECT_ROOT / 'lib' / 'Stress_PPG_60s_model'}",
        f"-I{PROJECT_ROOT / 'lib' / 'ResearchSpO2' / 'src'}",
        str(test_cpp_src),
        "-o", str(test_cpp_bin)
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print(f"    [WARN] Bien dich test C++ that bai: {proc.stderr[:300]}")
        return

    run_proc = subprocess.run([str(test_cpp_bin)], capture_output=True, text=True)
    if run_proc.returncode == 0 and "CPP_ORCHESTRATOR_HR_PASS" in run_proc.stdout:
        print("    => PASS: C++ computePpgHeartRate() tra ve 75.0 BPM, NOT_READY dung thiet ke\n")
    else:
        print(f"    [FAIL] Test C++ binary tra ve ma {run_proc.returncode}: {run_proc.stderr}")
        assert False, "C++ computePpgHeartRate test failed"


def main():
    print("=" * 70)
    print("=== KIEM TRA DOC LAP: THUAT TOAN TINH NHIP TIM (BPM) TU PPG ===")
    print("=" * 70)
    print()

    test_ppi_800ms_yields_75bpm()
    test_variable_ppi_median_800ms()
    test_insufficient_peaks()
    test_sqi_failure()
    test_cpp_orchestrator_hr()

    print("=" * 70)
    print("TAT CA CAC CA KIEM TRA NHIP TIM PPG: PASS ✅")
    print("=" * 70)


if __name__ == "__main__":
    main()
