#include "app_conf.h"


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


