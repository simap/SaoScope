/*
 * app.c
 *
 *  Created on: Dec 7, 2022
 *      Author: benh
 */

#include "main.h"
#include "ssd1306.h"
#include "ssd1306_tests.h"

#include "sampler.h"
#include "button.h"

Sampler sampler;

ButtonState button1, button2, button3;



extern uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];


volatile uint32_t ticks;


uint32_t getCycles() {
	return SysTick->VAL;
}

volatile uint32_t pollDuration;
void systickISR() {
	ticks++;

	volatile uint32_t c = getCycles();
	if (ticks & 1) {
		//processing 3 buttons takes about 312 cycles
		// uint32_t gpioState = GPIOA->IDR;
		// buttonProcess(&button1, gpioState & BUTTON1_Pin); //button1 is active high
		// buttonProcess(&button2, !(gpioState & BUTTON2_Pin)); //button2 is active low
		// buttonProcess(&button3, !(gpioState & BUTTON3_Pin)); //button3 is active low
	}
	pollDuration = c - getCycles();

//	if (ticks & 1) {
//		LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_7);
//	} else {
//		LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_7);
//	}
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

	buttonInit(&button1, 10, 250);
	buttonInit(&button2, 10, 250);
	buttonInit(&button3, 10, 250);

	// LL_GPIO_ResetOutputPin(VOUT_EN_GPIO_Port, VOUT_EN_Pin);

	LL_SYSTICK_EnableIT();

	delay(10);


	//set GPIO speed
	LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_7, LL_GPIO_SPEED_FREQ_VERY_HIGH);

	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF_5); //set to TIM17_CH1

