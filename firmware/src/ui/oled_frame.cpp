#include "ui/oled_frame.h"
#include <cstdio>
#include <cmath>
namespace ppgfw {
namespace {
const char* uploadText(UploadStage stage) {
    switch(stage) {
        case UploadStage::Offline: return "Che do offline";
        case UploadStage::NotConfigured: return "Chua cai mang";
        case UploadStage::Ready: return "Wi-Fi dang tat";
        case UploadStage::Queued: return "Cho gui ket qua";
        case UploadStage::Wifi: return "Dang noi Wi-Fi...";
        case UploadStage::Clock: return "Dong bo gio...";
        case UploadStage::Sending: return "Dang gui...";
        case UploadStage::Sent: return "May chu da nhan";
        case UploadStage::Deferred: return "Chua gui / cho lai";
        case UploadStage::Rejected: return "KQ moi CHUA luu/gui";
    }
    return "Chua gui";
}
void row(OledFrame& f, unsigned n, const char* text) {
    std::snprintf(f.rows[n].data(), f.rows[n].size(), "%s", text);
}
void metric(OledFrame& f, unsigned n, const char* label, MetricValue m, const char* unit) {
    if(m.status == ValueStatus::Valid && std::isfinite(m.value))
        std::snprintf(f.rows[n].data(), f.rows[n].size(), "%s %.1f %s", label, m.value, unit);
    else std::snprintf(f.rows[n].data(), f.rows[n].size(), "%s --", label);
}
}
OledFrame makeOledFrame(MeasurementState state, const ResultSnapshot& r,
    bool ppg, bool contact, bool lead_off, uint8_t page, UploadSnapshot u) {
    OledFrame f{};
    switch(state) {
    case MeasurementState::Idle:
        row(f,0,"SAN SANG DO"); row(f,1,"Dat ngon tay + ECG");
        row(f,2,"Giu yen trong 60s"); row(f,3,"OLED tat khi dang do");
        row(f,4,uploadText(u.stage));
        row(f,5,u.pending?"B1:Do B2:Gui lai":"B1:Bat dau do"); break;
    case MeasurementState::ContactWait:
        row(f,0,"KIEM TRA TIEP XUC");
        row(f,1,!ppg?"MAX30102: LOI":(contact?"Ngon tay: OK":"Hay dat ngon tay"));
        row(f,2,lead_off?"Hay gan dien cuc ECG":"ECG: san sang");
        row(f,3,"Giu yen, tha long"); row(f,4,"Do 60s: OLED se tat"); row(f,5,"B2:Huy"); break;
    case MeasurementState::Warmup:
    case MeasurementState::Measuring:
        // Controller keeps the panel asleep; no SPI refreshes while sensing.
        row(f,0,"DANG DO - GIU YEN"); row(f,5,"B2:Huy"); break;
    case MeasurementState::QualityEvaluation:
        row(f,0,"DANG XU LY"); row(f,2,"Kiem tra tin hieu"); row(f,3,"Tinh ket qua AI..."); break;
    case MeasurementState::Result:
        if(page%3 == 0) {
            row(f,0,r.remeasure?"KQ 1/3 - CAN DO LAI":"KET QUA 1/3");
            metric(f,1,"ECG",r.feature_packet.metrics.ecg_heart_rate_bpm,"bpm");
            metric(f,2,"PPG",r.feature_packet.metrics.ppg_pulse_rate_bpm,"bpm");
            const auto& s=r.feature_packet.metrics.spo2;
            metric(f,3,"SpO2 uoc",{s.status,s.percent},"%");
            row(f,4,uploadText(u.stage));
        } else if(page%3 == 1) {
            row(f,0,"SANG LOC AI 2/3");
            row(f,1,r.stress.status!=InferenceStatus::Valid?"Stress: chua du DL":
                r.stress.label==BranchLabel::Stress?"Stress: tang":
                r.stress.label==BranchLabel::Baseline?"Stress: muc nen":"Stress: --");
            row(f,2,r.rhythm.status!=InferenceStatus::Valid?"AF: chua du du lieu":
                r.rhythm.label==BranchLabel::Af?"AF: CANH BAO SANG LOC":
                r.rhythm.label==BranchLabel::NonAf?"AF: khong phat hien":"AF: --");
            row(f,3,r.remeasure?"Tin hieu yeu: do lai":"Chi la sang loc");
            row(f,4,"Khong phai chan doan");
        } else {
            row(f,0,"GUI KET QUA 3/3"); row(f,1,uploadText(u.stage));
            std::snprintf(f.rows[2].data(),22,"Cho gui: %u",unsigned(u.pending));
            row(f,3,u.pending?"Tat nguon se mat KQ":"Wi-Fi tat sau khi gui");
            row(f,4,u.pending?"Ve cho: B2 gui lai":
                u.stage==UploadStage::Sent?"Du lieu tren may chu":"Kiem tra trang thai");
            if(u.stage==UploadStage::Offline || u.stage==UploadStage::NotConfigured)
                row(f,4,"Chua gui len may chu");
        }
        row(f,5,"B1:Do lai B2:Trang"); break;
    case MeasurementState::Error:
        row(f,0,"LOI CAM BIEN"); row(f,2,"Kiem tra day / nguon");
        row(f,3,"Khong co ket qua moi"); row(f,5,"B1:Thu lai"); break;
    default:
        row(f,0,"EDGE AI - PPG + ECG"); row(f,2,"Dang khoi dong..."); break;
    }
    return f;
}
}
