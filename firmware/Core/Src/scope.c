
#include "scope.h"
#include "ssd1306.h"
#include "sampler.h"
#include "stdio.h"

#define TDIVMODCALC(xDivNs, rate) {xDivNs, rate, 1000000000l / rate}

typedef struct {
	uint32_t xDivNs;
	uint32_t rate;
	uint32_t nsPerSample;
} TDivMode;

/*
static const AdcClockDividerSetting adcClockDividerSettings[11] = {
		{LL_ADC_CLOCK_ASYNC_DIV2, EFFECTIVE_RATE(2)}, //1.75msps
		{LL_ADC_CLOCK_ASYNC_DIV4, EFFECTIVE_RATE(4)}, //875ksps
		{LL_ADC_CLOCK_ASYNC_DIV6, EFFECTIVE_RATE(6)}, //583ksps
		{LL_ADC_CLOCK_ASYNC_DIV8, EFFECTIVE_RATE(8)}, //437.5ksps
		{LL_ADC_CLOCK_ASYNC_DIV10, EFFECTIVE_RATE(10)}, //350ksps
		{LL_ADC_CLOCK_ASYNC_DIV12, EFFECTIVE_RATE(12)}, //291.6ksps
		{LL_ADC_CLOCK_ASYNC_DIV16, EFFECTIVE_RATE(16)}, //218.75ksps
		{LL_ADC_CLOCK_ASYNC_DIV32, EFFECTIVE_RATE(32)}, //109.375ksps
		{LL_ADC_CLOCK_ASYNC_DIV64, EFFECTIVE_RATE(64)}, //54.7ksps
		{LL_ADC_CLOCK_ASYNC_DIV128, EFFECTIVE_RATE(128)}, //27.3ksps
		{LL_ADC_CLOCK_ASYNC_DIV256, EFFECTIVE_RATE(256)} //13.6ksps
};
*/

//with a sample buffer of 1024, there's 8x what we need for a 128px display.
//with 16 pixels per div, we can show 8 divs on screen. when we have more than 3 or so samples per pixel, we could use a lower sample rate
//scope stepping is usually 1, 2, 5, 10, 20, 50, 100, etc. thats a step of 2x, 2.5x, 2x, 2x, 2.5x, 2x, etc

static const TDivMode tDivModes[1] = {
		TDIVMODCALC(500, 1750000), //500ns/div, 0.875 samples per div, .054 samples per pixel
		TDIVMODCALC(1000, 1750000), //1us/div, 1.75 samples per div, .109 samples per pixel
		TDIVMODCALC(2000, 1750000), //2us/div, 3.5 samples per div, .218 samples per pixel
		TDIVMODCALC(5000, 1750000), //5us/div, 8.75 samples per div, .546 samples per pixel
		TDIVMODCALC(10000, 1750000), //10us/div, 17.5 samples per div, 1.09 samples per pixel
		TDIVMODCALC(20000, 1750000), //20us/div, 35 samples per div, 2.18 samples per pixel
		TDIVMODCALC(50000, 875000), //50us/div, 43.75 samples per div, 2.73 samples per pixel
		TDIVMODCALC(100000, 437500), //100us/div, 43.75 samples per div, 2.73 samples per pixel
		TDIVMODCALC(200000, 218750), //200us/div, 43.75 samples per div, 2.73 samples per pixel
		TDIVMODCALC(500000, 109375), //500us/div, 54.7 samples per div, 3.42 samples per pixel
		TDIVMODCALC(1000000, 54687.5), //1ms/div, 54.7 samples per div, 3.42 samples per pixel
		TDIVMODCALC(2000000, 27343.75), //2ms/div, 54.7 samples per div, 3.42 samples per pixel
		TDIVMODCALC(5000000, 13671.875), //5ms/div, 68.36 samples per div, 4.27 samples per pixel
};

// at 3.5msps 7 samples gives 2us/div



int frame = 0;
uint32_t frameTimer = 0;
int fps = 0;
int pwm;


void scopeDrawInfoPanel(ScopeSettings *scope, Sampler *sampler) {
	char buff[64];



	ssd1306_FillRectangle(0, 15, 48, 32, White);
	int Vp = (scope->maxVoltage - scope->minVoltage)/10;
	int VpVolts = Vp/100;
	int VpHv = (Vp - VpVolts*100);
	snprintf(buff, sizeof(buff), "Vp: %d.%02d", VpVolts, VpHv);
	ssd1306_SetCursor(1, 16);
	ssd1306_WriteString(buff, Font_6x8, Black);

	snprintf(buff, sizeof(buff), "FPS: %d", fps);
	ssd1306_SetCursor(1, 24);
	ssd1306_WriteString(buff, Font_6x8, Black);
}



void scopeDraw(ScopeSettings *scope, Sampler *sampler) {

	if (getTicks() - frameTimer >= 1000) {
		fps = frame;
		frameTimer += 1000;
		frame = 0;
	}


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

	if (pwm == 0) {
		delay(5);
		ssd1306_SetContrast(0x0f);
	} else {
        ssd1306_UpdateScreen();

		ssd1306_SetContrast(0xff);
	}

	scope->minVoltage = INT32_MAX;
	scope->maxVoltage = INT32_MIN;




	//find first rising edge

	scope->triggerIndex  = -1;
	for (int x = 64; x < 1024; x++) {
		int sample = getSample(sampler, x);
		if (scope->triggerIndex  == -1 && sample < 200) {
			scope->triggerIndex  = x;
		}

		if (scope->triggerIndex  >= 0 && sample > 200) {
			scope->triggerIndex  = x;
			break;
		}
	}

	if (scope->triggerIndex  < 64 || scope->triggerIndex  > 800)
		scope->triggerIndex  = 64;

	scope->triggerIndex  -= 64;


	for (int x = 0; x < SSD1306_WIDTH; x++) {

		int sample = getSample(sampler, x + scope->triggerIndex );

		int mv = 6600 * sample / 2048;

		if (mv > scope->maxVoltage)
			scope->maxVoltage = mv;
		if (mv < scope->minVoltage)
			scope->minVoltage = mv;

		if (pwm > 0) {
			int y = 31 - sample/20;
			SSD1306_COLOR color = White;
			if (y > 31) {
				y = 31;
				color = pwm;
			} else if (y < 0) {
				y = 0;
				color = pwm;
			}
			ssd1306_DrawPixel(x, y , color);
		}

	}



	if (pwm == 0) {
		drawInfoPanel();

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


//		if (pwm == 0) {
//			delay(3);
//		} else {
//			delay(10);
//		}

    pwm++;
    if (pwm > 1)
    	pwm = 0;

    frame++;


}
