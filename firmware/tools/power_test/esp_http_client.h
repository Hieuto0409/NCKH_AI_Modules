#pragma once
#include "mqtt_client.h"
constexpr int ESP_ERR_HTTP_EAGAIN=11, HTTP_METHOD_POST=1;
struct esp_http_client_config_t {
 const char *url{}, *cert_pem{}, *user_agent{};
 int method{},timeout_ms{},buffer_size{},buffer_size_tx{};
 bool is_async{},disable_auto_redirect{},skip_cert_common_name_check{};
};
struct FakeHttp {
 int result=ESP_ERR_HTTP_EAGAIN,status=200,created=0,cleaned=0,performed=0;
 bool fail_init=false,fail_header=false; std::string payload;
};
inline FakeHttp http;
using esp_http_client_handle_t=FakeHttp*;
inline auto esp_http_client_init(const esp_http_client_config_t* c) {
 assert(c->is_async && c->disable_auto_redirect && !c->skip_cert_common_name_check);
 assert(c->cert_pem && strstr(c->cert_pem,"BEGIN CERTIFICATE"));
 assert(!strcmp(c->url,"https://test/api/v1/test-token/telemetry"));
 assert(c->method==HTTP_METHOD_POST && c->timeout_ms==1000);
 ++http.created;return http.fail_init?nullptr:&http;
}
inline int esp_http_client_set_header(FakeHttp*,const char*,const char*) {return http.fail_header?-1:ESP_OK;}
inline int esp_http_client_set_post_field(FakeHttp*,const char* p,int n) {http.payload.assign(p,n);return ESP_OK;}
inline int esp_http_client_perform(FakeHttp*) {++http.performed;return http.result;}
inline int esp_http_client_get_status_code(FakeHttp*) {return http.status;}
inline int esp_http_client_cleanup(FakeHttp*) {++http.cleaned;return ESP_OK;}
