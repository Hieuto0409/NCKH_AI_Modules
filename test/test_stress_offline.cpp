/**
 * test_stress_offline.cpp
 * -----------------------
 * Đối chiếu độc lập đầu ra suy luận C++ (stress_ppg::infer()) với giá trị
 * tham chiếu tính bằng Python từ file model.json trên 6 fixture.
 *
 * QUY TẮC ĐỐI CHIẾU:
 *   - ref_prob trong từng fixture được sinh độc lập bởi Python từ model.json
 *     với độ chính xác double (64-bit float).
 *   - Bài test KHÔNG dùng kMean, kScale, kCoef, kIntercept trong header C++
 *     để tự tính lại xác suất tham chiếu (tránh so sánh vòng tròn / circular check).
 *   - Đầu ra C++ stress_ppg::infer() được so trực tiếp với ref_prob (dung sai 1e-9).
 *   - Nhãn C++ được so với deployment_pred (nhãn dự đoán của deployment model).
 *   - Nếu bất kỳ fixture nào sai xác suất (> 1e-9) hoặc sai nhãn, trả exit code 1.
 *
 * PHẠM VI KIỂM CHỨNG:
 *   - Đúng 6/6 fixture được đối chiếu Python–C++ trực tiếp tại đây.
 *   - Không gọi kết quả này là "60/60 Python–C++". Toàn bộ 60 cửa sổ được chạy
 *     và đánh giá độc lập bởi script Python test/verify_stress.py.
 *
 * KẾT QUẢ TRÊN TẬP TEST WESAD (S13 + S16):
 *   - Evaluation model (fit train only): 56/60 = 93.33% (kết quả chính thức báo cáo NCKH)
 *   - Deployment model (fit train+val) : 58/60 = 96.67% (hệ số nhúng trong model.json/header)
 *   - 2 cửa sổ False Positive của deployment model: S13_baseline_t_267.9 và S13_baseline_t_297.9.
 *
 * Build & Run (MinGW):
 *   g++ -std=c++11 -I lib/Stress_PPG_60s_model test/test_stress_offline.cpp -o .pio/test_stress_offline.exe
 *   .pio/test_stress_offline.exe
 */

#include <cstdio>
#include <cstring>
#include <cmath>
#include "stress_ppg_model.h"

// Dung sai so sánh xác suất Python vs C++
static const double kProbTol = 1e-9;

// ── Cấu trúc fixture đối chiếu độc lập ───────────────────────────────────────
struct Fixture {
    const char* window_id;
    const char* true_label;       // Nhãn thực tế trong WESAD test set
    const char* deployment_pred;  // Nhãn Python dự đoán từ model.json
    double ref_prob;              // Xác suất tham chiếu tính từ Python (model.json, float64)
    double features[stress_ppg::kFeatureCount];
};

// Thứ tự 14 đặc trưng:
//   mean_hr_bpm, std_hr_bpm, min_hr_bpm, max_hr_bpm,
//   mean_pp_ms, median_pp_ms, sdnn_ms, rmssd_ms,
//   sdsd_ms, pnn20_pct, pnn50_pct, cvnn,
//   beat_count, valid_rr_ratio
// Giá trị ref_prob lấy nguyên bản từ Python infer() sử dụng model.json:
static const Fixture kFixtures[] = {
    // ── Fixture 1: S13 baseline (False Positive của deployment model) ─────────
    {
        "S13_baseline_t_267.9",
        /*true_label=*/"baseline",
        /*deployment_pred=*/"stress",
        /*ref_prob=*/0.648799505354633710,
        {90.09228867656934, 9.035973070988689, 72.45283018867924, 123.87096774193549,
         671.875, 671.875, 60.48228723909283, 87.13422523306909,
         87.85392657484061, 68.85245901639344, 40.98360655737705, 0.09002014844888234,
         92.0, 0.6813186813186813}
    },
    // ── Fixture 2: S13 baseline (False Positive của deployment model) ─────────
    {
        "S13_baseline_t_297.9",
        /*true_label=*/"baseline",
        /*deployment_pred=*/"stress",
        /*ref_prob=*/0.789563413268324021,
        {86.58417498494916, 11.009403664794496, 68.57142857142857, 120.0,
         703.6546610169491, 703.125, 86.60672525483149, 138.1943917985421,
         139.40108775934678, 81.03448275862068, 60.3448275862069, 0.12308129264668562,
         89.0, 0.6704545454545454}
    },
    // ── Fixture 3: S13 baseline (True Negative) ──────────────────────────────
    {
        "S13_baseline_t_387.9",
        /*true_label=*/"baseline",
        /*deployment_pred=*/"baseline",
        /*ref_prob=*/0.189585595098368670,
        {88.11953963036558, 8.277801967523317, 68.57142857142857, 116.36363636363636,
         687.2632575757576, 671.875, 69.79604259495842, 79.10443741807896,
         79.7141558452914, 44.61538461538462, 23.076923076923077, 0.10155648774409382,
         81.0, 0.825}
    },
    // ── Fixture 4: S13 stress (True Positive) ────────────────────────────────
    {
        "S13_stress_t_3260.9",
        /*true_label=*/"stress",
        /*deployment_pred=*/"stress",
        /*ref_prob=*/0.997457661378017613,
        {110.80989507545938, 20.574140562029303, 80.0, 147.69230769230768,
         559.375, 562.5, 99.56373320812646, 147.15309050268542,
         148.3756051937551, 93.22033898305084, 79.66101694915254, 0.17799103143352218,
         93.0, 0.6521739130434783}
    },
    // ── Fixture 5: S16 baseline (True Negative) ──────────────────────────────
    {
        "S16_baseline_t_128.4",
        /*true_label=*/"baseline",
        /*deployment_pred=*/"baseline",
        /*ref_prob=*/0.135514937572992544,
        {66.07963127789854, 9.074618597175785, 54.857142857142854, 91.42857142857143,
         923.125, 953.125, 113.59129150204271, 149.83648613248582,
         151.33986185625687, 83.6734693877551, 65.3061224489796, 0.12305082356348566,
         70.0, 0.7246376811594203}
    },
    // ── Fixture 6: S16 stress (True Positive) ────────────────────────────────
    {
        "S16_stress_t_2018.4",
        /*true_label=*/"stress",
        /*deployment_pred=*/"stress",
        /*ref_prob=*/0.993083082776053172,
        {135.8409094935241, 11.952106145992367, 109.71428571428571, 174.54545454545453,
         444.96527777777777, 437.5, 37.92969651514446, 56.288025536700296,
         56.606665075155206, 67.41573033707866, 28.08988764044944, 0.08524192427906051,
         113.0, 0.8035714285714286}
    },
};
static const size_t kNFixtures = sizeof(kFixtures) / sizeof(kFixtures[0]);

