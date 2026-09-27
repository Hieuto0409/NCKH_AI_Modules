#include "network/mqtt_client.h"
#include <ctime>
#include <Arduino.h>
#include <cassert>
#include <iostream>
using namespace ppgfw;
void reset() { WiFi=FakeWiFi{};http=FakeHttp{};fakeNetworkNow=1800000000;timeStarts=0; }
void connect(MqttClient& c) {c.loop(0,true);WiFi.status_value=WL_CONNECTED;c.loop(1,true);}
int main() {
 reset(); {
  MqttClient c;c.begin();assert(c.publish("first"));assert(!c.radioActive());connect(c);
  assert(c.pending()==1 && c.acknowledged()==0 && c.uploadStatus().stage==UploadStage::Sending);
  for(unsigned i=2;i<10;++i)c.loop(i,true);
  assert(http.created==1 && c.pending()==1); // EAGAIN is never an acknowledgement.
  http.result=ESP_OK;c.loop(10,true);
  assert(c.acknowledged()==1 && c.pending()==0 && !c.radioActive());
  assert(c.uploadStatus().stage==UploadStage::Sent && WiFi.mode_value==WIFI_OFF);
 }
 for(int status : {301,302,401,403,500,503}) {reset();
  MqttClient c;c.begin();c.publish("retry");http.result=ESP_OK;http.status=status;connect(c);
  assert(c.pending()==1 && c.acknowledged()==0 && !c.radioActive());
  assert(c.uploadStatus().stage==UploadStage::Deferred);
  c.loop(40000,true);assert(WiFi.begins==1); // no automatic battery-draining retry
  assert(c.retryPending());c.loop(40001,false);assert(!c.radioActive());
 }
 reset(); {
  MqttClient c;c.begin();c.publish("tls-failure");http.result=-1;connect(c);
  assert(c.pending()==1 && c.acknowledged()==0 && !c.radioActive());
 }
 reset(); {
  MqttClient c;c.begin();c.publish("clock");fakeNetworkNow=0;connect(c);
  assert(c.uploadStatus().stage==UploadStage::Clock && timeStarts==1 && !http.created);
  c.loop(20,true);assert(timeStarts==1);c.loop(30000,true);
  assert(!c.radioActive() && c.pending()==1 && !http.created);
  assert(c.retryPending());fakeNetworkNow=1800000000;c.loop(31000,true);
  WiFi.status_value=WL_CONNECTED;http.result=ESP_OK;c.loop(31001,true);
  assert(c.pending()==0 && c.acknowledged()==1);
 }
 reset(); {
  MqttClient c;c.begin();c.publish("cancel");connect(c);c.loop(2,false);
  assert(http.cleaned==1 && !c.radioActive() && c.pending()==1);
 }
 reset(); {
  MqttClient c;c.begin();for(int i=0;i<4;++i)assert(c.publish("q"));
  assert(!c.publish("newest"));assert(c.uploadStatus().stage==UploadStage::Rejected);
  http.result=ESP_OK;connect(c);for(int i=2;i<6;++i)c.loop(i,true);
  assert(c.pending()==0 && c.acknowledged()==4 && c.uploadStatus().stage==UploadStage::Rejected);
 }
 for(int failure=0;failure<2;++failure) {reset();MqttClient c;c.begin();c.publish("setup-error");
  http.fail_init=failure==0;http.fail_header=failure==1;connect(c);
  assert(!c.radioActive() && c.pending()==1 && c.acknowledged()==0);
 }
 std::cout<<"HTTPS transport PASS: ACK, EAGAIN, six HTTP errors, TLS failure, clock timeout/retry, cancellation, queue rejection, init/header failures\n";
}
