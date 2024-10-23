#include "sampler.h"


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
	memset(sampler->sampleBuffer, 0, sizeof(sampler->sampleBuffer));
	sampler->dcOffset = 2048;
	sampler->startIndex = 0;


	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) sampler->sampleBuffer);
	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) &ADC1->DR);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, SAMPLE_BUFFER_SIZE);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
}



uint32_t setSampleRate(Sampler *sampler, uint32_t rate) {


	/*
	Follow this procedure to disable the ADC:
	1. Check that ADSTART = 0 in the ADC_CR register to ensure that no conversion is
	ongoing. If required, stop any ongoing conversion by writing 1 to the ADSTP bit in the
	ADC_CR register and waiting until this bit is read at 0.
	2. Set ADDIS = 1 in the ADC_CR register.
	3. If required by the application, wait until ADEN = 0 in the ADC_CR register, indicating
	that the ADC is fully disabled (ADDIS is automatically reset once ADEN = 0).
	4. Clear the ADRDY bit in ADC_ISR register by programming this bit to 1 (optional).
	 */

	if (LL_ADC_REG_IsConversionOngoing(ADC1)) {
		LL_ADC_REG_StopConversion(ADC1);
		while (LL_ADC_REG_IsStopConversionOngoing(ADC) || LL_ADC_REG_IsConversionOngoing(ADC1))
			;
	}

	if (LL_ADC_IsEnabled(ADC1)) {
		LL_ADC_Disable(ADC1);
		while (LL_ADC_IsEnabled(ADC1))
			;
	}

	uint32_t actualRate = 0;

	//make changes
	if (rate > 2916667) {
		//{div: 2, sampleTime: 3.5, resTime: 6.5, ns: 285.7142857142857, rate: '3,500,000'}
		LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_3CYCLES_5);
		LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_6B);
		LL_ADC_SetCommonClock(ADC1_COMMON, LL_ADC_CLOCK_ASYNC_DIV2);
		LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
		actualRate = 3500000;
		sampler->speed = SPEED_ULTRA;
		sampler->shift = 6;
	} else if (rate > 1750000) {
		LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_3CYCLES_5);
		LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_8B);
		LL_ADC_SetCommonClock(ADC1_COMMON, LL_ADC_CLOCK_ASYNC_DIV2);
		LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
		actualRate = 2916667;
		sampler->speed = SPEED_FAST;
		sampler->shift = 4;
	} else {
		LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_7CYCLES_5);
		LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
		LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
		sampler->speed = SPEED_NORMAL;
		sampler->shift = 0;

		AdcClockDividerSetting divSetting = adcClockDividerSettings[0];

		//find the highest clock div that still has a rate above what we need
		for (int i = 1; i < 11; i++) {
			if (adcClockDividerSettings[i].effectiveRate >= rate) {
				divSetting = adcClockDividerSettings[i];
			}
		}

		LL_ADC_SetCommonClock(ADC1_COMMON, divSetting.clockSetting);
		actualRate = divSetting.effectiveRate;

		//TODO find the highest oversample div that has a rate above what we need
//		LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_GRP_REGULAR_CONTINUED); //enable oversampler (why this obtuse API?)
//		LL_ADC_ConfigOverSamplingRatioShift
	}

	//TODO set holdoff such that we won't trigger while there's still junk in the buffer from old samples
	//for now zero it out so it's obvious
	memset(sampler->sampleBuffer, 0, sizeof(sampler->sampleBuffer));

	/*
	Follow this procedure to enable the ADC:
	1. Clear the ADRDY bit in ADC_ISR register by programming this bit to 1.
	2. Set ADEN = 1 in the ADC_CR register.
	3. Wait until ADRDY = 1 in the ADC_ISR register (ADRDY is set after the ADC startup
	time). This can be handled by interrupt if the interrupt is enabled by setting the
	ADRDYIE bit in the ADC_IER register.
	*/


	LL_ADC_ClearFlag_ADRDY(ADC1);
	LL_ADC_Enable(ADC1);
	while(!LL_ADC_IsActiveFlag_ADRDY(ADC1))
		;

	LL_ADC_REG_StartConversion(ADC1);

	return sampler->sampleRate = actualRate;
}

void stopSampler(Sampler *sampler) {
	if (!LL_ADC_REG_IsConversionOngoing(ADC1))
		return;
	LL_ADC_REG_StopConversion(ADC1);
	while (LL_ADC_REG_IsStopConversionOngoing(ADC) || LL_ADC_REG_IsConversionOngoing(ADC1))
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