int main() {
    printf("===================================================================\n");
    printf("=== test_stress_offline.cpp: Doi chieu doc lap Python - C++ ===\n");
    printf("===================================================================\n");
    printf("Nguon xac suat tham chieu: Python doc lap tinh tu model.json\n");
    printf("Khong dung he so trong header C++ de tinh lai ref_prob.\n");
    printf("Dung sai cho phep (probability tolerance): %.0e\n", kProbTol);
    printf("So luong fixture doi chieu: %zu fixture\n", kNFixtures);
    printf("-------------------------------------------------------------------\n\n");

    int pass_count = 0;
    int fail_count = 0;

    for (size_t i = 0; i < kNFixtures; ++i) {
        const Fixture& fx = kFixtures[i];

        // 1. Chạy hàm suy luận C++ từ stress_ppg_model.h
        auto r = stress_ppg::infer(fx.features, stress_ppg::kFeatureCount);
        const char* cpp_label = r.stress ? "stress" : "baseline";

        // 2. So sánh trực tiếp với giá trị tham chiếu Python (fx.ref_prob)
        double prob_diff = std::fabs(r.stress_probability - fx.ref_prob);
        bool prob_ok = (prob_diff <= kProbTol);

        // 3. So sánh nhãn C++ với nhãn tham chiếu của deployment model
        bool label_ok = (strcmp(cpp_label, fx.deployment_pred) == 0);

        // Điều kiện PASS: tính toán hợp lệ, sai số xác suất <= 1e-9, đúng nhãn
        bool pass = (r.valid && prob_ok && label_ok);

        printf("[%s]\n", fx.window_id);
        printf("  true_label=%s | deployment_pred=%s | cpp_label=%s\n",
               fx.true_label, fx.deployment_pred, cpp_label);
        printf("  prob_cpp=%.15f\n", r.stress_probability);
        printf("  prob_py =%.15f\n", fx.ref_prob);
        printf("  diff    =%.2e (%s)\n", prob_diff, prob_ok ? "MATCH" : "MISMATCH");

        if (strcmp(fx.deployment_pred, fx.true_label) != 0) {
            printf("  [Ghi chu: False Positive cua deployment model tren cua so nay]\n");
        }

        if (pass) {
            printf("  => PASS (C++ khop hoan toan Python model.json)\n\n");
            pass_count++;
        } else {
            printf("  => FAIL (Sai lech tinh toan C++ so voi Python model.json!)\n\n");
            fail_count++;
        }
    }

    printf("-------------------------------------------------------------------\n");
    printf("KET QUA DOI CHIEU PYTHON - C++: %d/%zu PASS, %d FAIL\n",
           pass_count, kNFixtures, fail_count);
    printf("Luu y pham vi: Day la %d/%zu fixture duoc doi chieu Python-C++.\n",
           pass_count, kNFixtures);
    printf("KHONG goi day la 60/60 Python-C++.\n");
    printf("Toan bo 60 cua so WESAD (S13+S16) duoc kiem tra bang Python trong\n");
    printf("test/verify_stress.py:\n");
    printf("  - Deployment model: 58/60 (96.67%%)\n");
    printf("  - Evaluation model: 56/60 (93.33%% - so lieu bao cao chinh thuc)\n");
    printf("===================================================================\n");

    return (fail_count == 0) ? 0 : 1;
}
