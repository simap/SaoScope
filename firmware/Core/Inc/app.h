#include "app_conf.h"

//defaults, override in app_conf.h

#ifndef INTERFACE_SCAN_INTERVAL_MS
#define INTERFACE_SCAN_INTERVAL_MS 50
#endif

#ifndef BUTTON_DEBOUNCE_COUNT
#define BUTTON_DEBOUNCE_COUNT 2
#endif

#ifndef BUTTON_HOLD_COUNT
#define BUTTON_HOLD_COUNT 12
#endif

#ifndef SAMPLE_BUFFER_SIZE
#define SAMPLE_BUFFER_SIZE 1024
#endif

//ui message timer
#ifndef MESSAGE_TIMEOUT_MS
#define MESSAGE_TIMEOUT_MS 2000
#endif

#ifndef CONTINUOUS_SCAN_TIMEOUT_CYCLES
//200ms in cycles at 56mhz
#define CONTINUOUS_SCAN_TIMEOUT_CYCLES (30 * 56000000ll / 1000)
#endif

//check that sample buffer size is a power of 2
#if (SAMPLE_BUFFER_SIZE & (SAMPLE_BUFFER_SIZE - 1)) != 0
#error "SAMPLE_BUFFER_SIZE must be a power of 2"
#endif

#define SAMPLE_BUFFER_MASK (SAMPLE_BUFFER_SIZE - 1)


uint32_t getCycles();
uint32_t getTicks();


void adcManagerSystickISR();
void adcManagerSetup();
void adcManagerLoop();
int32_t uvToAdc(int32_t uv);
int32_t adcToUv(int32_t adc);
void setSignalGen();
void setScopeMode(int mode);
