#include "main.h"
#include "app.h"
#include "sampler.h"
#include "button.h"
#include "dial.h"
#include "scope.h"

// several times a second, pause the sampler ADC scanning and scan the dials and vrefint (aka extras)
// we can listen for the dma transfer complete interrupt and check the time since last scanning dials
// if scope is running, go back to scanning for the scope
// if scope is stopped, we scan extras periodically and go to an idle state

extern Sampler sampler;
extern ButtonState modeButton, runButton, edgeButton, signalButton;
extern DialState vposDial, vdivDial, tdivDial, triggerDial;
extern uint32_t vdd;
extern ScopeSettings scope;

TriggerEdge lastEdge = RISING;

/*
prebuffer -> free running circular ADC. set a timer to allow roughly half of the buffer to fill
prearm -> set ADC WD for the level opposite trigger edge (e.g. high for falling edge)
    for continuous, also set a timer that will jump to capturing state if it hasn't happened yet
armed -> set ADC WD for the trigger (e.g. low for falling edge), link to tim1 to start completion timer automatically
capturing -> set tim1 to capture half of the samples after the trigger
complete -> copy samples to snapshot (dma mem2mem or memcpy)
    for one-shot stop, all others repeat
    */

enum {
    AM_BUFFERING_START,
    AM_PREARM,
    AM_ARMED,
    AM_CAPTURING,
    AM_COMPLETE,
    AM_SCANNING_EXTRAS,
    AM_IDLE



	// SCANNING_SCOPE,
	// SCANNING_EXTRAS,
    // SCANNING_TRIGGERED, //saw trigger event, capturing the following samples
    // SCANNING_IDLE //not scanning anything, scope is paused
} scanningMode = AM_IDLE;

volatile uint16_t extrasSamples[5]; 
volatile uint8_t extrasSamplesReady = 0;
uint32_t lastExtrasScan = 0;

void setTim1OneShot(volatile uint32_t period) {
    //change prescaler as needed to get around the desired period in clock cycles
    int prescaler = period >> 16;
    if (prescaler > 0) {
        period = period / (prescaler + 1);
    }
    LL_TIM_SetPrescaler(TIM1, prescaler);
    LL_TIM_SetAutoReload(TIM1, period);
    LL_TIM_SetOnePulseMode(TIM1, LL_TIM_ONEPULSEMODE_SINGLE);
    //disable interrupt
    LL_TIM_DisableIT_UPDATE(TIM1);
    LL_TIM_GenerateEvent_UPDATE(TIM1);
    //clear the update flag and enable interrupt
    LL_TIM_ClearFlag_UPDATE(TIM1);
    LL_TIM_EnableIT_UPDATE(TIM1);

    LL_TIM_EnableCounter(TIM1);
}

uint32_t calcCyclesToFillBufferToHalf() {
    uint32_t rate = sampler.sampleRate;
    const int samples = SAMPLE_BUFFER_SIZE / 2;
    //trick: shift up intermediate calculations to get a more accurate result in case of non whole number division
    //e.g. (int)(56mhz / 2916667) * 512 = 9728
    //but 56mhz << 4 = 896mhz, 896mhz / 2916667 = 307.2, (307 * 512) >> 4 = 9824
    return (((SystemCoreClock << 4) / rate) * samples) >> 4;
}

int32_t uvToAdc(int32_t uv) {
    //adc values 0-4095 represent input voltages of -15V to +15V
    
    // 30v / 4096 * 1000000mv = 7324.21875 microvolts per adc step

    //7324.21875 becomes a whole number if we multiply by 32 (or shift left by 5)
    //magic number: 234375 = 7324.21875 * 32
    //we have enough bits to multiply by 32 without overflow first

    return ((uv << 5) / 234375) + 2047;
}

void setWdLow() {
    LL_ADC_ConfigAnalogWDThresholds(ADC1, LL_ADC_AWD1, 4095, uvToAdc(scope.triggerLevelUv));
    LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, LL_ADC_AWD_CHANNEL_1_REG);
    LL_ADC_ClearFlag_AWD1(ADC1);
    LL_ADC_EnableIT_AWD1(ADC1);
}

void setWdHigh() {
    LL_ADC_ConfigAnalogWDThresholds(ADC1, LL_ADC_AWD1, uvToAdc(scope.triggerLevelUv), 0);
    LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, LL_ADC_AWD_CHANNEL_1_REG);
    LL_ADC_ClearFlag_AWD1(ADC1);
    LL_ADC_EnableIT_AWD1(ADC1);
}

