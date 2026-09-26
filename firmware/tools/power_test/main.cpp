#include "network/mqtt_client.h"
#include <cassert>
#include <iostream>
using ppgfw::MqttClient;
void reset() {WiFi=FakeWiFi{};broker=FakeMqtt{};}
void connect(MqttClient& c,uint32_t now=0) {
 c.loop(now,true);WiFi.status_value=WL_CONNECTED;c.loop(now+1,true);
 broker.event(MQTT_EVENT_CONNECTED);c.loop(now+2,true);
}
int main() {
 reset();{
 MqttClient c;c.begin();assert(WiFi.mode_value==WIFI_OFF && !WiFi.auto_reconnect);
 c.loop(0,true);assert(WiFi.begins==0);
 assert(c.publish("first"));c.loop(1,false);assert(WiFi.begins==0 && c.pending()==1);
 // A completed new measurement arms one upload session, preserving the older result.
 assert(c.publish("second"));connect(c,10);
 assert(c.pending()==2 && c.acknowledged()==0 && c.radioActive());
 broker.event(MQTT_EVENT_PUBLISHED,999);c.loop(13,true);assert(c.pending()==2);
 broker.event(MQTT_EVENT_PUBLISHED,broker.id);c.loop(14,true);assert(c.pending()==1);
 broker.event(MQTT_EVENT_PUBLISHED,broker.id);c.loop(15,true);
 assert(c.pending()==0 && c.acknowledged()==2 && !c.radioActive() && WiFi.mode_value==WIFI_OFF);
 assert(broker.sent.size()==2 && broker.sent[0]=="first" && broker.sent[1]=="second");
 }
 reset();{
 MqttClient c;c.begin();assert(c.publish("offline"));c.loop(100,true);
 c.loop(30100,true);assert(!c.radioActive() && c.pending()==1);
 for(int i=0;i<10;++i)c.loop(40000+i*1000,true);
 assert(WiFi.begins==1); // timeout cannot start an endless battery-draining retry loop
 assert(c.publish("retry-trigger"));c.loop(60000,true);assert(WiFi.begins==2);c.suspend();
 }
 reset();{
 MqttClient c;c.begin();c.publish("rollover");c.loop(0xfffffff0u,true);
 c.loop(0xfffffff0u+29999u,true);assert(c.radioActive());
 c.loop(0xfffffff0u+30000u,true);assert(!c.radioActive() && c.pending()==1);
 }
 reset();{
 MqttClient c;c.begin();c.publish("interrupted");connect(c);
 c.loop(3,false);assert(!c.radioActive() && c.pending()==1 && !c.connected());
 c.loop(4,true);assert(!c.radioActive());
 c.publish("next");connect(c,10);assert(broker.sent.back()=="interrupted");c.suspend();
 }
 reset();{
 MqttClient c;c.begin();for(int i=0;i<4;++i)assert(c.publish("queued"));
 assert(!c.publish("overflow") && c.rejected()==1 && c.pending()==4);
 c.suspend();
 }
 reset();{
 MqttClient c;c.begin();c.publish("broker-error");connect(c);
 broker.event(MQTT_EVENT_ERROR);c.loop(3,true);
 assert(!c.radioActive() && c.pending()==1 && c.acknowledged()==0);
 }
 reset();{
 MqttClient c;c.begin();c.publish("enqueue-error");broker.fail_enqueue=true;connect(c);
 assert(!c.radioActive() && c.pending()==1);
 }
 reset();{
 MqttClient c;c.begin();c.publish("init-error");broker.fail_init=true;
 c.loop(0,true);WiFi.status_value=WL_CONNECTED;c.loop(1,true);
 assert(!c.radioActive() && c.pending()==1);
 }
 std::cout<<"{\"network_scenarios\":8,\"status\":\"PASS\",\"scope\":\"production upload controller with fake WiFi/ESP-MQTT; no real broker or RF measurement\"}\n";
}
