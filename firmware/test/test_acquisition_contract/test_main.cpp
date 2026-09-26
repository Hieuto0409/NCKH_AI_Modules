#include <unity.h>
#include "drivers/max30102_fifo.h"
#include "acquisition/ecg_acquisition.h"
#include "acquisition/timestamp_service.h"
#include "features/feature_builder.h"
#include "features/ecg_window_builder.h"
#include "app/measurement_state_machine.h"
#include "metrics/spo2_rate_adapter.h"
#include <cmath>
#include <cstring>
#include <vector>
using namespace ppgfw;
struct FakeBus : Max30102Bus {
    uint8_t status=0, wp=0, rp=0, overflow=0;
    unsigned popped=0, reads=0, fail_data_read=0;
    bool nack=false;
    std::vector<unsigned> writes;
    bool read(uint8_t reg,uint8_t* out,size_t n) override {
        if(nack) return false;
        if(reg==0) { *out=status; status=0; return true; }
        if(reg==4) { out[0]=wp; out[1]=overflow; out[2]=rp; return n==3; }
        if(reg==9) { *out=3; return true; }
        if(reg!=7 || n%6 || n>30) return false;
        if(++reads==fail_data_read) return false;
        for(size_t i=0;i<n/6;++i) {
            uint32_t red=10000+popped,ir=20000+popped;
            for(unsigned j=0;j<3;++j) {
                out[i*6+j]=uint8_t(red>>(16-8*j)); out[i*6+3+j]=uint8_t(ir>>(16-8*j));
            }
            ++popped; rp=(rp+1)&31;
        }
        return true;
    }
    bool write(uint8_t reg,uint8_t value) override {
        if(nack) return false;
        writes.push_back((reg<<8)|value);
        if(reg==4) wp=value; if(reg==5) overflow=value; if(reg==6) rp=value;
        return true;
    }
};
void test_fifo_preserves_every_sample_and_order() {
    for(unsigned count : {1U,3U,4U,8U,31U,32U}) {
        FakeBus bus; bus.rp=27; bus.wp=(27+count)&31; bus.status=count==32?0x80:0;
        Max30102Fifo fifo; PpgFifoSample out[32]{};
        const auto r=fifo.drain(bus,out,32);
        TEST_ASSERT_FALSE(r.discontinuity); TEST_ASSERT_EQUAL_UINT32(count,r.count);
        for(unsigned i=0;i<count;++i) { TEST_ASSERT_EQUAL_UINT32(10000+i,out[i].red); TEST_ASSERT_EQUAL_UINT32(20000+i,out[i].ir); }
        TEST_ASSERT_EQUAL_UINT32(0,fifo.drain(bus,out,32).count);
    }
}
void test_fifo_partial_read_nack_overflow_and_stale_recover() {
    for(int scenario=0;scenario<4;++scenario) {
        FakeBus bus; bus.wp=8;
        if(scenario==0) bus.fail_data_read=2;
        if(scenario==1) bus.nack=true;
        if(scenario==2) bus.overflow=31;
        if(scenario==3) bus.status=0x80; // stale A_FULL must not survive recovery
        Max30102Fifo fifo; PpgFifoSample out[32]{};
        auto r=fifo.drain(bus,out,32,scenario==3);
        TEST_ASSERT_TRUE(r.discontinuity); TEST_ASSERT_EQUAL_UINT32(0,r.count);
        TEST_ASSERT_GREATER_THAN_UINT32(0,r.lost_samples);
        bus.nack=false; bus.fail_data_read=0;
        if(scenario==1) { TEST_ASSERT_TRUE(fifo.drain(bus,out,32).discontinuity); }
        TEST_ASSERT_EQUAL_UINT32(0,fifo.drain(bus,out,32).count);
        bus.wp=4; TEST_ASSERT_EQUAL_UINT32(4,fifo.drain(bus,out,32).count);
    }
}
uint64_t fake_now=0;
uint64_t fakeClock() { return fake_now; }
struct FakeAdc : EcgAdcBackend {
    int reads=0; bool off=false;
    bool begin() override {return true;}
    int16_t readRaw() override {++reads;return 2000;}
    bool leadOff() const override{return off;}
    const char* name() const override{return "fake";}
};
void test_adc_delay_never_backfills_historical_samples() {
    fake_now=0; FakeAdc adc; EcgSampleQueue q; EcgAcquisition acq(adc,q,fakeClock);
    TEST_ASSERT_TRUE(acq.begin()); fake_now=10000; acq.poll();
    TEST_ASSERT_EQUAL_INT(1,adc.reads); TEST_ASSERT_EQUAL_UINT32(1,q.size());
    EcgSample s; TEST_ASSERT_TRUE(q.pop(s)); TEST_ASSERT_EQUAL_UINT64(10000,s.timestamp_us);
    TEST_ASSERT_EQUAL_UINT32(5,s.seq); TEST_ASSERT_BITS_HIGH(SampleDropoutContext,s.flags);
    TEST_ASSERT_EQUAL_UINT32(5,acq.diagnostics().dropped_samples);
    acq.poll(); TEST_ASSERT_EQUAL_INT(1,adc.reads);
    fake_now=12000; acq.poll(); acq.resetWindowDiagnostics();
    TEST_ASSERT_EQUAL_UINT32(1,q.size()); // reset cannot erase producer data
    TEST_ASSERT_TRUE(q.pop(s)); TEST_ASSERT_EQUAL_UINT32(6,s.seq);
}
void test_ppg_poll_jitter_does_not_reset_spo2() {
    Spo2RateAdapter200To100 a; a.reset({200,63,63,8192});
    uint64_t last=0; uint32_t seq=0;
    for(unsigned batch=0;batch<200;++batch) {
        auto p=PpgTimestampReconstructor::plan(1000000+batch*20001ULL,4,last,5000);
        for(unsigned i=0;i<4;++i) {
            last=p.first_timestamp_us+i*5000;
            a.push({last,seq++,40000,60000,SampleValid|SampleContact});
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0,a.resetCount()); TEST_ASSERT_EQUAL_UINT32(400,a.modulePairCount());
    a.push({last+5000,seq-1,40000,60000,SampleValid|SampleContact});
    TEST_ASSERT_EQUAL_UINT32(0,a.modulePairCount()); TEST_ASSERT_EQUAL_UINT32(1,a.resetCount());
    a.updateConfig({100,63,63,8192});
    TEST_ASSERT_EQUAL(int(Spo2EstimatorStatus::InvalidInput),int(a.evaluate(true).estimator_status));
}
void test_clock_uses_exact_microsecond_deadline_and_drain() {
    MeasurementStateMachine sm; const uint64_t start=(1ULL<<32)*1000+123;
    sm.begin(start); sm.selfTestComplete(true,start);
    sm.update(start,true,false,true,false); sm.update(start+1,false,false,true,false);
    sm.update(start+500001,false,false,true,false);
    TEST_ASSERT_EQUAL(int(MeasurementState::Warmup),int(sm.state()));
    sm.update(start+3500001,false,false,true,false);
    const auto end=sm.measurementEndUs();
    TEST_ASSERT_EQUAL_UINT64(60000000,end-sm.measurementStartUs());
    sm.update(end-1,false,false,true,false); TEST_ASSERT_EQUAL(int(MeasurementState::Measuring),int(sm.state()));
    sm.update(end,false,false,true,false,false); TEST_ASSERT_EQUAL(int(MeasurementState::Measuring),int(sm.state()));
    sm.update(end+5000,false,false,true,false,true); TEST_ASSERT_EQUAL(int(MeasurementState::QualityEvaluation),int(sm.state()));
}
void fillEcg(EcgWindowBuilder& b,bool dirty_first=false) {
    b.reset(1000000);
    for(uint32_t n=0;n<30000;++n) {
        const uint64_t t=1000000+n*2000ULL;
        uint16_t flags=SampleValid;
        if(dirty_first && n<2500) flags|=SampleLeadOff;
        b.addSample({t,n,2000,flags},n%2?20.0F:-20.0F);
        if(n%500==0) b.addPeak(t,100);
    }
}
void test_ecg_selects_30s_same_window_and_earliest_tie() {
    EcgWindowBuilder b; fillEcg(b);
    auto r=b.build(61000000);
    TEST_ASSERT_EQUAL_UINT64(1000000,r.metadata.start_timestamp_us);
    TEST_ASSERT_EQUAL_UINT64(31000000,r.metadata.end_timestamp_us);
    TEST_ASSERT_EQUAL_UINT16(29,r.features.interval_count);
    TEST_ASSERT_EQUAL_UINT32(15000,r.metadata.integrity.received_samples);
    TEST_ASSERT_EQUAL(int(QualityStatus::Good),int(r.metadata.quality.status));
    fillEcg(b,true); r=b.build(61000000);
    TEST_ASSERT_EQUAL_UINT64(6000000,r.metadata.start_timestamp_us);
    TEST_ASSERT_EQUAL_UINT16(29,r.features.interval_count);
    TEST_ASSERT_EQUAL_UINT32(0,r.metadata.integrity.lead_off_count);
    TEST_ASSERT_EQUAL(int(FeatureVectorStatus::NotReady),int(b.build(2000000).features.status));
}
void test_ecg_gap_rejects_without_cross_gap_interval() {
    EcgWindowBuilder b; fillEcg(b);
    // Gap at 30 s is inside candidates 1..6, but outside candidate 0.
    b.breakContinuity(31000000);
    auto r=b.build(61000000);
    TEST_ASSERT_EQUAL_UINT64(1000000,r.metadata.start_timestamp_us);
    TEST_ASSERT_EQUAL(int(QualityStatus::Good),int(r.metadata.quality.status));
    b.breakContinuity(30000000); r=b.build(61000000);
    TEST_ASSERT_EQUAL(int(FeatureVectorStatus::QualityRejected),int(r.features.status));
}
void test_raw_replay_warmup_boundaries_and_gap_gate() {
    for(int gap=0;gap<2;++gap) {
        FeatureBuilder b; b.resetPipeline();
        const uint64_t begin=4000000, end=64000000;
        for(uint32_t n=0;n<=31500;++n) {
            const uint64_t t=1000000+n*2000ULL;
            if(t==begin) b.beginMeasurementWindow(begin);
            const double phase=2*3.141592653589793*double(t)/1000000;
            b.consume(EcgSample{t,n,int16_t(2000+100*std::sin(phase)),SampleValid});
            if(n%5==0) {
                // Two PPG samples every 10 ms, preserving 200 Hz timestamps.
                for(unsigned j=0;j<2;++j) {
                    const uint64_t pt=t+j*5000; const uint32_t seq=(n/5)*2+j;
                    const double p=2*3.141592653589793*double(pt)/1000000;
                    const uint16_t flags=SampleValid|SampleContact|((gap && pt==34000000)?SampleDropoutContext:0);
                    b.consume(PpgSample{pt,seq,uint32_t(40000+500*std::sin(p)),uint32_t(60000+1200*std::sin(p)),flags});
                }
            }
        }
        TEST_ASSERT_TRUE(b.windowDrained(end));
        auto r=b.build(end,{},{});
        TEST_ASSERT_EQUAL_UINT64(60000000,r.ppg_window.end_timestamp_us-r.ppg_window.start_timestamp_us);
        TEST_ASSERT_EQUAL_UINT32(12000,r.ppg_window.integrity.received_samples);
        TEST_ASSERT_EQUAL_UINT64(4000000,r.spo2_window.end_timestamp_us-r.spo2_window.start_timestamp_us);
        TEST_ASSERT_EQUAL_UINT64(end,r.spo2_window.end_timestamp_us);
        TEST_ASSERT_EQUAL_UINT64(30000000,r.ecg_window.end_timestamp_us-r.ecg_window.start_timestamp_us);
        if(gap) TEST_ASSERT_EQUAL(int(FeatureVectorStatus::QualityRejected),int(r.stress_ppg.status));
        else {
            TEST_ASSERT_EQUAL_UINT32(0,r.metrics.spo2.adapter_reset_count);
            TEST_ASSERT_EQUAL(int(FeatureVectorStatus::Ready),int(r.stress_ppg.status));
            TEST_ASSERT_FLOAT_WITHIN(1.0F,60,r.metrics.ppg_pulse_rate_bpm.value);
        }
    }
}

void test_fifo_random_batch_boundaries() {
    FakeBus bus; Max30102Fifo fifo; PpgFifoSample out[32]; unsigned total=0, rng=17;
    for(unsigned k=0;k<100;++k) {
        rng=rng*1664525U+1013904223U; const unsigned n=1+rng%31;
        bus.wp=(bus.rp+n)&31;
        auto r=fifo.drain(bus,out,32); TEST_ASSERT_EQUAL_UINT32(n,r.count);
        for(unsigned j=0;j<n;++j) TEST_ASSERT_EQUAL_UINT32(10000+total+j,out[j].red);
        total+=n;
    }
}
void test_ecg_queue_overrun_preserves_loss_context() {
    fake_now=0; FakeAdc adc; EcgSampleQueue q; EcgAcquisition a(adc,q,fakeClock);a.begin();
    for(unsigned n=0;n<1030;++n) {fake_now=n*2000ULL;a.poll();}
    TEST_ASSERT_EQUAL_UINT32(7,a.diagnostics().queue_drop_count);
    EcgSample sample;while(q.pop(sample)) {}
    fake_now=2060000;a.poll();TEST_ASSERT_TRUE(q.pop(sample));
    TEST_ASSERT_BITS_HIGH(SampleDropoutContext,sample.flags);TEST_ASSERT_EQUAL_UINT32(1030,sample.seq);
    for(unsigned pause : {10000U,20000U,100000U}) {
        fake_now+=pause;a.poll();TEST_ASSERT_TRUE(q.pop(sample));
        TEST_ASSERT_EQUAL_UINT64(fake_now,sample.timestamp_us);TEST_ASSERT_FALSE(q.pop(sample));
    }
}
void test_state_cancel_restart_has_new_window() {
    MeasurementStateMachine s;s.begin(1000);s.selfTestComplete(true,1000);
    auto start=[&](uint64_t t) {s.update(t,true,false,true,false);s.update(t+1,false,false,true,false);
        s.update(t+500001,false,false,true,false);s.update(t+3500001,false,false,true,false);};
    start(2000);auto first=s.measurementStartUs();
    s.update(first+1000,false,true,true,false);TEST_ASSERT_EQUAL(int(MeasurementState::Idle),int(s.state()));
    start(first+2000);TEST_ASSERT_GREATER_THAN_UINT64(first,s.measurementStartUs());
    TEST_ASSERT_EQUAL_UINT64(60000000,s.measurementEndUs()-s.measurementStartUs());
}
void test_ecg_boundary_count_jitter_is_not_a_transport_gap() {
    EcgWindowBuilder b;b.reset(1000000);
    // A sample shifts just beyond the boundary: count-based completeness still
    // applies; it must not invent a sequence gap from nominal count alone.
    for(uint32_t n=0;n<14999;++n) {
        b.addSample({1000001+n*2000ULL,n,2000,SampleValid},n%2?20:-20);
        if(n%500==0)b.addPeak(1000001+n*2000ULL,100);
    }
    const auto r=b.build(31000000);
    TEST_ASSERT_EQUAL_UINT32(0,r.metadata.integrity.dropped_samples);
    TEST_ASSERT_EQUAL(int(QualityStatus::Good),int(r.metadata.quality.status));
}

void test_ecg_rate_change_selects_most_valid_rr_not_session_average() {
    EcgWindowBuilder b;b.reset(1000000);
    for(uint32_t n=0;n<30000;++n) {
        const auto t=1000000+n*2000ULL;b.addSample({t,n,2000,SampleValid},n%2?20:-20);
        if((n<15000 && n%500==0)||(n>=15000 && (n-15000)%333==0))b.addPeak(t,100);
    }
    auto r=b.build(61000000);
    TEST_ASSERT_EQUAL_UINT64(31000000,r.metadata.start_timestamp_us);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F,.666F,r.features.values[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F,0,r.features.values[2]);
}
void test_raw_flatline_and_clipping_are_rejected() {
    FeatureBuilder b;b.reset(1000000);
    for(uint32_t n=0;n<=30000;++n) {
        auto t=1000000+n*2000ULL;
        b.consume(EcgSample{t,n,4095,SampleValid|SampleClipping});
        if(n%5==0)for(unsigned j=0;j<2;++j)
            b.consume(PpgSample{t+j*5000,(n/5)*2+j,40000,60000,SampleValid|SampleContact});
    }
    const auto r=b.build(61000000,{},{});
    TEST_ASSERT_EQUAL(int(QualityStatus::Poor),int(r.ppg_quality.status));
    TEST_ASSERT_EQUAL(int(QualityStatus::Poor),int(r.ecg_quality.status));
    TEST_ASSERT_EQUAL(int(FeatureVectorStatus::QualityRejected),int(r.ecg_af.status));
    TEST_ASSERT_EQUAL(int(FeatureVectorStatus::QualityRejected),int(r.stress_ppg.status));
}
void test_adc_pauses_and_resumes_without_counting_sleep_as_loss() {
    fake_now=0; FakeAdc adc; EcgSampleQueue q; EcgAcquisition a(adc,q,fakeClock);
    TEST_ASSERT_TRUE(a.begin()); a.poll();
    TEST_ASSERT_TRUE(a.setActive(false)); TEST_ASSERT_EQUAL_UINT32(0,q.size());
    fake_now=60000000; a.poll(); TEST_ASSERT_EQUAL_INT(1,adc.reads);
    TEST_ASSERT_TRUE(a.setActive(true)); a.poll();
    TEST_ASSERT_EQUAL_INT(2,adc.reads); TEST_ASSERT_EQUAL_UINT32(0,a.diagnostics().dropped_samples);
    EcgSample sample;TEST_ASSERT_TRUE(q.pop(sample));
    TEST_ASSERT_EQUAL_UINT64(fake_now,sample.timestamp_us);
    TEST_ASSERT_BITS_HIGH(SampleDropoutContext,sample.flags);
}
void test_contact_timeout_survives_warmup_contact_bounce() {
    MeasurementStateMachine sm; sm.begin(0); sm.selfTestComplete(true,0);
    sm.update(1,true,false,false,true);
    sm.update(10,false,false,true,false);sm.update(500010,false,false,true,false);
    TEST_ASSERT_EQUAL(int(MeasurementState::Warmup),int(sm.state()));
    sm.update(500011,false,false,false,true);
    TEST_ASSERT_EQUAL(int(MeasurementState::ContactWait),int(sm.state()));
    sm.update(120000001,false,false,false,true);
    TEST_ASSERT_EQUAL(int(MeasurementState::Idle),int(sm.state()));
    sm.update(120000002,true,false,false,true);
    TEST_ASSERT_EQUAL(int(MeasurementState::ContactWait),int(sm.state()));
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_adc_pauses_and_resumes_without_counting_sleep_as_loss);
    RUN_TEST(test_contact_timeout_survives_warmup_contact_bounce);
    RUN_TEST(test_ecg_rate_change_selects_most_valid_rr_not_session_average);
    RUN_TEST(test_raw_flatline_and_clipping_are_rejected);
    RUN_TEST(test_fifo_random_batch_boundaries);
    RUN_TEST(test_ecg_queue_overrun_preserves_loss_context);
    RUN_TEST(test_state_cancel_restart_has_new_window);
    RUN_TEST(test_ecg_boundary_count_jitter_is_not_a_transport_gap);
    RUN_TEST(test_fifo_preserves_every_sample_and_order);
    RUN_TEST(test_fifo_partial_read_nack_overflow_and_stale_recover);
    RUN_TEST(test_adc_delay_never_backfills_historical_samples);
    RUN_TEST(test_ppg_poll_jitter_does_not_reset_spo2);
    RUN_TEST(test_clock_uses_exact_microsecond_deadline_and_drain);
    RUN_TEST(test_ecg_selects_30s_same_window_and_earliest_tie);
    RUN_TEST(test_ecg_gap_rejects_without_cross_gap_interval);
    RUN_TEST(test_raw_replay_warmup_boundaries_and_gap_gate);
    return UNITY_END();
}
