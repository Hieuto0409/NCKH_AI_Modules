/**
 * @file main.cpp
 * @brief ESP32-S3 main — ba nhánh giám sát sức khỏe độc lập.
 *
 * Ba nhánh (mỗi nhánh có trạng thái riêng, không dùng chung buffer):
 *   1. Stress PPG   : 14 đặc trưng HRV (double[14]) → stress/baseline
 *   2. ECG AF/non-AF: 9 đặc trưng ECG-HRV (float[9]) → AF/non-AF
 *   3. SpO₂         : Raw IR/RED @ 100 Hz → ước tính SpO₂
 *
 * CHẾ ĐỘ BUILD
 * ─────────────
 *   Môi trường mặc định (esp32-s3-devkitc-1):
 *     Không định nghĩa TEST_OFFLINE_FIXTURE.
 *     Boot → in trạng thái NOT_READY cho ba nhánh → idle loop.
 *     Không tạo dữ liệu giả. Không in kết quả như thể đo được.
 *
 *   Môi trường test_fixture (trong platformio.ini):
 *     Định nghĩa -DTEST_OFFLINE_FIXTURE=1.
 *     In rõ "OFFLINE TEST — NOT A SENSOR MEASUREMENT" lên Serial.
 *     Chạy fixture cố định từ CSV/log để kiểm tra API tích hợp.
 *     Kết quả fixture KHÔNG được dùng trong báo cáo như đo thật.
 *
 * ⚠ DISCLAIMER (giữ nguyên, không xóa):
 *   Stress : evaluation model 93.33% trên WESAD test (S13+S16), NOT trên MAX30102.
 *   ECG    : AF/non-AF, NOT chẩn đoán mọi rối loạn nhịp, NOT y tế.
 *   SpO₂   : Ước tính tỷ số AC/DC (Maxim/SparkFun), NOT kiểm chứng lâm sàng.
 *
 * Điểm nối chờ module nhóm trưởng:
 *   - double stressFeatures[14] : HRV từ cửa sổ PPG 60 s (sau QC)
 *   - float  ecgFeatures[9]     : từ R-peak detector (đơn vị cần xác minh)
 *   - uint32_t ir, red @ 100 Hz : raw ADC từ MAX30102 FIFO (sampleAverage=1)
 *   - bool poorSignal            : flag tức thời, KHÔNG phải SQI 5 s PPG
 */

#include <Arduino.h>
#include "health_orchestrator.h"

// ── Forward declarations ──────────────────────────────────────────────────────
static void printStressResult(const orchestrator::StressResult& r);
static void printEcgResult(const orchestrator::EcgResult& r);
static void printSpo2Result(const orchestrator::Spo2Result& r);

// ══════════════════════════════════════════════════════════════════════════════
// OFFLINE TEST FIXTURES
// Được kích hoạt CHỈ khi build với môi trường test_fixture
// (-DTEST_OFFLINE_FIXTURE=1 trong platformio.ini).
// Giá trị này là dữ liệu kiểm thử phần mềm, KHÔNG phải đo từ cảm biến.
// ══════════════════════════════════════════════════════════════════════════════
#ifdef TEST_OFFLINE_FIXTURE

// ── Fixture A: Stress PPG ─────────────────────────────────────────────────────
// Nguồn: dòng 2 của stress_hrv_features_60s_test.csv (S13, nhãn thật: baseline)
// Nhãn thật   : baseline
// Dự đoán deployment_model: stress  ← đây là false positive của model,
//   không phải lỗi C++; xác suất p≈0.649 > ngưỡng 0.5
// Dự đoán evaluation_model: không biết từ code này (hệ số khác)
// Thứ tự feature: mean_hr_bpm, std_hr_bpm, min_hr_bpm, max_hr_bpm,
//                 mean_pp_ms, median_pp_ms, sdnn_ms, rmssd_ms,
//                 sdsd_ms, pnn20_pct, pnn50_pct, cvnn,
//                 beat_count, valid_rr_ratio
static const double kStressFixture[stress_ppg::kFeatureCount] = {
    90.09228867656934,    // mean_hr_bpm
     9.035973070988689,   // std_hr_bpm
    72.45283018867924,    // min_hr_bpm
   123.87096774193549,    // max_hr_bpm
   671.875,               // mean_pp_ms
   671.875,               // median_pp_ms
    60.48228723909283,    // sdnn_ms
    87.13422523306909,    // rmssd_ms
    87.85392657484061,    // sdsd_ms
    68.85245901639344,    // pnn20_pct
    40.98360655737705,    // pnn50_pct
     0.09002014844888234, // cvnn
    92.0,                 // beat_count
     0.6813186813186813   // valid_rr_ratio
};
// Kết quả được: deployment_model → stress (FP); evaluation_model → không biết từ đây.

