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
