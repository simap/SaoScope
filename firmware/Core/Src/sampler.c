#include "sampler.h"
#include "string.h"

#define EFFECTIVE_RATE(D) (int) (70000000 / (D) / (7.5 + 12.5) + 0.5)

typedef struct {
	uint32_t clockSetting;
	uint32_t effectiveRate;
} AdcClockDividerSetting;

static const AdcClockDividerSetting adcClockDividerSettings[11] = {
		{LL_ADC_CLOCK_ASYNC_DIV2, EFFECTIVE_RATE(2)},
		{LL_ADC_CLOCK_ASYNC_DIV4, EFFECTIVE_RATE(4)},
		{LL_ADC_CLOCK_ASYNC_DIV6, EFFECTIVE_RATE(6)},
		{LL_ADC_CLOCK_ASYNC_DIV8, EFFECTIVE_RATE(8)},
		{LL_ADC_CLOCK_ASYNC_DIV10, EFFECTIVE_RATE(10)},
		{LL_ADC_CLOCK_ASYNC_DIV12, EFFECTIVE_RATE(12)},
		{LL_ADC_CLOCK_ASYNC_DIV16, EFFECTIVE_RATE(16)},
		{LL_ADC_CLOCK_ASYNC_DIV32, EFFECTIVE_RATE(32)},
		{LL_ADC_CLOCK_ASYNC_DIV64, EFFECTIVE_RATE(64)},
		{LL_ADC_CLOCK_ASYNC_DIV128, EFFECTIVE_RATE(128)},
		{LL_ADC_CLOCK_ASYNC_DIV256, EFFECTIVE_RATE(256)}
};



void initSampler(Sampler * sampler) {
	sampler->dcOffset = 2048;
	sampler->startIndex = 0;
	setSampleRate(sampler, 1750000);
}



uint32_t setSampleRate(Sampler *sampler, uint32_t rate) {
	uint32_t actualRate = 0;

	if (rate > 2916667) {
		//{div: 2, sampleTime: 3.5, resTime: 6.5, ns: 285.7142857142857, rate: '3,500,000'}
		actualRate = 3500000;
		sampler->speed = SPEED_ULTRA;
		sampler->shift = 6;
		sampler->adcClock = LL_ADC_CLOCK_ASYNC_DIV2;
		sampler->adcSampleTime = LL_ADC_SAMPLINGTIME_3CYCLES_5;
		sampler->adcResolution = LL_ADC_RESOLUTION_6B;
		sampler->adcOversampleEnabled = LL_ADC_OVS_DISABLE;
		sampler->adcOversampleRatio = 0;
		sampler->adcOversampleShift = 0;
	} else if (rate > 1750000) {
		actualRate = 2916667;
		sampler->speed = SPEED_FAST;
		sampler->shift = 4;
		sampler->adcClock = LL_ADC_CLOCK_ASYNC_DIV2;
		sampler->adcSampleTime = LL_ADC_SAMPLINGTIME_3CYCLES_5;
		sampler->adcResolution = LL_ADC_RESOLUTION_8B;
		sampler->adcOversampleEnabled = LL_ADC_OVS_DISABLE;
		sampler->adcOversampleRatio = 0;
		sampler->adcOversampleShift = 0;
	} else {
		sampler->speed = SPEED_NORMAL;
		sampler->shift = 0;
		sampler->adcSampleTime = LL_ADC_SAMPLINGTIME_7CYCLES_5;
		sampler->adcResolution = LL_ADC_RESOLUTION_12B;


		AdcClockDividerSetting divSetting = adcClockDividerSettings[0];

		//find the highest clock div that still has a rate above what we need
		for (int i = 1; i < 11; i++) {
			if (adcClockDividerSettings[i].effectiveRate >= rate) {
				divSetting = adcClockDividerSettings[i];
			}
		}

		sampler->adcClock = divSetting.clockSetting;
		actualRate = divSetting.effectiveRate;

		//TODO find the highest oversample div that has a rate above what we need
		// LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_GRP_REGULAR_CONTINUED); //enable oversampler (why this obtuse API?)
		// LL_ADC_ConfigOverSamplingRatioShift
		sampler->adcOversampleEnabled = LL_ADC_OVS_DISABLE;
		sampler->adcOversampleRatio = 0;
		sampler->adcOversampleShift = 0;
	}

	//TODO set holdoff such that we won't trigger while there's still junk in the buffer from old samples
	//for now zero it out so it's obvious
	memset(sampler->sampleBuffer, 0, sizeof(sampler->sampleBuffer));

	return sampler->sampleRate = actualRate;
}

void stopSampler(Sampler *sampler) {
	if (!LL_ADC_REG_IsConversionOngoing(ADC1))
		return;
	LL_ADC_REG_StopConversion(ADC1);
	while (LL_ADC_REG_IsStopConversionOngoing(ADC1) || LL_ADC_REG_IsConversionOngoing(ADC1))
		;
	sampler->startIndex = 1024 - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_1) ;
}

void startSampler(Sampler *sampler) {
	if (!LL_ADC_REG_IsConversionOngoing(ADC1))
		LL_ADC_REG_StartConversion(ADC1);
}

int16_t getSample(Sampler *sampler, int index) {
	int si = (sampler->startIndex + index) & SAMPLE_BUFFER_MASK;
	return (sampler->sampleBuffer[si] << sampler->shift) - sampler->dcOffset;
}
