/*
 * sampler.h
 *
 *  Created on: Jan 14, 2024
 *      Author: benh
 */

#ifndef INC_SAMPLER_H_
#define INC_SAMPLER_H_

#include "main.h"

#include "app.h"


typedef struct {
	uint32_t sampleRate;
	volatile uint16_t sampleBuffer[SAMPLE_BUFFER_SIZE];
	int startIndex;

	volatile uint16_t snapshot[SAMPLE_BUFFER_SIZE]; //copy of the sample buffer for display
	uint32_t snapshotSampleRate;
	volatile uint8_t newSnapshotReady;
	int16_t dcOffset;
	int shift; //number of bits to shift the sample left to get a 12 bit value (in case of 8 or 6 bit sampling)

	//ADC settings to get this sampleRate
	uint32_t adcClock;
	uint32_t adcSampleTime;
	uint32_t adcResolution;
	//TODO oversample settings are all in CFGR2, maybe just use one var here along with MODIFY_REG
	uint32_t adcOversampleEnabled;
	uint32_t adcOversampleRatio;
	uint32_t adcOversampleShift;
} Sampler;


void initSampler(Sampler * s);

uint32_t setSampleRate(Sampler * s, uint32_t rate);

void stopSampler(Sampler * s);
void startSampler(Sampler * s);



int16_t getSample(Sampler * s, int index);
int16_t getInterpolatedSample(Sampler *sampler, int32_t position);

#endif /* INC_SAMPLER_H_ */
