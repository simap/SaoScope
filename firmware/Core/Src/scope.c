
#include "scope.h"
#include "ssd1306.h"
#include "sampler.h"
#include "stdio.h"

extern Sampler sampler;

ScopeSettings scope;
//	TAMP->BKP0R

#define TDIVMODCALC(xDivNs, rate) {xDivNs, rate, 1000000000 / rate}

typedef struct {
	uint32_t xDivNs;
	uint32_t rate;
	uint32_t nsPerSample;
} TDivMode;

//with a sample buffer of 1024, there's 8x what we need for a 128px display.
static const TDivMode tDivModes[1] = {
		TDIVMODCALC(2000, 3500000),
		TDIVMODCALC(5000, 2916667),
		TDIVMODCALC(10000, 1750000),
		TDIVMODCALC(20000, 1750000),
		TDIVMODCALC(50000, 437500),

};

// at 3.5msps 7 samples gives 2us/div



int frame = 0;
uint32_t frameTimer = 0;
int fps = 0;
int pwm;

void handkeModePress() {

}

void handleUpPress() {

}

void handleDownPress() {

}

void drawInfoPanel() {
	char buff[64];



	ssd1306_FillRectangle(0, 15, 48, 32, White);
	int Vp = (scope.maxVoltage - scope.minVoltage)/10;
	int VpVolts = Vp/100;
	int VpHv = (Vp - VpVolts*100);
	snprintf(buff, sizeof(buff), "Vp: %d.%02d", VpVolts, VpHv);
	ssd1306_SetCursor(1, 16);
	ssd1306_WriteString(buff, Font_6x8, Black);

	snprintf(buff, sizeof(buff), "FPS: %d", fps);
	ssd1306_SetCursor(1, 24);
	ssd1306_WriteString(buff, Font_6x8, Black);
}



void drawScope() {

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

	scope.minVoltage = INT32_MAX;
	scope.maxVoltage = INT32_MIN;




	//find first rising edge

	scope.triggerIndex  = -1;
	for (int x = 64; x < 1024; x++) {
		int sample = getSample(&sampler, x);
		if (scope.triggerIndex  == -1 && sample < 200) {
			scope.triggerIndex  = x;
		}

		if (scope.triggerIndex  >= 0 && sample > 200) {
			scope.triggerIndex  = x;
			break;
		}
	}

	if (scope.triggerIndex  < 64 || scope.triggerIndex  > 800)
		scope.triggerIndex  = 64;

	scope.triggerIndex  -= 64;


	for (int x = 0; x < SSD1306_WIDTH; x++) {

		int sample = getSample(&sampler, x + scope.triggerIndex );

		int mv = 6600 * sample / 2048;

		if (mv > scope.maxVoltage)
			scope.maxVoltage = mv;
		if (mv < scope.minVoltage)
			scope.minVoltage = mv;

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