// ── Fixture B: ECG AF/non-AF ──────────────────────────────────────────────────
// Nguồn: giá trị plausible dùng để kiểm tra API biên dịch; không có nhãn xác minh.
// Không thể báo accuracy từ fixture này.
// Đơn vị 9 đặc trưng CHƯA xác minh với dữ liệu huấn luyện EI; không gán đơn vị.
// Thứ tự: mean_rr, median_rr, sdnn, rmssd, pnn50, cv_rr, iqr_rr, min_rr, max_rr
static const float kEcgFixture[9] = {
    0.82f,   // mean_rr  — đơn vị chưa xác minh
    0.81f,   // median_rr
    0.04f,   // sdnn
    0.03f,   // rmssd
    20.0f,   // pnn50
    0.05f,   // cv_rr
    0.05f,   // iqr_rr
    0.72f,   // min_rr
    0.95f    // max_rr
};

// ── Fixture C: SpO₂ — tín hiệu hình sin tổng hợp ─────────────────────────────
// Nguồn: cùng pattern với tests/test.cpp của module ResearchSpO2.
// KHÔNG đại diện cho đo người thật.
static void fillSpo2Fixture() {
    orchestrator::resetSpo2();
    for (int rep = 0; rep < 4; rep++) {   // 4 × 100 = 400 cặp raw
        for (int i = 0; i < 100; i++) {
            uint32_t ir  = (uint32_t)(60000 + 1200 * sin(i * 2.0 * 3.14159265358979 / 20.0));
            uint32_t red = (uint32_t)(40000 +  500 * sin(i * 2.0 * 3.14159265358979 / 20.0));
            orchestrator::pushSpo2(ir, red);
        }
    }
}

#endif // TEST_OFFLINE_FIXTURE

// ══════════════════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("=== NCKH Health Monitor Boot ==="));

#ifdef TEST_OFFLINE_FIXTURE
    // ─── Chế độ fixture offline ───────────────────────────────────────────────
    Serial.println(F(""));
    Serial.println(F("*** OFFLINE TEST — NOT A SENSOR MEASUREMENT ***"));
    Serial.println(F("    Gia tri hardcoded tu CSV/log de kiem tra API."));
    Serial.println(F("    Khong dung ket qua nay trong bao cao nhu do thuc."));
    Serial.println(F(""));

    // Branch 1: Stress PPG
    Serial.println(F("--- Branch 1: Stress PPG (offline fixture) ---"));
    Serial.println(F("  Fixture: S13_baseline_t_267.9  [nhan that: baseline]"));
    auto stressResult = orchestrator::runStress(kStressFixture, stress_ppg::kFeatureCount);
    printStressResult(stressResult);

    // Branch 2: ECG AF/non-AF
    Serial.println(F("--- Branch 2: ECG AF/non-AF (offline fixture) ---"));
    Serial.println(F("  Fixture: gia tri tu dong API test; khong co nhan xac minh."));
    auto ecgResult = orchestrator::runEcg(kEcgFixture, 9);
    printEcgResult(ecgResult);

    // Branch 3: SpO2
    Serial.println(F("--- Branch 3: SpO2 (offline fixture, tin hieu tong hop) ---"));
    fillSpo2Fixture();
    auto spo2Result = orchestrator::evaluateSpo2(true); // externalQualityOk=true cho fixture sach
    printSpo2Result(spo2Result);

    Serial.println(F(""));
    Serial.println(F("*** OFFLINE TEST COMPLETE — Entering idle loop ***"));
    Serial.println(F("    Cho module xu ly tin hieu nho truong de dung pipeline thuc."));

#else
    // ─── Chế độ mặc định: hiển thị NOT_READY, không tạo dữ liệu giả ─────────
    orchestrator::resetSpo2();
    Serial.println(F(""));
    Serial.println(F("Trang thai ba nhanh (chua co dau vao):"));

    // Stress: không có feature → NOT_READY
    auto sr = orchestrator::runStress(nullptr, 0);
    Serial.printf("  Stress PPG : %s\n",
        sr.state == orchestrator::ReadyState::NOT_READY ? "NOT_READY" : "READY");

    // ECG: không có feature → NOT_READY
    auto er = orchestrator::runEcg(nullptr, 0);
    Serial.printf("  ECG AF/non-AF: %s\n",
        er.state == orchestrator::ReadyState::NOT_READY ? "NOT_READY" : "READY");

    // SpO2: mới reset → NOT_READY
    auto sp = orchestrator::evaluateSpo2(true);
    Serial.printf("  SpO2       : %s\n",
        sp.state == orchestrator::ReadyState::NOT_READY ? "NOT_READY" : "READY");

    Serial.println(F(""));
    Serial.println(F("Cho dau vao tu module xu ly tin hieu nho truong."));
    Serial.println(F("Xem src/main.cpp loop() de biet diem noi."));
#endif
}

