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

typedef enum {
	SPEED_ULTRA, // div: 1, sampleTime: 1.5, resTime: 6.5     = 3,500,000 SPS
	SPEED_FAST, // div: 1, sampleTime: 3.5, resTime: 8.5      = 2,916,667 SPS
	SPEED_NORMAL, // div: 1, sampleTime: 7.5, resTime: 12.5   = 1,750,000 SPS
} SamplerSpeed;


typedef struct {
	uint32_t sampleRate;
	volatile uint16_t sampleBuffer[SAMPLE_BUFFER_SIZE];
	int startIndex;
	int16_t dcOffset;
	SamplerSpeed speed;
	int shift;
	uint8_t isLocked;

	//ADC settings
	uint32_t adcClock;
	uint32_t adcSampleTime;
	uint32_t adcResolution;
	//TODO oversample settings are all in CFGR2, maybe just use one var and mask it
	uint32_t adcOversampleEnabled;
	uint32_t adcOversampleRatio;
	uint32_t adcOversampleShift;
} Sampler;


void initSampler(Sampler * s);

uint32_t setSampleRate(Sampler * s, uint32_t rate);

void stopSampler(Sampler * s);
void startSampler(Sampler * s);



int16_t getSample(Sampler * s, int index);


#endif /* INC_SAMPLER_H_ */