void disableWd() {
    LL_ADC_DisableIT_AWD1(ADC1);
    LL_ADC_ClearFlag_AWD1(ADC1);
    LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, LL_ADC_AWD_DISABLE);
}


void adcStopAndDisable() {
    /* FTM:
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
    scanningMode = AM_SCANNING_EXTRAS;
    lastExtrasScan = getTicks();

    adcStopAndDisable();

    disableWd();

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
    scanningMode = AM_BUFFERING_START;

    adcStopAndDisable();

    LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_1);

    //disable WD for now
    LL_ADC_DisableIT_AWD1(ADC1);
    LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, LL_ADC_AWD_DISABLE);

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




    //FTM: CCRDY handshake requires 1APB + 2 ADC + 3 APB cycles after the channel configuration has been changed.
    //if CCRDY isn't set when the adc is enabled it will ignore changes
    //TODO since this happens much later than changing these settings, we could probably skip this
    while (!LL_ADC_IsActiveFlag_CCRDY(ADC1))
        ;
    LL_ADC_ClearFlag_CCRDY(ADC1);
    
    adcEnableAndStart();

    setTim1OneShot(calcCyclesToFillBufferToHalf());
}

void stopScanning() {
    adcStopAndDisable();
    LL_DMA_ClearFlag_TC1(DMA1);
    scanningMode = AM_IDLE;
}

void startPrearm() {
    scanningMode = AM_PREARM;
    //set ADC WD for the level opposite trigger edge (e.g. high for falling edge)
    if (scope.triggerSlope == RISING) {
        setWdLow();
    } else {
        setWdHigh();
    }
    if (scope.mode == CONTINUOUS) {
        //TODO probably need a longer timeout to let more time waiting for a trigger. 
        setTim1OneShot(calcCyclesToFillBufferToHalf());
    }
}

void startArm() {
    scanningMode = AM_ARMED;
    //set ADC WD for the trigger (e.g. low for falling edge)
    if (scope.triggerSlope == RISING) {
        setWdHigh();
    } else {
        setWdLow();
    }
}

void startCapturing() {
    scanningMode = AM_CAPTURING;
    //set tim1 to capture half of the samples after the trigger
    setTim1OneShot(calcCyclesToFillBufferToHalf());
    disableWd();
}

void completeAndSnapshot() {
    scanningMode = AM_COMPLETE;
    //stop the adc so memory can be copied
    adcStopAndDisable();
    sampler.startIndex = SAMPLE_BUFFER_SIZE - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_1) ;
    // //copy the samples to the snapshot, reordering from the circular buffer
    // memcpy(sampler.snapshot, &sampler.sampleBuffer[sampler.startIndex], (SAMPLE_BUFFER_SIZE - sampler.startIndex) * sizeof(uint16_t));
    // if (sampler.startIndex > 0)
    //     memcpy(&sampler.snapshot[SAMPLE_BUFFER_SIZE - sampler.startIndex], sampler.sampleBuffer, sampler.startIndex * sizeof(uint16_t));

    sampler.newSnapshotReady = 1;

    if (scope.mode == SINGLE) {
        scope.runMode = STOPPED;
    }

    //see if we should switch to scanning dials
    if (getTicks() - lastExtrasScan >= INTERFACE_SCAN_INTERVAL_MS) {
        startScanningExtras();
    } else {
        stopScanning();
    }
}

//scans extras when scanning mode is idle, when scope is stopped
void adcManagerSystickISR() {
    //if idle, check to see if we should scan extras, otherwise let dma isr handle it
    if (scanningMode == AM_IDLE) {
       if (getTicks() - lastExtrasScan >= INTERFACE_SCAN_INTERVAL_MS) {
            startScanningExtras();
        }
    }
}

void ADC1_IRQHandler() {
    if (LL_ADC_IsActiveFlag_AWD1(ADC1)) {
        LL_ADC_ClearFlag_AWD1(ADC1);
        switch (scanningMode) {
            case AM_PREARM:
                startArm();
                break;
            case AM_ARMED:
                startCapturing();
                break;
            case AM_CAPTURING:
            case AM_COMPLETE:
            case AM_SCANNING_EXTRAS:
            case AM_IDLE:
            default:
                //shouldn't happen
                // HardFault_Handler();
                break;
        }
    }
}

//switches between scanning extras and scanning scope when running
void adcManagerDmaISR() {
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        
        switch (scanningMode) {
            //there's a few states where its probably OK to switch to scanning extras for a little bit
            //these are states that are waiting for something to happen and could take a long time
            case AM_PREARM:
            case AM_ARMED:
                //skip if the scope is continuous mode, since that will complete soon enough
                if (scope.mode == CONTINUOUS) {
                    break;
                }
                //see if we should switch to scanning dials
                //TODO its possible that for high speed scans we have a few samples aready, we aught to save those and resume later
                //this will restart the whole process later
                if (getTicks() - lastExtrasScan >= INTERFACE_SCAN_INTERVAL_MS) {
                    startScanningExtras();
                }
                break;
            case AM_SCANNING_EXTRAS:
                //process the dial samples later, go back to scope scanning
                extrasSamplesReady = 1;
                if (scope.runMode == RUNNING && !sampler.newSnapshotReady) {
                    startScanningScope();
                } else {
                    stopScanning();
                }
                break;
            case AM_CAPTURING:
                //norhing to do, circular buffer is filling
                break;
            case AM_COMPLETE:
            case AM_IDLE:
            default:
                //shouldn't happen
                // HardFault_Handler();
                break;

        }
    }
}

//TODO handle adc wd interrupts


void TIM1_BRK_UP_TRG_COM_IRQHandler() {

    if (LL_TIM_IsActiveFlag_UPDATE(TIM1)) {
        LL_TIM_ClearFlag_UPDATE(TIM1);
        switch (scanningMode) {
            case AM_BUFFERING_START:
                startPrearm();
                break;
            case AM_PREARM:
            case AM_ARMED:
                if (scope.mode == CONTINUOUS)
                    startCapturing();
                break;
            case AM_CAPTURING:
                completeAndSnapshot();
                break;
            case AM_COMPLETE:
            case AM_SCANNING_EXTRAS:
            case AM_IDLE:
            default:
                //shouldn't happen
                HardFault_Handler();
                break;
        }
    }
}

void adcManagerSetup() {
    //enable tim1 irq
    //first stop the timer and clear pending interrupts
    LL_TIM_DisableCounter(TIM1);
    LL_TIM_ClearFlag_UPDATE(TIM1);
    NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 0);
    NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);

    //enable adc irq for watchdog
    NVIC_SetPriority(ADC1_IRQn, 0);
    NVIC_EnableIRQ(ADC1_IRQn);

    //perform ADC calibration
	LL_ADC_StartCalibration(ADC1);
	while (LL_ADC_IsCalibrationOnGoing(ADC1))
		;

    //this stays the same for all ADC DMA transfers
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) &ADC1->DR);
    //listen for transfer complete so we can switch between scanning dials and scope if needed
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);

    startScanningExtras();

}

void adcManagerLoop() {
    if (sampler.newSnapshotReady) {
        //copy the samples to the snapshot, reordering from the circular buffer
        memcpy(sampler.snapshot, &sampler.sampleBuffer[sampler.startIndex], (SAMPLE_BUFFER_SIZE - sampler.startIndex) * sizeof(uint16_t));
        if (sampler.startIndex > 0)
            memcpy(&sampler.snapshot[SAMPLE_BUFFER_SIZE - sampler.startIndex], sampler.sampleBuffer, sampler.startIndex * sizeof(uint16_t));
        sampler.snapshotSampleRate = sampler.sampleRate;
        sampler.newSnapshotReady = 0;
    }
    //TODO not sure if this belongs here, maybe move to something more about UI
    //see if dial scans are ready, and if so, process the samples
    //if run buttonProcess for each button
    if (extrasSamplesReady) {
        extrasSamplesReady = 0;
        vdd = __LL_ADC_CALC_VREFANALOG_VOLTAGE(extrasSamples[4], LL_ADC_RESOLUTION_12B);

        //process buttons. if the reading is very low (under 10%) then the button is pressed, otherwise the dial reading is processed
        buttonProcess(&modeButton, extrasSamples[0] < 200);
        buttonProcess(&runButton, extrasSamples[1] < 200);
        buttonProcess(&edgeButton, extrasSamples[2] < 200);
        buttonProcess(&signalButton, extrasSamples[3] < 200);

        //process dials unless sample indicates button is pressed
        const int offset = 320; //the 1k resistors added to the pot mean the lowest value we can read is roughly 4096/12 = 341. figure on 5% + 1% + 1% of resistor error, so 320 is a good threshold
        if (extrasSamples[0] >= offset)
            dialProcess(&vposDial, extrasSamples[0] - offset);
        if (extrasSamples[1] >= offset)
            dialProcess(&vdivDial, extrasSamples[1] - offset);
        if (extrasSamples[2] >= offset)
            dialProcess(&tdivDial, extrasSamples[2] - offset);
        if (extrasSamples[3] >= offset)
            dialProcess(&triggerDial, extrasSamples[3] - offset);
        
    }
}

