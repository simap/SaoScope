#include "dial.h"
#include <stdlib.h>

void dialInit(DialState *dial, uint16_t filterFraction, uint16_t maxValue, uint16_t steps) {
    dial->acc = 0;
    dial->filtered = 0;
    dial->filterFraction = filterFraction;
    dial->maxValue = maxValue;
    dial->steps = steps;
    dial->stepValue = (0x10000 / dial->steps);
    dial->value = 0;
    dial->hasChanged = 0;
}

bool dialProcess(DialState *dial, uint16_t rawValue) {
    //first normalize value to 0-65535
    rawValue = rawValue > dial->maxValue ? dial->maxValue : rawValue;
    int32_t normalized = ((uint32_t)rawValue << 16) / dial->maxValue;

    //FIXME this didn't work super well, needs a proper filter, just use the value as is for now
    //filter the value
    // dial->acc += ((normalized - dial->acc) * dial->filterFraction) >> 16;
    // dial->filtered = dial->acc;

    dial->filtered = normalized;

    //quantize the value to steps
    const uint32_t halfStep = dial->stepValue >> 1;
    const uint32_t threeQuarters = (dial->stepValue*3) >> 2;
    const uint32_t lastValue = dial->value * dial->stepValue + halfStep; //center the value in the step
    //implement some hysterisis to keep the value from changing with noise.
    //require 3/4 of a step from center to consider the value changed
    volatile int difference = (int32_t)dial->filtered - (int32_t)lastValue;
    if (abs(difference) > threeQuarters) {
        dial->value = (dial->filtered) / dial->stepValue;
        dial->hasChanged = 1;
        return 1;
    }

    return 0;
}

uint16_t dialGetValue(DialState *dial) {
    return dial->value;
}

bool dialPollChangeEvent(DialState *dial) {
    if (dial->hasChanged) {
        dial->hasChanged = 0;
        return 1;
    }
    return 0;
}
