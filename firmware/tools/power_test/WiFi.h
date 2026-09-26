#pragma once
constexpr int WIFI_OFF=0,WIFI_STA=1,WL_CONNECTED=3;
struct FakeWiFi {
 int mode_value=0, begins=0, status_value=0;
 bool auto_reconnect=true;
 void persistent(bool) {}
 void setAutoReconnect(bool enabled) {auto_reconnect=enabled;}
 bool mode(int value) {mode_value=value;return true;}
 void begin(const char*, const char*) {++begins;status_value=0;}
 int status() {return status_value;}
 void disconnect(bool,bool) {status_value=0;}
};
inline FakeWiFi WiFi;
