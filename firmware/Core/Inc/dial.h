
#ifndef __DIAL_H
#define __DIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


struct DialState {
    int32_t acc; //accumulator for filtering
    uint16_t filtered; //filtered value
    uint16_t filterFraction; //q16 fixed point fraction representing the rate of change
    uint16_t maxValue; //for scaling readings
    uint16_t steps; //quantize the input to steps
    uint16_t stepValue;
    uint16_t value; //current value in steps
    //consumable events
	uint8_t hasChanged:1;
};

typedef volatile struct DialState DialState;

void dialInit(DialState *dial, uint16_t filterFraction, uint16_t maxValue, uint16_t steps);
bool dialProcess(DialState *dial, uint16_t rawValue);
uint16_t dialGetValue(DialState *dial);
bool dialPollChangeEvent(DialState *dial);

#ifdef __cplusplus
}
#endif

#endif //__DIAL_H
