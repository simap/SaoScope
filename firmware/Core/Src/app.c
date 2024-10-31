/*
 * app.c
 *
 *  Created on: Dec 7, 2022
 *      Author: benh
 */

#include "main.h"
#include "app.h"

#include "ssd1306.h"
#include "ssd1306_tests.h"

#include "sampler.h"
#include "scope.h"
#include "button.h"
#include "dial.h"

#include "stdio.h"

Sampler sampler;
ScopeSettings scope;
ButtonState modeButton, runButton, edgeButton, signalButton;
DialState vposDial, vdivDial, tdivDial, triggerDial;
volatile uint32_t ticks;
uint32_t vdd;


uint32_t getCycles() {
	return SysTick->VAL;
}

void systickISR() {
	ticks++;
	adcManagerSystickISR();
	// if (ticks & 1) {
		//processing 3 buttons takes about 312 cycles
		// uint32_t gpioState = GPIOA->IDR;
		// buttonProcess(&button1, gpioState & BUTTON1_Pin); //button1 is active high
		// buttonProcess(&button2, !(gpioState & BUTTON2_Pin)); //button2 is active low
		// buttonProcess(&button3, !(gpioState & BUTTON3_Pin)); //button3 is active low
	// }

//	if (ticks & 1) {
//		LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_7);
//	} else {
//		LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_7);
//	}

	if ((ticks & 31) == 31) {
		//TODO only do this if we aren't in a trigger capture event

	}

}




uint32_t getTicks() {
	return ticks;
}

void delay(uint32_t ms) {
	uint32_t timer = ticks;
	while (ticks - timer < ms) {
		//wait
	}
}

void delayCycles(uint32_t c) {
	uint32_t timer = SysTick->VAL;
	while (SysTick->VAL - timer < c) {
		//wait
	}
}

volatile int rateSwitch = 0;