//
	LL_TIM_OC_SetMode(TIM17, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
//	LL_TIM_OC_EnablePreload(TIM16, LL_TIM_CHANNEL_CH1);

	int cycles = SystemCoreClock / 50000 ;

	LL_TIM_SetAutoReload(TIM17, cycles-1);
	LL_TIM_OC_SetCompareCH1(TIM17, cycles / 2);
	LL_TIM_GenerateEvent_UPDATE(TIM17);

	LL_TIM_EnableAllOutputs(TIM17);
	LL_TIM_CC_EnableChannel(TIM17, LL_TIM_CHANNEL_CH1);
	LL_TIM_EnableCounter(TIM17);

	LL_ADC_REG_SetSequencerChannels(ADC1, LL_ADC_CHANNEL_1);


	initSampler(&sampler);

	// volatile uint32_t rate = setSampleRate(&sampler, 1750000);
	volatile uint32_t rate = setSampleRate(&sampler, 2916667);
	


//	LL_TIM_SetPrescaler(TIM3, 0);
	//the fastest we can sample (via trigger) is every 22 cycles. with 8 bit, 1.5 sample time, 2.91MHz
	//bumping sample time up to 3.5, 26 cycles is the min sample time. 2.46MHz
	//7.5 sample time requires 32 cycles, 2MHz
	//the shape changes slightly going from 1.5 to 7.5, which is technically where it should be based on the DS

//	LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_19CYCLES_5);
//	LL_TIM_SetAutoReload(TIM3, 50-1);
//	LL_TIM_GenerateEvent_UPDATE(TIM3);
//
//
//	LL_ADC_Enable(ADC1);
//	LL_ADC_REG_StartConversion(ADC1);
//	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) sampleBuffer);
//	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t) &ADC1->DR);
//	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, ELEM(sampleBuffer));
//	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
//
//	LL_TIM_EnableCounter(TIM3);


	//HACK speed up i2c
	LL_I2C_Disable(I2C1);
//	LL_I2C_SetTiming(I2C1, 0x00200606);
	// LL_I2C_SetTiming(I2C1, __LL_I2C_CONVERT_TIMINGS(
	// 				0, //PRESCALER
	// 				0, //SETUP_TIME
	// 				0, //HOLD_TIME
	// 				6, //SCLH_PERIOD
	// 				6 //SCLL_PERIOD
	// 		));

	LL_I2C_Enable(I2C1);


    ssd1306_Init();
   ssd1306_SetContrast(0xff);

    //set clock
//    ssd1306_WriteCommand(0xd5);
//    ssd1306_WriteCommand(0xf0);


    //erase offscreen rows
//    ssd1306_Fill(White);
//    for(uint8_t i = 4; i < 8; i++) {
//        ssd1306_WriteCommand(0xB0 + i); // Set the current RAM page address.
//        ssd1306_WriteCommand(0x00);
//        ssd1306_WriteCommand(0x10);
//        ssd1306_WriteData(&SSD1306_Buffer[0],SSD1306_WIDTH);
//    }
//
//    //setup scroll
//    ssd1306_WriteCommand(0x29); //or 0x2a
//    ssd1306_WriteCommand(0); //dummy
//    ssd1306_WriteCommand(0x0f); //start page
//    ssd1306_WriteCommand(0b00000111); //interval
//    ssd1306_WriteCommand(0x0f); //end page
//    ssd1306_WriteCommand(32); //row offset
//
//
//    ssd1306_WriteCommand(0x2F);//activate scroll



    int frame = 0;
    uint32_t frameTimer = ticks;
    int fps = 0;
    int pwm;

    int run = 1;

    for(;;) {
//		ssd1306_TestFPS();

    	if (ticks - frameTimer >= 1000) {
    		fps = frame;
    		frameTimer += 1000;
    		frame = 0;
    	}

    	if (buttonPollClickEvent(&button2)) {
    		run = !run;
    	}

    	if (buttonPollClickEvent(&button3)) {
    		rateSwitch++;
    		if (rateSwitch > 8)
    			rateSwitch = 0;

    		switch (rateSwitch) {
    		case 0:
    			rate = setSampleRate(&sampler, 4375000);
    			break;
    		case 1:
    			rate = setSampleRate(&sampler, 2916667);
    			break;
    		case 2:
    			rate = setSampleRate(&sampler, 1750000);
    			break;
    		case 3:
				rate = setSampleRate(&sampler, 1750000/2);
				break;
			case 4:
				rate = setSampleRate(&sampler, 1750000/4);
				break;
			case 5:
				rate = setSampleRate(&sampler, 1750000/8);
				break;
			case 6:
				rate = setSampleRate(&sampler, 1750000/16);
				break;
			case 7:
				rate = setSampleRate(&sampler, 1750000/32);
				break;
			case 8:
				rate = setSampleRate(&sampler, 1750000/64);
				break;
    		}

    	}


		stopSampler(&sampler);


        ssd1306_Fill(Black);
//        ssd1306_WriteCommand(0x2E); //deactivate scroll
//        ssd1306_WriteCommand(0x2F);//activate scroll

        //use secret command to fill black rect in vram instead of sending all the bits
//        ssd1306_WriteCommand(0x24);
//        ssd1306_WriteCommand(0);
//        ssd1306_WriteCommand(0); //rowstart
//        ssd1306_WriteCommand(0x00); //pattern
//        ssd1306_WriteCommand(3); //rowend
//        ssd1306_WriteCommand(1);
//        ssd1306_WriteCommand(128);


//		delay(5);

//		if (pwm == 0) {
//			delay(5);
//			ssd1306_SetContrast(0x0f);
//		} else {
////	        ssd1306_UpdateScreen();
//
//			ssd1306_SetContrast(0xff);
//		}

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
//		int start = 64;


		SSD1306_COLOR blink = (getTicks() & 0x80) ? White : Black;
		for (int x = 0; x < SSD1306_WIDTH; x++) {

			int sample = getSample(&sampler, x + start);

			int mv = 15000 * sample / 2048;
//			int mv = 3300 * sample / 2048;

			if (mv > maxVoltage)
				maxVoltage = mv;
			if (mv < minVoltage)
				minVoltage = mv;

			if (pwm > 0) {
				int y = 31 - sample/20;
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

		}

    	if (run) {
    		startSampler(&sampler);
    	}

		char buff[64];

		if (pwm == 0) {
			const int yStart = 32;
			ssd1306_FillRectangle(0, 15 + yStart, 48, 32 + yStart, White);
			int Vp = (maxVoltage - minVoltage)/10;
			int VpVolts = Vp/100;
			int VpHv = (Vp - VpVolts*100);
			snprintf(buff, sizeof(buff), "Vp: %d.%02d", VpVolts, VpHv);
			ssd1306_SetCursor(1, 16 + yStart);
			ssd1306_WriteString(buff, Font_6x8, Black);

			snprintf(buff, sizeof(buff), "FPS: %d", fps);
			ssd1306_SetCursor(1, 24 + yStart);
			ssd1306_WriteString(buff, Font_6x8, Black);

//			ssd1306_SetContrast(0x00);
		} else {
//			ssd1306_SetContrast(0xff);
		}



//		if (pwm == 0) {
//			ssd1306_SetContrast(0x0f);
//		} else {
//			ssd1306_SetContrast(0xff);
//		}


//        ssd1306_WriteCommand(0x2E); //deactivate scroll
        ssd1306_UpdateScreen();
//        ssd1306_WriteCommand(0x2F);//activate scroll


		if (pwm == 0) {
			delayCycles(56000 / 2);
//			delay(1);
		} else {
			delayCycles(56000);
		}

        pwm++;
        if (pwm > 1)
        	pwm = 0;

        frame++;

        delay(1);

    }
}
