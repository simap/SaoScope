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
#include "button.h"
#include "dial.h"

#include "stdio.h"

Sampler sampler;
ButtonState button1, button2, button3, button4;
DialState dial1, dial2, dial3, dial4;
volatile uint32_t ticks;
uint32_t vdd;


uint32_t getCycles() {
	return SysTick->VAL;
}

void systickISR() {
	ticks++;
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
	buttonInit(&button1, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);
	buttonInit(&button2, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);
	buttonInit(&button3, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);
	buttonInit(&button4, BUTTON_DEBOUNCE_COUNT, BUTTON_HOLD_COUNT);

	dialInit(&dial1, 0x8000, 3500, 100);
	dialInit(&dial2, 0x8000, 3500, 100);
	dialInit(&dial3, 0x8000, 3500, 100);
	dialInit(&dial4, 0x8000, 3500, 100);


	LL_SYSTICK_EnableIT();

	delay(10);

	initSampler(&sampler);
	adcManagerSetup();

	//set up TIM17 signal generation on PA7
	LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_7, LL_GPIO_SPEED_FREQ_VERY_HIGH);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF_5); //set to TIM17_CH1

	LL_TIM_OC_SetMode(TIM17, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);

	int cycles = SystemCoreClock / 20000 ;

	LL_TIM_SetAutoReload(TIM17, cycles-1);
	LL_TIM_OC_SetCompareCH1(TIM17, cycles / 2);
	LL_TIM_GenerateEvent_UPDATE(TIM17);

	LL_TIM_EnableAllOutputs(TIM17);
	LL_TIM_CC_EnableChannel(TIM17, LL_TIM_CHANNEL_CH1);
	LL_TIM_EnableCounter(TIM17);


	//HACK speed up i2c, well above spec, but it works. about 1.25mhz
	LL_I2C_Disable(I2C1);
	LL_I2C_SetTiming(I2C1, __LL_I2C_CONVERT_TIMINGS(
			0, //PRESCALER
			1, //SETUP_TIME
			0, //HOLD_TIME
			8, //SCLH_PERIOD
			8 //SCLL_PERIOD
	));
	LL_I2C_Enable(I2C1);


	ssd1306_Init();
	ssd1306_SetContrast(0xff);


    int frame = 0;
    uint32_t frameTimer = ticks;
    volatile int fps = 0;
    int pwm;
    int run = 1;
	uint32_t rate = 1750000;

    for(;;) {
		adcManagerLoop();
//		ssd1306_TestFPS();

    	if (ticks - frameTimer >= 1000) {
    		fps = frame;
    		frameTimer += 1000;
    		frame = 0;
    	}

		if (dialPollChangeEvent(&dial3)) {
			uint32_t tmp = dialGetValue(&dial3);
			tmp = (tmp * tmp)/95;
			rate = tmp * 35000;
			rate = setSampleRate(&sampler, rate);
		}

		if (buttonPollClickEvent(&button2)) {
			run = !run;
		}

		// stopSampler(&sampler);

		if (!run) {
			continue;
		}


        ssd1306_Fill(Black);

		int32_t minVoltage = INT32_MAX;
		int32_t maxVoltage = INT32_MIN;


		//find first rising edge

		int start = -1;
		for (int x = 64; x < 1024; x++) {
			int sample = getSample(&sampler, x);
			if (start == -1 && sample < 200) {
				start = x;
			}

			if (start >= 0 && sample > 200) {
				start = x;
				break;
			}
		}

		if (start < 64 || start > 800)
			start = 64;

		start -= 64;

		int vdiv = dialGetValue(&dial2);
		int vpos = dialGetValue(&dial1);

		SSD1306_COLOR blink = (getTicks() & 0x80) ? White : Black;
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
		
		// snprintf(buff, sizeof(buff), "FPS: %d", fps);
		// ssd1306_SetCursor(1, 24 + yStart);
		// ssd1306_WriteString(buff, Font_6x8, White);

		snprintf(buff, sizeof(buff), "V: %dmv %dsps", vdd, rate);
		ssd1306_SetCursor(1, 16 + yStart);
		ssd1306_WriteString(buff, Font_6x8, White);

		snprintf(buff, sizeof(buff), "%d, %d, %d, %d ", dialGetValue(&dial1), dialGetValue(&dial2), dialGetValue(&dial3), dialGetValue(&dial4));
		ssd1306_SetCursor(1, 24 + yStart);
		ssd1306_WriteString(buff, Font_6x8, White);

		ssd1306_WriteString(button1.isHeld ? "H" : button1.isDown ? "O" : "-", Font_6x8, White);
		ssd1306_WriteString(button2.isHeld ? "H" : button2.isDown ? "O" : "-", Font_6x8, White);
		ssd1306_WriteString(button3.isHeld ? "H" : button3.isDown ? "O" : "-", Font_6x8, White);
		ssd1306_WriteString(button4.isHeld ? "H" : button4.isDown ? "O" : "-", Font_6x8, White);



        ssd1306_UpdateScreen();

        frame++;
    }
}