// ══════════════════════════════════════════════════════════════════════════════
void loop() {
#ifdef TEST_OFFLINE_FIXTURE
    delay(10000); // fixture đã chạy trong setup(); không làm gì thêm
#else
    // ─── Điểm nối pipeline thực ────────────────────────────────────────────
    // Chưa kết nối module nhóm trưởng. Các TODO dưới đây mô tả giao diện.
    //
    // TODO (SpO₂ — 100 Hz push từ MAX30102 FIFO):
    //   uint32_t ir, red;
    //   bool poorSignal;
    //   // ... đọc từ sensor FIFO tại 100 Hz thực, sampleAverage=1 ...
    //   orchestrator::pushSpo2(ir, red);
    //   // Evaluate mỗi giây sau 4 s đầu:
    //   auto spo2 = orchestrator::evaluateSpo2(!poorSignal);
    //   if (spo2.state == orchestrator::ReadyState::READY && spo2.detail.valid())
    //       Serial.printf("[SpO2] %d%% (status=%s)\n",
    //           spo2.detail.percent, research_spo2::statusText(spo2.detail.status));
    //
    // TODO (Stress PPG — mỗi cửa sổ 60 s, sau QC nhịp):
    //   double stressFeatures[14]; // do module nhóm trưởng tính
    //   auto stress = orchestrator::runStress(stressFeatures, 14);
    //   if (stress.state == orchestrator::ReadyState::READY && stress.valid)
    //       Serial.printf("[Stress] %s (p=%.3f)\n",
    //           stress.stress ? "STRESS" : "BASELINE", stress.probability);
    //
    // TODO (ECG AF/non-AF — khi có 9 feature từ R-peak detector):
    //   float ecgFeatures[9]; // do module nhóm trưởng tính; đơn vị cần xác minh
    //   auto ecg = orchestrator::runEcg(ecgFeatures, 9);
    //   if (ecg.state == orchestrator::ReadyState::READY && ecg.valid)
    //       Serial.printf("[ECG] %s (P_AF=%.3f)\n",
    //           ecg.is_af ? "AF" : "non-AF", ecg.prob_af);
    //
    delay(1000);
#endif
}

// ─── Print helpers ────────────────────────────────────────────────────────────
static void printStressResult(const orchestrator::StressResult& r) {
    if (r.state == orchestrator::ReadyState::NOT_READY) {
        Serial.println(F("  Trang thai: NOT_READY (chua co 14 feature)"));
        return;
    }
    if (!r.valid) {
        Serial.println(F("  Trang thai: READY nhung model tra ket qua khong hop le"));
        return;
    }
    const char* pred = r.stress ? "stress" : "baseline";
    Serial.printf("  READY | prob=%.4f | deployment_pred=%s", r.probability, pred);
    if (r.heart_rate_bpm > 0.0) {
        Serial.printf(" | HR(PPG)=%.1f bpm", r.heart_rate_bpm);
    }
    Serial.println();
    Serial.println(F("  [FIXTURE] Nhan that: baseline. Deployment model du doan sai (FP)."));
    Serial.println(F("  [NOTE] Day la ket qua kiem tra API, khong phai do nguoi thuc."));
    Serial.println(F("  [DISCLAIMER] 93.33% la evaluation model tren WESAD (S13+S16)."));
}

static void printEcgResult(const orchestrator::EcgResult& r) {
    if (r.state == orchestrator::ReadyState::NOT_READY) {
        Serial.println(F("  Trang thai: NOT_READY (chua co 9 feature)"));
        return;
    }
    if (!r.valid) {
        Serial.println(F("  Trang thai: READY nhung EI inference tra loi"));
        return;
    }
    Serial.printf("  READY | P(AF)=%.4f | P(non-AF)=%.4f | pred=%s\n",
                  r.prob_af, r.prob_nonaf, r.is_af ? "AF" : "non-AF");
    Serial.println(F("  [FIXTURE] Khong co nhan xac minh — khong tinh accuracy."));
    Serial.println(F("  [NOTE] Don vi 9 feature chua xac minh voi training data EI."));
    Serial.println(F("  [DISCLAIMER] AF/non-AF only. Khong phai chan doan y te."));
}

static void printSpo2Result(const orchestrator::Spo2Result& r) {
    if (r.state == orchestrator::ReadyState::NOT_READY) {
        Serial.println(F("  Trang thai: NOT_READY (< 4 s du lieu)"));
        return;
    }
    Serial.printf("  READY | status=%s", research_spo2::statusText(r.detail.status));
    if (r.detail.valid()) {
        Serial.printf(" | SpO2=%d%%", r.detail.percent);
        if (r.detail.heartRateValid)
            Serial.printf(" | HR=%d bpm (uoc tinh thuat toan)", r.detail.heartRate);
    }
    Serial.println();
    Serial.println(F("  [FIXTURE] Tin hieu hinh sin tong hop — KHONG phai do thuc."));
    Serial.println(F("  [DISCLAIMER] Uoc tinh ty so AC/DC, chua kiem chung lam sang."));
}