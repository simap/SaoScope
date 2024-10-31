/*
 * scope.h
 *
 *  Created on: Jan 12, 2024
 *      Author: benh
 */

#ifndef INC_SCOPE_H_
#define INC_SCOPE_H_


#include <stdint.h>


typedef enum {
	CONTINUOUS, // Continuous signal display
	NORMAL, // Update on trigger events
	SINGLE, // Stop after first trigger event
	ROLL //  The waveform moves continuously across the screen from right to left
} ScopeMode;

typedef enum {
	RUNNING,
	STOPPED
} RunMode;

typedef enum {
    RISING,
    FALLING
} TriggerEdge;

typedef enum {
	LOGIC,      //use the digital "logic" input
    CHANNEL_1   //use the scope channel (with a threshold)
} TriggerSource;


typedef struct {
	uint32_t YDivUv;        // Vertical scale in microvolts/div
	int32_t YOffsetUv;      // Vertical offset in microvolts
} ChannelSettings;


typedef enum {
	SAMPLE, PEAK, AVERAGE
} SampleMode;


typedef enum {
    DOTS, VECTOR
} DrawMode;


//we have 5 32-bit backup registers. Some scope settings should remain during power down.
//UI mode stuff wouldn't need to, but vdiv/tdiv/trigger stuff should

typedef struct {
	ScopeMode mode;
	RunMode runMode;

	uint32_t XDivNs;   // Horizontal scale in ns/div
	// int32_t XOffsetNs; // Horizontal offset in ns

	ChannelSettings channel1;

	// TriggerSource triggerSource;
	TriggerEdge triggerSlope;
	int32_t triggerLevelUv; // For analog sources, the voltage threshold in microvolts
	// uint32_t triggerHoldoffUs; // microseconds to ignore trigger sources for this long

	SampleMode sampleMode;
	DrawMode drawMode;

	int32_t triggerIndex;

	//analysis data
	int32_t minVoltage;
	int32_t maxVoltage;
	//maybe add frequency, etc
} ScopeSettings;


#endif /* INC_SCOPE_H_ */
