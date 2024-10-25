#include "app_conf.h"


#define INTERFACE_SCAN_INTERVAL_MS 50
#define BUTTON_DEBOUNCE_COUNT 2
#define BUTTON_HOLD_COUNT 12

#ifndef SAMPLE_BUFFER_SIZE
#define SAMPLE_BUFFER_SIZE 1024
#endif

//check that sample buffer size is a power of 2
#if (SAMPLE_BUFFER_SIZE & (SAMPLE_BUFFER_SIZE - 1)) != 0
#error "SAMPLE_BUFFER_SIZE must be a power of 2"
#endif

#ifndef SAMPLE_BUFFER_MASK
#define SAMPLE_BUFFER_MASK (SAMPLE_BUFFER_SIZE - 1)
#endif

uint32_t getCycles();
uint32_t getTicks();



void adcManagerSetup();
void adcManagerLoop();
