/*
 * sampler.h
 *
 *  Created on: Jan 14, 2024
 *      Author: benh
 */

#ifndef INC_SAMPLER_H_
#define INC_SAMPLER_H_

#include "main.h"


#define SAMPLE_BUFFER_SIZE 1024
#define SAMPLE_BUFFER_MASK 0x3FF

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
} Sampler;


void initSampler(Sampler * s);

uint32_t setSampleRate(Sampler * s, uint32_t rate);

void stopSampler(Sampler * s);
void startSampler(Sampler * s);



int16_t getSample(Sampler * s, int index);


#endif /* INC_SAMPLER_H_ */
