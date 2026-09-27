#include "ui/oled_frame.h"
#include "config/sampling.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
using namespace ppgfw;
bool contains(const OledFrame& f,const char* text) {
 for(const auto& row:f.rows)if(std::strstr(row.data(),text))return true;return false;
}
int main() {
 ResultSnapshot r{};UploadSnapshot u{};
 r.normal=true;r.rhythm.label=BranchLabel::NonAf; // stale labels cannot override invalid status
 auto f=makeOledFrame(MeasurementState::Result,r,true,true,false,1,u);
 assert(contains(f,"chua du") && !contains(f,"khong phat hien") && !contains(f,"NORMAL"));
 r.rhythm.status=InferenceStatus::Valid;
 f=makeOledFrame(MeasurementState::Result,r,true,true,false,1,u);
 assert(contains(f,"khong phat hien") && !contains(f,"NORMAL"));
 r.feature_packet.metrics.spo2={};r.feature_packet.metrics.spo2.status=ValueStatus::Valid;
 r.feature_packet.metrics.spo2.percent=std::numeric_limits<float>::infinity();
 f=makeOledFrame(MeasurementState::Result,r,true,true,false,0,u);
 assert(contains(f,"SpO2 uoc --") && !contains(f,"inf"));
 for(unsigned stage=0;stage<=unsigned(UploadStage::Rejected);++stage) {
  u.stage=UploadStage(stage);u.pending=stage==unsigned(UploadStage::Deferred)?4:0;
  for(unsigned state=0;state<=unsigned(MeasurementState::Error);++state)
   for(unsigned page=0;page<3;++page){
    f=makeOledFrame(MeasurementState(state),r,false,false,true,page,u);
    for(const auto& row:f.rows){assert(strlen(row.data())<=21);for(char c:row)assert(c>=0 && c<127);}
    if(u.stage!=UploadStage::Sent)assert(!contains(f,"May chu da nhan") && !contains(f,"Du lieu tren may chu"));
   }
 }
 MeasurementStateMachine sm;sm.begin(0);sm.selfTestComplete(true,0);sm.update(1,true,false,false,true);
 sm.update(10,false,false,true,false);sm.update(500010,false,false,true,false);
 sm.update(3500010,false,false,true,false);sm.update(63500010,false,false,true,false);
 sm.qualityEvaluated(64000000);sm.keepResultVisible(108000000);
 sm.update(109000000,false,false,true,false);assert(sm.state()==MeasurementState::Result);
 sm.update(153000000,false,false,true,false);assert(sm.state()==MeasurementState::Idle);
 // Same production frame builder used by the display; useful for reviewing exact labels.
 for(unsigned page=0;page<3;++page) {
  f=makeOledFrame(MeasurementState::Result,r,true,true,false,page,{UploadStage::Deferred,1,0,0});
  for(const auto& row:f.rows)std::cout<<row.data()<<'\n';std::cout<<'\n';
 }
 std::cout<<"OLED frame and result navigation regression PASS\n";
}
