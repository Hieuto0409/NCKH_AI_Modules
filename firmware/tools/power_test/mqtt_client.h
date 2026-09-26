#pragma once
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
using esp_event_base_t=const char*;
constexpr int ESP_OK=0,MQTT_EVENT_CONNECTED=1,MQTT_EVENT_DISCONNECTED=2,MQTT_EVENT_ERROR=3,
 MQTT_EVENT_PUBLISHED=4,MQTT_EVENT_ANY=-1,MQTT_TRANSPORT_OVER_TCP=1;
struct esp_mqtt_event_t {int msg_id;};
using esp_mqtt_event_handle_t=esp_mqtt_event_t*;
struct esp_mqtt_client_config_t {const char* host{};int port{};const char* username{};const char* password{};
 int transport{},buffer_size{},network_timeout_ms{},keepalive{};bool disable_auto_reconnect{};};
struct FakeMqtt {
 void (*callback)(void*,esp_event_base_t,int32_t,void*){}; void* context{};
 std::vector<std::string> sent; int id=0;bool fail_enqueue=false,fail_init=false;
 void event(int kind,int msg=-1) {esp_mqtt_event_t e{msg};callback(context,nullptr,kind,&e);}
};
inline FakeMqtt broker;
using esp_mqtt_client_handle_t=FakeMqtt*;
inline auto esp_mqtt_client_init(const esp_mqtt_client_config_t* c) {
 assert(c->disable_auto_reconnect && c->network_timeout_ms==1000);
 assert(!std::strcmp(c->username,"test-token") && !*c->password && c->port==1883);
 return broker.fail_init?nullptr:&broker;
}
inline int esp_mqtt_client_register_event(FakeMqtt*,int,decltype(FakeMqtt::callback) f,void* context) {
 broker.callback=f;broker.context=context;return ESP_OK;
}
inline int esp_mqtt_client_start(FakeMqtt*) {return ESP_OK;}
inline int esp_mqtt_client_stop(FakeMqtt*) {return ESP_OK;}
inline int esp_mqtt_client_destroy(FakeMqtt*) {return ESP_OK;}
inline int esp_mqtt_client_enqueue(FakeMqtt*,const char* topic,const char* data,int,int qos,int retain,bool store) {
 assert(qos==1 && retain==0 && store && !std::strcmp(topic,"v1/devices/me/telemetry"));
 if(broker.fail_enqueue)return -1;broker.sent.push_back(data);return ++broker.id;
}