void run() {

	scope.mode = CONTINUOUS;
	scope.runMode = STOPPED;


	buttonInit(&modeButton, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);
	buttonInit(&runButton, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);
	buttonInit(&edgeButton, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);
	buttonInit(&signalButton, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);

	dialInit(&vposDial, 0x8000, 3500, 100);
	dialInit(&vdivDial, 0x8000, 3500, 100);
	dialInit(&tdivDial, 0x8000, 3500, 100);
	dialInit(&triggerDial, 0x8000, 3500, 100);


	LL_SYSTICK_EnableIT();

	delay(10);

	initSampler(&sampler);
	adcManagerSetup();

	//set up TIM14 signal generation on PA7
	LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_7, LL_GPIO_SPEED_FREQ_VERY_HIGH);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF_4); //set to TIM14_CH1

	LL_TIM_OC_SetMode(TIM14, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);

	int signalFreq = 20000;
	int cycles = SystemCoreClock / signalFreq;

	LL_TIM_SetAutoReload(TIM14, cycles-1);
	LL_TIM_OC_SetCompareCH1(TIM14, cycles / 2);
	LL_TIM_GenerateEvent_UPDATE(TIM14);

	LL_TIM_EnableAllOutputs(TIM14);
	LL_TIM_CC_EnableChannel(TIM14, LL_TIM_CHANNEL_CH1);
	LL_TIM_EnableCounter(TIM14);


	//HACK speed up i2c, well above spec, but it works. about 1.25mhz
	LL_I2C_Disable(I2C1);
	LL_I2C_SetTiming(I2C1, __LL_I2C_CONVERT_TIMINGS(
			0, //PRESCALER
			1, //SETUP_TIME
			0, //HOLD_TIME
			6, //SCLH_PERIOD
			12 //SCLL_PERIOD
	));
	LL_I2C_Enable(I2C1);


	ssd1306_Init();
	ssd1306_SetContrast(0xff);


    int frame = 0;
    uint32_t frameTimer = ticks;
    volatile int fps = 0;
    int pwm;
	uint32_t rate = 1750000;

	SSD1306_COLOR flickering = White;

    for(;;) {
		adcManagerLoop();
//		ssd1306_TestFPS();

    	if (ticks - frameTimer >= 1000) {
    		fps = frame;
    		frameTimer += 1000;
    		frame = 0;
    	}

		if (dialPollChangeEvent(&tdivDial)) {
			uint32_t tmp = dialGetValue(&tdivDial);
			tmp = (tmp * tmp)/95;
			rate = tmp * 35000;
			rate = setSampleRate(&sampler, rate);
		}

		int vdiv = dialGetValue(&vdivDial);
		int vpos = dialGetValue(&vposDial);
		int triggerValue = dialGetValue(&triggerDial) * 40 - 2048;

		if (buttonPollClickEvent(&runButton)) {
			scope.runMode = scope.runMode == STOPPED ? RUNNING : STOPPED;
		}

		if (buttonPollClickEvent(&modeButton)) {
			int next = scope.mode + 1;
			if (next > SINGLE)
				next = CONTINUOUS;
			scope.mode = next;
		}

		if (buttonPollClickEvent(&edgeButton)) {
			scope.triggerSlope = scope.triggerSlope == RISING ? FALLING : RISING;
		}

		//draw scope

        ssd1306_Fill(Black);

		int32_t minVoltage = INT32_MAX;
		int32_t maxVoltage = INT32_MIN;


		//find first rising edge

		int start = -1;
		for (int x = SAMPLE_BUFFER_SIZE/2 - 5; x < SAMPLE_BUFFER_SIZE; x++) {
			int sample = getSample(&sampler, x);
			if (start == -1 && sample < triggerValue) {
				start = x;
			}

			if (start >= 0 && sample > triggerValue) {
				start = x;
				break;
			}
		}

		if (start < 64 || start > 800)
			start = 64;

		start -= 64;



		SSD1306_COLOR blink = (getTicks() & 0x80) ? White : Black;
		flickering = (getTicks() & 0x20) ? White : Black;
		for (int x = 0; x < SSD1306_WIDTH; x++) {
			int sample = getSample(&sampler, x + start);
			int mv = 15000 * sample / 2048;

			if (mv > maxVoltage)
				maxVoltage = mv;
			if (mv < minVoltage)
				minVoltage = mv;

			int y = vpos - sample/vdiv;

//				int y = 31 - sample/100;
			SSD1306_COLOR color = White;
			if (y > 63) {
				y = 63;
				color = blink;
			} else if (y < 0) {
				y = 0;
				color = blink;
			}
			ssd1306_DrawPixel(x, y , color);
		}

		//draw trigger threshold line
		for (int x = 0; x < SSD1306_WIDTH; x += 16) {
			ssd1306_DrawPixel(x, vpos - triggerValue/vdiv, flickering);
		}

		//draw center line showing trigger time
		for (int y = 0; y < 32; y += 8) {
			ssd1306_DrawPixel(64, y, flickering);
		}



    	// if (run) {
    	// 	startSampler(&sampler);
    	// }

		char buff[64];

		const int yStart = 32;
		// ssd1306_FillRectangle(0, 15 + yStart, 127, 32 + yStart, White);
		int Vp = (maxVoltage - minVoltage)/10;
		int VpVolts = Vp/100;
		int VpHv = (Vp - VpVolts*100);
		// snprintf(buff, sizeof(buff), "Vp: %d.%02d", VpVolts, VpHv);
		// ssd1306_SetCursor(1, 16 + yStart);
		// ssd1306_WriteString(buff, Font_6x8, Black);
		
		// snprintf(buff, sizeof(buff), "FPS: %d ", fps);
		// ssd1306_SetCursor(1, 24 + yStart);
		// ssd1306_WriteString(buff, Font_6x8, White);

		snprintf(buff, sizeof(buff), "V: %dmv %dsps", vdd, rate);
		ssd1306_SetCursor(1, 16 + yStart);
		ssd1306_WriteString(buff, Font_6x8, White);

		// snprintf(buff, sizeof(buff), "%d, %d, %d, %d ", dialGetValue(&vposDial), dialGetValue(&vdivDial), dialGetValue(&tdivDial), dialGetValue(&triggerDial));
		// ssd1306_SetCursor(1, 24 + yStart);
		// ssd1306_WriteString(buff, Font_6x8, White);

		// ssd1306_WriteString(modeButton.isHeld ? "H" : modeButton.isDown ? "O" : "-", Font_6x8, White);
		// ssd1306_WriteString(runButton.isHeld ? "H" : runButton.isDown ? "O" : "-", Font_6x8, White);
		// ssd1306_WriteString(edgeButton.isHeld ? "H" : edgeButton.isDown ? "O" : "-", Font_6x8, White);
		// ssd1306_WriteString(signalButton.isHeld ? "H" : signalButton.isDown ? "O" : "-", Font_6x8, White);


		ssd1306_SetCursor(1, 24 + yStart);

		const char * modeStr = scope.mode == CONTINUOUS ? "SCAN" : scope.mode == NORMAL ? "NORM" : "SING";
		const char * runStr = scope.runMode == STOPPED ? "S" : "R";
		const char * edgeStr = scope.triggerSlope == RISING ? "_/" : "\\_";

		ssd1306_WriteString(modeStr, Font_6x8, White);
		ssd1306_WriteString(" ", Font_6x8, White);
		ssd1306_WriteString(runStr, Font_6x8, White);
		ssd1306_WriteString(" ", Font_6x8, White);

		//pad leading zeros for 5 digits
		snprintf(buff, sizeof(buff), "~% 5d ", signalFreq);
		ssd1306_WriteString(buff, Font_6x8, White);
		ssd1306_WriteString(edgeStr, Font_6x8, White);


		// snprintf(buff, sizeof(buff), "%d, %d, %d, %d ", dialGetValue(&vposDial), dialGetValue(&vdivDial), dialGetValue(&tdivDial), dialGetValue(&triggerDial));
		// ssd1306_WriteString(buff, Font_6x8, White);

		// ssd1306_WriteString(modeButton.isHeld ? "H" : modeButton.isDown ? "O" : "-", Font_6x8, White);
		// ssd1306_WriteString(runButton.isHeld ? "H" : runButton.isDown ? "O" : "-", Font_6x8, White);
		// ssd1306_WriteString(edgeButton.isHeld ? "H" : edgeButton.isDown ? "O" : "-", Font_6x8, White);
		// ssd1306_WriteString(signalButton.isHeld ? "H" : signalButton.isDown ? "O" : "-", Font_6x8, White);



        ssd1306_UpdateScreen();

        frame++;
    }
}
