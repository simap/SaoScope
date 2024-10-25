#include "main.h"
#include "app.h"
#include "sampler.h"
#include "button.h"
#include "dial.h"

// about 10 times a second, pause the sampler ADC scanning and scan the dials and vrefint (aka extras)
// we can listen for the dma transfer complete interrupt and check the time since last scanning dials


extern Sampler sampler;
extern ButtonState button1, button2, button3, button4;
extern DialState dial1, dial2, dial3, dial4;
extern uint32_t vdd;

enum {
	SCANNING_SCOPE,
	SCANNING_EXTRAS
} scanningMode = SCANNING_SCOPE;

volatile uint16_t extrasSamples[5]; 
volatile uint8_t extrasSamplesReady = 0;

void adcStopAndDisable() {
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
		while (LL_ADC_REG_IsStopConversionOngoing(ADC1) || LL_ADC_REG_IsConversionOngoing(ADC1))
			;
	}

	if (LL_ADC_IsEnabled(ADC1)) {
		LL_ADC_Disable(ADC1);
		while (LL_ADC_IsEnabled(ADC1))
			;
	}
}

void adcEnableAndStart() {
    LL_ADC_ClearFlag_ADRDY(ADC1);
	LL_ADC_Enable(ADC1);
	while(!LL_ADC_IsActiveFlag_ADRDY(ADC1))
		;

	LL_ADC_REG_StartConversion(ADC1);
}


void startScanningExtras() {
    adcStopAndDisable();

    //sample the 4 dials and vrefint
    LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_0 | LL_ADC_CHANNEL_2 | LL_ADC_CHANNEL_3 | LL_ADC_CHANNEL_6 | LL_ADC_CHANNEL_VREFINT);

    //switch dma to one shot through the extras sample, not continuous
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_LIMITED);
    //point dma to the extras buffer
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) &extrasSamples);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 5);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    //vrefint requires 4us sampling time, at 35mhz, 140 cycles
    //10k impedance dials need 19.5 to 39.5 sample cycles time (for 12 bit resolution at 35mhz)
    //TODO maybe sample the dials then come back for vrefint so we don't waste time, though that will be more complex and waste cycles interrupting. vrefint needn't be sampled this frequently
    //sampling 5 channels will take (160.5 + 12.5) * 5 = 865 cycles. at 35mhz, 24.7us
    //sampling 4 channels at 39.5 sample time would take (39.5 + 12.5) * 4 = 204 cycles. at 35mhz, 5.8us
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_160CYCLES_5);
    LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
    LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
    LL_ADC_SetCommonClock(ADC1_COMMON, LL_ADC_CLOCK_ASYNC_DIV2);

    //CCRDY handshake requires 1APB + 2 ADC + 3 APB cycles after the channel configuration has been changed.
    //if CCRDY isn't set when the adc is enabled it will ignore changes
    //TODO since this happens much later than changing these settings, we could probably skip this
    while (!LL_ADC_IsActiveFlag_CCRDY(ADC1))
        ;
    LL_ADC_ClearFlag_CCRDY(ADC1);

    adcEnableAndStart();
}

void startScanningScope() {
    adcStopAndDisable();

    LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_1);

    //switch dma to continuous
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_CONTINUOUS);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);
    //point dma to the sampler buffer
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) &sampler.sampleBuffer);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, SAMPLE_BUFFER_SIZE);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, sampler.adcSampleTime);
    LL_ADC_SetResolution(ADC1, sampler.adcResolution);
    LL_ADC_SetOverSamplingScope(ADC1, sampler.adcOversampleEnabled ? LL_ADC_OVS_GRP_REGULAR_CONTINUED : LL_ADC_OVS_DISABLE);
    LL_ADC_ConfigOverSamplingRatioShift(ADC1, sampler.adcOversampleRatio, sampler.adcOversampleShift);
    LL_ADC_SetCommonClock(ADC1_COMMON, sampler.adcClock);

    //CCRDY handshake requires 1APB + 2 ADC + 3 APB cycles after the channel configuration has been changed.
    //if CCRDY isn't set when the adc is enabled it will ignore changes
    //TODO since this happens much later than changing these settings, we could probably skip this
    while (!LL_ADC_IsActiveFlag_CCRDY(ADC1))
        ;
    LL_ADC_ClearFlag_CCRDY(ADC1);

    adcEnableAndStart();
}


void adcManagerDmaISR() {
    static uint32_t lastExtrasScan = 0;
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        
        switch (scanningMode) {
            case SCANNING_SCOPE:
                //see if we should switch to scanning dials
                if (getTicks() - lastExtrasScan >= INTERFACE_SCAN_INTERVAL_MS) {
                    scanningMode = SCANNING_EXTRAS;
                    lastExtrasScan = getTicks();

                    startScanningExtras();
                }
                break;
            case SCANNING_EXTRAS:
                //process the dial samples later, go back to scope scanning
                extrasSamplesReady = 1;
                scanningMode = SCANNING_SCOPE;
                startScanningScope();
                break;
        }
    }
}

void adcManagerSetup() {
    //perform ADC calibration
	LL_ADC_StartCalibration(ADC1);
	while (LL_ADC_IsCalibrationOnGoing(ADC1))
		;

    //this stays the same for all ADC DMA transfers
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) &ADC1->DR);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);

    scanningMode = SCANNING_EXTRAS;
    startScanningExtras();

}

void adcManagerLoop() {
    //see if dial scans are ready, and if so, process the samples
    //if run buttonProcess for each button
    if (extrasSamplesReady) {
        extrasSamplesReady = 0;
        vdd = __LL_ADC_CALC_VREFANALOG_VOLTAGE(extrasSamples[4], LL_ADC_RESOLUTION_12B);

        //process buttons. if the reading is very low (under 10%) then the button is pressed, otherwise the dial reading is processed
        buttonProcess(&button1, extrasSamples[0] < 200);
        buttonProcess(&button2, extrasSamples[1] < 200);
        buttonProcess(&button3, extrasSamples[2] < 200);
        buttonProcess(&button4, extrasSamples[3] < 200);

        //process dials unless sample indicates button is pressed
        const int offset = 320; //the 1k resistors added to the pot mean the lowest value we can read is roughly 4096/12 = 341. figure on 5% + 1% + 1% of resistor error, so 320 is a good threshold
        if (extrasSamples[0] >= offset)
            dialProcess(&dial1, extrasSamples[0] - offset);
        if (extrasSamples[1] >= offset)
            dialProcess(&dial2, extrasSamples[1] - offset);
        if (extrasSamples[2] >= offset)
            dialProcess(&dial3, extrasSamples[2] - offset);
        if (extrasSamples[3] >= offset)
            dialProcess(&dial4, extrasSamples[3] - offset);
        
    }
}

