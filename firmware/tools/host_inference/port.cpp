// Host-only platform services; generated model/SDK remain unchanged.
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
EI_IMPULSE_ERROR ei_run_impulse_check_canceled() { return EI_IMPULSE_OK; }
EI_IMPULSE_ERROR ei_sleep(int32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); return EI_IMPULSE_OK; }
uint64_t ei_read_timer_us() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
uint64_t ei_read_timer_ms() { return ei_read_timer_us()/1000; }
void ei_printf(const char* format, ...) { va_list a; va_start(a,format); vfprintf(stderr,format,a); va_end(a); }
void ei_printf_float(float f) { ei_printf("%f",f); }
void ei_putchar(char c) { fputc(c,stderr); }
void* ei_malloc(size_t n) { return malloc(n); }
void* ei_calloc(size_t n,size_t s) { return calloc(n,s); }
void ei_free(void* p) { free(p); }
extern "C" void DebugLog(const char* s) { ei_printf("%s",s); }
