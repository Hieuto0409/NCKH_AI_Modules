#include "ResearchSpO2.h"
#include "MaximCore.h"
namespace research_spo2 {
static uint8_t fault(uint32_t ir, uint32_t red, Config c) {
    if (ir > 262143 || red > 262143) return 1;
    if (ir == 262143 || red == 262143) return 2;
    if (ir == 0 || red == 0 || ir < c.minIr) return 3;
    return 0;
}
static Status faultStatus(uint8_t f) {
    return f == 1 ? Status::InvalidInput : f == 2 ? Status::Clipped : Status::ContactLost;
}
Result calculate25(const uint32_t* ir, const uint32_t* red, std::size_t n,
                   bool quality, Config config) {
    Result r;
    if (!ir || !red || n != 100) { r.status=Status::InvalidInput; return r; }
    if (!quality) { r.status=Status::QualityRejected; return r; }
    uint32_t x[100], y[100];
    for (unsigned i=0; i<100; ++i) {
        const uint8_t f=fault(ir[i],red[i],config);
        if (f) { r.status=faultStatus(f); return r; }
        x[i]=ir[i]; y[i]=red[i];
    }
    int32_t sp=-999, hr=-999; int8_t sv=0,hv=0;
    research_spo2_core::maxim_heart_rate_and_oxygen_saturation(x,100,y,&sp,&sv,&hr,&hv);
    if (sv != 1 || sp < 0 || sp > 100) { r.status=Status::AlgorithmRejected; return r; }
    r.status=Status::Ok; r.percent=sp;
    r.heartRateValid=(hv==1); r.heartRate=r.heartRateValid?hr:-1;
    return r;
}
void Stream100::reset() {
    write_=count_=faultWrite_=rawCount_=0; phase_=0;
    for (unsigned i=0;i<400;++i) faults_[i]=0;
}
void Stream100::push(uint32_t ir,uint32_t red) {
    faults_[faultWrite_]=fault(ir,red,config_);
    faultWrite_=(faultWrite_+1)%400;
    if (rawCount_<400) ++rawCount_;
    if (phase_==0) {
        ir_[write_]=ir; red_[write_]=red;
        write_=(write_+1)%100; if (count_<100) ++count_;
    }
    phase_=(phase_+1)%4;
}
Result Stream100::evaluate(bool quality) {
    Result r;
    if (rawCount_<400) return r;
    if (!quality) { r.status=Status::QualityRejected; return r; }
    // Check every raw sample, including samples discarded during decimation.
    for (unsigned i=0;i<400;++i) if (faults_[i]) {
        r.status=faultStatus(faults_[i]); return r;
    }
    uint32_t x[100],y[100];
    for (unsigned i=0;i<100;++i) {
        unsigned j=(write_+i)%100; x[i]=ir_[j]; y[i]=red_[j];
    }
    return calculate25(x,y,100,true,config_);
}
const char* statusText(Status s) {
    switch(s) {
    case Status::Ok:return "CO_KET_QUA_UOC_TINH";
    case Status::NeedData:return "CHUA_DU_4_GIAY";
    case Status::InvalidInput:return "DAU_VAO_KHONG_HOP_LE";
    case Status::ContactLost:return "KHONG_DU_TIEP_XUC";
    case Status::Clipped:return "TIN_HIEU_BAO_HOA";
    case Status::QualityRejected:return "TIN_HIEU_KEM";
    default:return "THUAT_TOAN_KHONG_CHO_KET_QUA";
    }
}
}
