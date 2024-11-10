/*
 * wGen.c
 *
 *  Created on: 06/18/2024
 *      Author: Michael Kurta
 */

#include "stm32h7xx_hal.h"
#include "wGen.h"
#include "string.h"
#include "SH1106.h"
#include "stdio.h"

#define ENCODER_PULSES_PER_STEP 2
#define pi  3.14152

// External Sprite Arrays from bitmap.h
extern const uint8_t ramp10[];
extern const uint8_t ramp20[];
extern const uint8_t ramp30[];
extern const uint8_t ramp40[];
extern const uint8_t ramp50[];
extern const uint8_t ramp60[];
extern const uint8_t ramp70[];
extern const uint8_t ramp80[];
extern const uint8_t ramp90[];
extern const uint8_t square10[];
extern const uint8_t square20[];
extern const uint8_t square30[];
extern const uint8_t square40[];
extern const uint8_t square50[];
extern const uint8_t square60[];
extern const uint8_t square70[];
extern const uint8_t square80[];
extern const uint8_t square90[];
extern const uint8_t sinewave[];
extern const uint8_t TX_Icon[];
extern const uint16_t ARR_period[];

extern uint16_t counter;
extern int8_t counterUp;
extern int8_t newDiff;
extern uint8_t isrCalled;

// Typedef handles for DAC, Timer6 and DMA
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim6;
extern DMA_HandleTypeDef hdma_dac1_ch1;


int32_t deltaFrequency 	= 0;	// Stores the current increment to add / subtract from wGen->frequency
uint16_t samples;				// Stores the size (samples) of the current waveform output buffer; depending on frequency

// Array of macro defined values which reference the current main menu cursor position
const int MAIN_OPTIONS[7] = {
	CURSOR_WAVEFORM_XPOS,
	CURSOR_FREQ_HUNDRED_XPOS,
	CURSOR_FREQ_TENS_XPOS,
	CURSOR_FREQ_ONES_XPOS,
	CURSOR_FREQ_UNITS_XPOS,
	CURSOR_PERCENT_XPOS,
	CURSOR_TX_XPOS
};

uint32_t TX_Bits[MAX_SAMPLES_PER_REV];				// Buffer which stores all the current waveform values

void lcdInit(wGen_HandleTypeDef * wGen){
	SH1106_DrawLine( 0, 50, 127, 50, 1);   			// Horizontal line above the data fields
	SH1106_DrawLine( 31, 51, 31, 63, 1);			// Vertical line in front of the MODE data field
	SH1106_GotoXY( 2, 53);
	SH1106_Puts("SINE", &Font_7x10, 1);

	// Display current frequency. Default is in kHz, but does not display
	char buf[7];
	sprintf(buf, "%i", wGen->frequency / 1000);
	SH1106_GotoXY( 34, 53);
	SH1106_Puts(buf, &Font_7x10, 1);

	// Display freq units
	SH1106_GotoXY( 60 , 53);
	SH1106_Puts("kHz", &Font_7x10, 1);

	uint16_t index 	= (wGen->counter >> 1) % 6;

	SH1106_DrawLine( 83, 51, 83, 64, 1);			// Vertical line in front of the FREQUENCY data field

	// Draw the current selection cursor, sine wave graphic, "TX:" and update the screen
	SH1106_DrawTriangle(MAIN_OPTIONS[index] - 4, 45, MAIN_OPTIONS[index] + 4, 45, MAIN_OPTIONS[index], 49, 1);
	SH1106_DrawBitmap(2, 0, sinewave, 80, 40, 1);
	SH1106_GotoXY( 90 , 15);
	SH1106_Puts("TX:", &Font_7x10, 1);
	SH1106_UpdateScreen();
}

wGen_HandleTypeDef wGen_create(){

	wGen_HandleTypeDef wGen;

	wGen.clickConsumed			= 1; 	//wGen.counter contains all required info to draw the screen
	wGen.counter 				= ROTARY_COUNTER_START;
	wGen.currentBufSize			= TX_BUF_SIZE_MAX_100000_HZ;
	wGen.currentStateBtn		= 0;
	wGen.currentStateClk 		= 0;
	wGen.currentMenuPos 		= 0;
	wGen.currentWaveSelected 	= 0;	// 0 = SINE, 1 = SQR, 2 = RAMP;
	wGen.currentPercent			= 50;
	wGen.frequency				= 100000;
	wGen.isPressed 				= 0;
	wGen.isTransmitting			= 0;
	wGen.millisStart 			= HAL_GetTick();
	wGen.lastUpdate 			= wGen.millisStart;
	wGen.menuMode				= 0;     											// 0 = Top Menu, 1 = Waveform submenu, 2 = Hundreds, 3 = Tens
	wGen.lastPress 				= 0;
	wGen.longPressTrigger 		= 0;
    wGen.previousMenuPos		= 0;
	wGen.previousPressState 	= 0;
	wGen.previousPressTime		= 0;
	wGen.previousStateClk 		= HAL_GPIO_ReadPin(CLK_IN_GPIO_Port, CLK_IN_Pin);
	wGen.rotaryDir 				= 0;
	wGen.stateChange			= 0;
	wGen.unitDisplay			= DISPLAY_UNITS_KHZ;


	return wGen;
}


void buttonUpdate(wGen_HandleTypeDef * wGen){
	wGen->currentStateBtn = !HAL_GPIO_ReadPin(GPIOC, B1_Pin);
	wGen->stateChange = wGen->currentStateBtn - wGen->previousPressState;
	if(wGen->stateChange < 0){
		if(HAL_GetTick() - wGen->previousPressTime > SWITCH_DEBOUNCE_MS){
			wGen->isPressed = 1;
			wGen->clickConsumed = 0;
			consumeClick(wGen);
		}
	}

	if(wGen->stateChange > 0){
		wGen->previousPressState = HAL_GetTick();
	}

	wGen->previousPressState = wGen->currentStateBtn;
}

void checkSampleChange(wGen_HandleTypeDef * wGen){
	getSamples(wGen);

	if(wGen->currentBufSize != samples){
		getSamples(wGen);
	}
}

// Switch case for every menu selection state
void consumeClick(wGen_HandleTypeDef * wGen){

	if(!wGen->menuMode){
		switch(wGen->currentMenuPos){
			case 0:
				selectWaveform(wGen);
				break;

			case 1:
				selectHundreds(wGen);
				break;

			case 2:
				selectTens(wGen);
				break;

			case 3:
				selectOnes(wGen);
				break;

			case 4:
				selectUnits(wGen);
				break;

			case 5:
				selectPercent(wGen);
				break;

			case 6:
				selectTransmit(wGen);
				break;

			default:
				selectWaveform(wGen);
			}
	}else{
		exitToMain(wGen);
	}


	wGen->clickConsumed = 1;
}

void eraseCursor(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledRectangle(1, 45, 110, 4, 0);
	SH1106_DrawFilledRectangle(CURSOR_TX_XPOS, CURSOR_TX_YPOS - 4, 4, 8, 0);
}

// Function for backing out of a submenu and exiting to main
void exitToMain(wGen_HandleTypeDef * wGen){
	char buf[2];
	switch(wGen->menuMode){
	// Waveform edit box exiting to main
	case 1:
		eraseCursor(wGen);
		SH1106_DrawTriangle(MAIN_OPTIONS[0] - 4, 45, MAIN_OPTIONS[0] + 4, 45, MAIN_OPTIONS[0], 49, 1);
		SH1106_DrawFilledRectangle(2, 52, 28, 11, 0);

		if(wGen->currentWaveSelected == 0){
			SH1106_GotoXY( 2, 53);
			SH1106_Puts("SINE", &Font_7x10, 1);

		}else if(wGen->currentWaveSelected == 1){
			SH1106_GotoXY( 5, 53);
			SH1106_Puts("SQR", &Font_7x10, 1);
		}else{
			SH1106_GotoXY( 2, 53);
			SH1106_Puts("RAMP", &Font_7x10, 1);
		}

		wGen->menuMode = 0;
		SH1106_UpdateScreen();
		wGen->counter = ROTARY_COUNTER_START;
		break;


	case 2:
		eraseCursor(wGen);
		SH1106_DrawTriangle(MAIN_OPTIONS[1] - 4, 45, MAIN_OPTIONS[1] + 4, 45, MAIN_OPTIONS[1], 49, 1);
		SH1106_DrawFilledRectangle(34, 52, 6, 11, 0);
		int numHundreds = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? wGen->frequency / 100000 : wGen->frequency / 100);
		sprintf(buf, "%d", numHundreds);
		SH1106_GotoXY(34, 53);
		if(numHundreds != 0){
			SH1106_Puts(buf, &Font_7x10, 1);
		}
		wGen->menuMode = 0;
		SH1106_UpdateScreen();
		wGen->counter = ROTARY_COUNTER_START + 1;
		break;

	case 3:
		eraseCursor(wGen);
		SH1106_DrawTriangle(MAIN_OPTIONS[2] - 4, 45, MAIN_OPTIONS[2] + 4, 45, MAIN_OPTIONS[2], 49, 1);
		SH1106_DrawFilledRectangle(41, 52, 6, 11, 0);
		int numTens = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 100000)/10000 : (wGen->frequency % 100) / 10);
		sprintf(buf, "%d", numTens);
		SH1106_GotoXY(41, 53);
		if((wGen->frequency >= 100000 && numTens == 0) || (wGen->frequency < 1000 && wGen->frequency > 99 && numTens == 0) || (numTens != 0)){
			SH1106_Puts(buf, &Font_7x10, 1);
		}
		wGen->menuMode = 0;
		SH1106_UpdateScreen();
		wGen->counter = ROTARY_COUNTER_START + 2;
		break;

	case 4:
		eraseCursor(wGen);
		SH1106_DrawTriangle(MAIN_OPTIONS[3]- 4, 45, MAIN_OPTIONS[3] + 4, 45, MAIN_OPTIONS[3], 49, 1);
		SH1106_DrawFilledRectangle(48, 52, 6, 11, 0);
		int numOnes = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 10000)/1000 : wGen->frequency % 10);
		sprintf(buf, "%d", numOnes);
		SH1106_GotoXY(48, 53);
		SH1106_Puts(buf, &Font_7x10, 1);
		wGen->menuMode = 0;
		SH1106_UpdateScreen();
		wGen->counter = ROTARY_COUNTER_START + 3;
		break;

	case 5:
		eraseCursor(wGen);
		SH1106_DrawTriangle(MAIN_OPTIONS[5]- 4, 45, MAIN_OPTIONS[5] + 4, 45, MAIN_OPTIONS[5], 49, 1);
		SH1106_DrawFilledRectangle(84, 52, 28, 11, 0);
		char buf[2];
		sprintf(buf, "%i", wGen->currentPercent);
		SH1106_GotoXY(85, 53);
		SH1106_Puts(buf, &Font_7x10, 1);
		SH1106_Puts(" %", &Font_7x10, 1);
		SH1106_UpdateScreen();
		wGen->menuMode = 0;
		wGen->counter = ROTARY_COUNTER_START + 4;
		break;
	}
}

void getSamples(wGen_HandleTypeDef * wGen){
	uint16_t lastSamples = samples;
	if(wGen->frequency < 251){
			samples = TX_BUF_SIZE_MAX_250_HZ;
		}else if(wGen->frequency < 501){
			samples = TX_BUF_SIZE_MAX_500_HZ;
		}else if(wGen->frequency <= 5000){
			samples = TX_BUF_SIZE_MAX_5000_HZ;
		}else if(wGen->frequency <= 10000){
			samples = TX_BUF_SIZE_MAX_10000_HZ;
		}else if(wGen->frequency <= 20000){
			samples = TX_BUF_SIZE_MAX_20000_HZ;
		}else if(wGen->frequency <= 100000){
			samples = TX_BUF_SIZE_MAX_100000_HZ;
		}else if(wGen->frequency <= 200000){
			samples = TX_BUF_SIZE_MAX_200000_HZ;
		}else{
			samples = TX_BUF_SIZE_MAX_999000_HZ;
		}
		wGen->currentBufSize = samples;
	if(lastSamples != samples){
		switch(wGen->currentWaveSelected){
		case 0:
			getSineVal(wGen);
			break;

		case 1:
			getSquareVal(wGen);
			break;

		case 2:
			getRampVal(wGen);
			break;

		default:
			getSineVal(wGen);
		}
	}
}

void getRampVal(wGen_HandleTypeDef * wGen){
	uint16_t rampUpDivs =   round(wGen->currentPercent * samples / 100) ;
	uint16_t rampDownDivs = samples - rampUpDivs;
	float divSize = 4095 / rampUpDivs;
	float accumulator = 0;
	int i;
	for(i = 0; i < rampUpDivs; i++){
		accumulator += divSize;
		TX_Bits[i] = round(accumulator);
	}

	divSize = 4095 / rampDownDivs;
	accumulator = RESOLUTION_12BIT - 1;
	for(i = rampUpDivs; i < samples; i++){
		accumulator -= divSize;
		TX_Bits[i] = round(accumulator);
	}
}

void getSquareVal(wGen_HandleTypeDef * wGen){
	for(int i = 0; i < samples; i++){
		TX_Bits[i] = ((float)wGen->currentPercent/100 * samples < i ? 0 : RESOLUTION_12BIT - 1);
	}
}

void getSineVal(wGen_HandleTypeDef * wGen){
	for(int i = 0; i < samples; i++){
		TX_Bits[i] = ((sin(i * 2 * pi / samples) + 1)) * RESOLUTION_12BIT / 2;
	}
}


// Detects changes in counter, which is modified in the ISR
void loopUpdate(wGen_HandleTypeDef * wGen){
	static int8_t lastTick = 0;
	if(!isrCalled){
		return;
	}
	wGen->rotaryDir = counterUp;

	// This method starts with a value of '0' and requires two positive or two negative rotary steps to change a value.
	// For example, if the assume a start count of zero, and if I move foreward one step, then back two steps, then forward three steps...
	// I've only made one change and that's at the final step.

	if(wGen->rotaryDir == ROTARY_DIRECTION_CLOCK){
		if(lastTick == 1){
			lastTick = 0;
			updateRotarySel(wGen);
			// do stuff
		}else{
			lastTick++;
		}

	}else if(wGen->rotaryDir == ROTARY_DIRECTION_ANTICLOCK){
		if(lastTick == -1){
			lastTick = 0;
			updateRotarySel(wGen);
			// do stuff
		}else{
			lastTick--;
		}
	}
	// let the loop complete its update before clearing the rotaryDir & isr flags
	wGen->rotaryDir = ROTARY_DIRECTION_NONE;
	isrCalled = 0;
}

void ramp(wGen_HandleTypeDef * wGen){
	SH1106_Puts("RAMP", &Font_7x10, 0);
	wGen->currentWaveSelected = 2;
	wGen->currentPercent = 50;
	SH1106_DrawBitmap(2, 0, ramp50, 80, 40, 1);
	SH1106_DrawFilledRectangle(84, 52, 28, 11, 0);
	SH1106_DrawFilledRectangle(84, 30, 34, 11, 0);
	SH1106_GotoXY(85, 53);
	SH1106_Puts("50 %", &Font_7x10, 1);
	SH1106_GotoXY(85, 30);
	SH1106_Puts("SYM:", &Font_7x10, 1);
}

void selectHundreds(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledTriangle(MAIN_OPTIONS[1] - 4, 45, MAIN_OPTIONS[1] + 4, 45, MAIN_OPTIONS[1], 49, 1);
	int num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? wGen->frequency / 100000 : wGen->frequency / 100);
	SH1106_DrawFilledRectangle(34, 52, 6, 11, 1);
	char buf[2];
	sprintf(buf, "%d", num);
	SH1106_GotoXY( 34, 53);
	SH1106_Puts(buf, &Font_7x10, 0);
	wGen->menuMode = 2;
	SH1106_UpdateScreen();
}

void selectTens(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledTriangle(MAIN_OPTIONS[2] - 4, 45, MAIN_OPTIONS[2] + 4, 45, MAIN_OPTIONS[2], 49, 1);
	int num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 100000)/10000 : (wGen->frequency % 100) / 10);
	SH1106_DrawFilledRectangle(41, 52, 6, 11, 1);
	char buf[2];
	sprintf(buf, "%d", num);
	SH1106_GotoXY( 41, 53);
	SH1106_Puts(buf, &Font_7x10, 0);
	wGen->menuMode = 3;
	SH1106_UpdateScreen();
}

void selectOnes(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledTriangle(MAIN_OPTIONS[3] - 4, 45, MAIN_OPTIONS[3] + 4, 45, MAIN_OPTIONS[3], 49, 1);
	int num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 10000)/1000 : wGen->frequency % 10);
	SH1106_DrawFilledRectangle(48, 52, 6, 11, 1);
	char buf[2];
	sprintf(buf, "%i", num);
	SH1106_GotoXY( 48, 53);
	SH1106_Puts(buf, &Font_7x10, 0);
	wGen->menuMode = 4;
	SH1106_UpdateScreen();
}

// Selects the kHz / Hz data field.  Unlike the other menus, this only toggles from the main menu
void selectUnits(wGen_HandleTypeDef * wGen){

	SH1106_DrawFilledRectangle(60, 52, 21, 11, 0);

	if(wGen->unitDisplay == DISPLAY_UNITS_KHZ){
		SH1106_GotoXY(63, 53);
		SH1106_Puts("Hz", &Font_7x10, 1);
		wGen->unitDisplay = DISPLAY_UNITS_HZ;
		wGen->frequency /= 1000;
	}else{
		SH1106_GotoXY(60, 53);
		SH1106_Puts("kHz", &Font_7x10, 1);
		wGen->unitDisplay = DISPLAY_UNITS_KHZ;
		wGen->frequency *= 1000;
	}
	SH1106_UpdateScreen();
}

void selectPercent(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledTriangle(MAIN_OPTIONS[5] - 4, 45, MAIN_OPTIONS[5] + 4, 45, MAIN_OPTIONS[5], 49, 1);
	SH1106_DrawFilledRectangle(84, 52, 28, 11, 1);
	char buf[2];
	sprintf(buf, "%i", wGen->currentPercent);
	SH1106_GotoXY(85, 53);
	SH1106_Puts(buf, &Font_7x10, 0);
	SH1106_Puts(" %", &Font_7x10, 0);
	wGen->menuMode = 5;
	SH1106_UpdateScreen();
}


void selectTransmit(wGen_HandleTypeDef * wGen){
	wGen->isTransmitting = (wGen->isTransmitting ? 0 : 1);

	if(wGen->isTransmitting){
		SH1106_DrawFilledRectangle(90, 14, 20, 11, 0);
		SH1106_GotoXY( 90 , 15);
		SH1106_Puts("TX!", &Font_7x10, 1);
		SH1106_DrawBitmap(115, 15, TX_Icon, 10, 10, 1);
		HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);

	}else{
		SH1106_DrawFilledRectangle(90, 14, 20, 11, 0);
		SH1106_GotoXY( 90 , 15);
		SH1106_Puts("TX:", &Font_7x10, 1);
		SH1106_DrawFilledRectangle(115, 15, 10, 10, 0);
		HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
	}

	SH1106_UpdateScreen();
}


// Puts the cursor into the waveform data field to edit
void selectWaveform(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledTriangle(MAIN_OPTIONS[0] - 4, 45, MAIN_OPTIONS[0] + 4, 45, MAIN_OPTIONS[0], 49, 1);
	switch(wGen->currentWaveSelected){
		case 0:				// SINE
			wGen->counter = 0x3F6C;
			SH1106_DrawFilledRectangle(2, 52, 28, 11, 1);
			SH1106_GotoXY( 2, 53);
			SH1106_Puts("SINE", &Font_7x10, 0);
			wGen->menuMode = 1;
			SH1106_UpdateScreen();
			if(wGen->isTransmitting){
				HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
				getSineVal(wGen);
				HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
			}
			break;

		case 1:				// SQR
			wGen->counter = 0x3F6E;
			SH1106_DrawFilledRectangle(2, 52, 28, 11, 1);
			SH1106_GotoXY( 5, 53);
			SH1106_Puts("SQR", &Font_7x10, 0);
			wGen->menuMode = 1;
			SH1106_UpdateScreen();
			if(wGen->isTransmitting){
				HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
				getSquareVal(wGen);
				HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
			}
			getSquareVal(wGen);
			break;

		case 2:				// RAMP
			wGen->counter = 0x3F70;
			SH1106_DrawFilledRectangle(2, 52, 28, 11, 1);
			SH1106_GotoXY( 2, 53);
			SH1106_Puts("RAMP", &Font_7x10, 0);
			wGen->menuMode = 1;
			SH1106_UpdateScreen();
			if(wGen->isTransmitting){
				HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
				getRampVal(wGen);
				HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
			}
			break;


		default:
			wGen->counter = 0x3F6C;
			SH1106_DrawFilledRectangle(2, 52, 28, 11, 1);
			SH1106_GotoXY( 2, 53);
			SH1106_Puts("SINE", &Font_7x10, 0);
			wGen->menuMode = 1;
			getSineVal(wGen);
			SH1106_UpdateScreen();
	}
}

void sine(wGen_HandleTypeDef * wGen){
	SH1106_GotoXY(2, 53);
	SH1106_Puts("SINE", &Font_7x10, 0);
	wGen->currentWaveSelected = 0;
	SH1106_DrawBitmap(2, 0, sinewave, 80, 40, 1);
	wGen->currentPercent = 50;
	SH1106_DrawFilledRectangle(84, 52, 28, 11, 0);
	SH1106_DrawFilledRectangle(84, 30, 30, 11, 0);
}

void square(wGen_HandleTypeDef * wGen){
	SH1106_GotoXY(5, 53);
	SH1106_Puts("SQR", &Font_7x10, 0);
	wGen->currentWaveSelected = 1;
	wGen->currentPercent = 50;
	SH1106_DrawBitmap(2, 0, square50, 80, 40, 1);
	SH1106_DrawFilledRectangle(84, 52, 28, 11, 0);
	SH1106_DrawFilledRectangle(84, 30, 30, 11, 0);
	SH1106_GotoXY(85, 30);
	SH1106_Puts("Duty:", &Font_7x10, 1);
	SH1106_GotoXY(85, 53);
	SH1106_Puts("50 %", &Font_7x10, 1);
}

void updateBitmap(wGen_HandleTypeDef * wGen){

	SH1106_DrawFilledRectangle(2, 0, 80, 40, 0);
	switch(wGen->currentPercent){

	case 10:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square10, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp10, 80, 40, 1);		// Ramp wave
		}
		break;

	case 20:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square20, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp20, 80, 40, 1);		// Ramp wave
		}
		break;

	case 30:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square30, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp30, 80, 40, 1);		// Ramp wave
		}
		break;

	case 40:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square40, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp40, 80, 40, 1);		// Ramp wave
		}
		break;

	case 50:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square50, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp50, 80, 40, 1);		// Ramp wave
		}
		break;

	case 60:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square60, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp60, 80, 40, 1);		// Ramp wave
		}
		break;

	case 70:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square70, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp70, 80, 40, 1);		// Ramp wave
		}
		break;

	case 80:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square80, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp80, 80, 40, 1);		// Ramp wave
		}
		break;

	case 90:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square90, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp90, 80, 40, 1);		// Ramp wave
		}
		break;

	default:
		if(wGen->currentWaveSelected == 1){						// Square wave
			SH1106_DrawBitmap(2, 0, square50, 80, 40, 1);
		}else{
			SH1106_DrawBitmap(2, 0, ramp50, 80, 40, 1);		// Ramp wave
		}

	}  // end switch

}

void updateHundreds(wGen_HandleTypeDef * wGen){
	// Polling function that reads wGen->frequency value and displays the 100th digit value in that spot
	// if a rotary tick is detected, wGen->frequency and the display are updated for that 100th digit

	SH1106_DrawFilledRectangle(34, 52, 6, 11, 1);
	// Read what value wGen->frequency is and draw the 100th digit
	int num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? wGen->frequency / 100000 : wGen->frequency / 100);

	if(wGen->rotaryDir == 1){
		if(num == 9){
			deltaFrequency 	= (wGen->unitDisplay ? -900000 : -900);
		}else{
			deltaFrequency = (wGen->unitDisplay ? 100000 : 100);
		}
		wGen->frequency += deltaFrequency;

	}else if(wGen->rotaryDir == -1){
		if(num == 0){
			deltaFrequency = (wGen->unitDisplay ? 900000 : 900);
		}else{
			deltaFrequency = (wGen->unitDisplay ? -100000 : -100);
		}
		wGen->frequency += deltaFrequency;
	}
	updateOutputFrequency(wGen);
	num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? wGen->frequency / 100000 : wGen->frequency / 100);
	SH1106_DrawFilledRectangle(34, 52, 6, 11, 1);
	char buf[2];
	sprintf(buf, "%d", num);
	SH1106_GotoXY( 34, 53);
	if(num != 0){
		SH1106_Puts(buf, &Font_7x10, 0);
	}
}

void updateTens(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledRectangle(41, 52, 6, 11, 1);
	// Read what value wGen->frequency is and draw the 10th digit
	int num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 100000)/10000 : (wGen->frequency % 100) / 10);
	if(wGen->rotaryDir == 1){
		if(num == 9){
			deltaFrequency = (wGen->unitDisplay ? -90000 : -90);
		}else{
			deltaFrequency = (wGen->unitDisplay ? 10000 : 10);
		}
		wGen->frequency += deltaFrequency;

	}else if(wGen->rotaryDir == -1){
		if(num == 0){
			deltaFrequency = (wGen->unitDisplay ? 90000 : 90);
		}else{
			deltaFrequency = (wGen->unitDisplay ? -10000 : -10);
		}
		wGen->frequency += deltaFrequency;
	}
	updateOutputFrequency(wGen);
	num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 100000)/10000 : (wGen->frequency % 100) / 10);
	SH1106_DrawFilledRectangle(41, 52, 6, 11, 1);
	char buf[2];
	sprintf(buf, "%d", num);
	SH1106_GotoXY( 41, 53);

	// All the conditions in which '0' will be drawn in the tens position; otherwise it will be left blank
	if((wGen->frequency >= 100000 && num == 0) || (wGen->frequency < 1000 && wGen->frequency > 99 && num == 0) || (num != 0)){
		SH1106_Puts(buf, &Font_7x10, 0);
	}

}

void updateOnes(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledRectangle(48, 52, 6, 11, 1);
	/// Read what value wGen->frequency is and draw the ones digit
	int num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 10000)/1000 : wGen->frequency % 10);
	if(wGen->rotaryDir == 1){
		if(num == 9){
			deltaFrequency = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? -9000 : -9);
		}else{
			deltaFrequency = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? 1000 : 1);
		}
		wGen->frequency += deltaFrequency;

	}else if(wGen->rotaryDir == -1){
		if(num == 0){
			deltaFrequency = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? 9000 : 9);
		}else{
			deltaFrequency = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? -1000 : -1);
		}
		wGen->frequency += deltaFrequency;
	}
	updateOutputFrequency(wGen);
	num = (wGen->unitDisplay == DISPLAY_UNITS_KHZ ? (wGen->frequency % 10000)/1000 : wGen->frequency % 10);
	SH1106_DrawFilledRectangle(48, 52, 6, 11, 1);
	char buf[2];
	sprintf(buf, "%i", num);
	SH1106_GotoXY( 48, 53);
	SH1106_Puts(buf, &Font_7x10, 0);
}

void updateOutputFrequency(wGen_HandleTypeDef * wGen){
	if(!deltaFrequency){
		return;
	}
	HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
	HAL_TIM_Base_Stop_IT(&htim6);
	getSamples(wGen);

	// Determine what number to use for an array index
	int num = wGen->unitDisplay == DISPLAY_UNITS_KHZ ? wGen->frequency/1000 + 999 : wGen->frequency - 1;

	htim6.Instance->ARR = ARR_period[num];
	htim6.Init.Period 	= htim6.Instance->ARR;

	HAL_TIM_Base_Start_IT(&htim6);
	HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
	deltaFrequency = 0;
}

void updatePercent(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledRectangle(84, 52, 28, 11, 1);
	if(wGen->rotaryDir == 1){
		if(wGen->currentPercent < 90){
			wGen->currentPercent += 10;
		}
	}else if(wGen->rotaryDir == -1){
		if(wGen->currentPercent > 10){
			wGen->currentPercent -= 10;
		}
	}
	updateBitmap(wGen);
	char buf[2];
	sprintf(buf, "%i", wGen->currentPercent);
	SH1106_GotoXY(85, 53);
	SH1106_Puts(buf, &Font_7x10, 0);
	SH1106_Puts(" %", &Font_7x10, 0);
	if(wGen->currentWaveSelected == 2 && wGen->isTransmitting){
		HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
		getRampVal(wGen);
		HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
	}else if(wGen->currentWaveSelected == 1 && wGen->isTransmitting){
		HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
		getSquareVal(wGen);
		HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
	}
	SH1106_UpdateScreen();
}


void updateRotarySel(wGen_HandleTypeDef * wGen){

	// TOP menu: rotary moves around the selections with a triangle
	// MODE menu: rotary moves between SINE, SQR, and RAMP
	// HUNDREDS menu: rotary changes the value from 0 (no char) to 9
	// TENS menu: (or ONES), rotary changes the value from 0 (no char) to 9
	// UNITS menu: rotary changes the units between kHz an Hz (kHz default)

	// Determine which menu we're in
	switch(wGen->menuMode){
		case 0: // Top menu

			// 5 menu options in SINE waveform mode, 6 RAMP and SQUARE due to the percentage modifier
			eraseCursor(wGen);
			if(wGen->rotaryDir == 1){
				wGen->currentMenuPos = (wGen->currentMenuPos ==  MAIN_MENU_OPTIONS - 1 ? 0 : wGen->currentMenuPos + 1);
			}else if(wGen->rotaryDir == - 1){
				wGen->currentMenuPos = (wGen->currentMenuPos == 0 ? MAIN_MENU_OPTIONS - 1 : wGen->currentMenuPos - 1);
			}
			if(wGen->currentMenuPos == 6){
				SH1106_DrawTriangle( CURSOR_TX_XPOS, CURSOR_TX_YPOS -4, CURSOR_TX_XPOS,
						CURSOR_TX_YPOS + 4, CURSOR_TX_XPOS + 4, CURSOR_TX_YPOS, 1);
			}else{
				SH1106_DrawTriangle( MAIN_OPTIONS[wGen->currentMenuPos] - 4, 45, MAIN_OPTIONS[wGen->currentMenuPos] + 4,
						45, MAIN_OPTIONS[wGen->currentMenuPos], 49, 1);
			}
			break;

		case 1:	// Waveform submenu
			updateWaveform(wGen);
			break;

		case 2: // Hundreds submenu
			updateHundreds(wGen);
			break;

		case 3: // Tens submenu
			updateTens(wGen);
			break;

		case 4:
			updateOnes(wGen);
			break;

		case 5:
			updatePercent(wGen);
			break;

	} // end Switch
	SH1106_UpdateScreen();
}

void updateTimerPeriod(wGen_HandleTypeDef * wGen){
	static uint8_t startup = 0;
	if(!startup){
		startup = 1;
		SH1106_DrawFilledRectangle(2, 23, 42, 11, 0);
		char buf[6];
		sprintf(buf, "%i", TIM6->ARR);
		SH1106_GotoXY(2, 12);
		SH1106_Puts("Period:", &Font_7x10, 1);
		SH1106_GotoXY(2, 23);
		SH1106_Puts(buf, &Font_7x10, 1);
		SH1106_UpdateScreen();
		getSineVal(wGen);
	}

	HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
	HAL_TIM_Base_Stop_IT(&htim6);

	if(wGen->rotaryDir == 1){
		htim6.Instance->ARR++;
		htim6.Init.Period++; 					// Update TIM6 ARR (Period)

	}else if(wGen->rotaryDir == -1){
		htim6.Instance->ARR--;
		htim6.Init.Period--; 					// Update TIM6 ARR (Period)
	}

	//TIM6_EGR |= (1 << TIMX_UG_BIT);			// Force Update Generation to reset counter
	SH1106_DrawFilledRectangle(2, 23, 42, 11, 0);
	char buf[6];
	sprintf(buf, "%i", TIM6->ARR);
	SH1106_GotoXY(2, 12);
	SH1106_Puts("Period:", &Font_7x10, 1);
	SH1106_GotoXY(2, 23);
	SH1106_Puts(buf, &Font_7x10, 1);
	HAL_TIM_Base_Start_IT(&htim6);
	HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);

}

void updateWaveform(wGen_HandleTypeDef * wGen){
	SH1106_DrawFilledRectangle(2, 0, 80, 40, 0);
	switch(wGen->currentWaveSelected){
		case 0:		// SINE
			SH1106_DrawFilledRectangle(2, 52, 28, 11, 1);
			if(wGen->rotaryDir){
				square(wGen);
				if(wGen->isTransmitting){
					HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
					getSquareVal(wGen);
					HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
				}
			}else{
				ramp(wGen);
				if(wGen->isTransmitting){
					HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
					getRampVal(wGen);
					HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
				}
			}
			SH1106_GotoXY(85, 53);
			SH1106_Puts("50 %", &Font_7x10, 1);
			break;

		case 1:		// SQR

			SH1106_DrawFilledRectangle(2, 52, 28, 11, 1);
			SH1106_GotoXY(2, 53);
			if(wGen->rotaryDir){
				ramp(wGen);
				if(wGen->isTransmitting){
					HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
					getRampVal(wGen);
					HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
				}
			}else{
				sine(wGen);
				if(wGen->isTransmitting){
					HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
					getSineVal(wGen);
					HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
				}
			}
			break;

		case 2:		// RAMP
			if(wGen->rotaryDir){
				sine(wGen);
				if(wGen->isTransmitting){
					HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
					getSineVal(wGen);
					HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
				}
			}else{
				square(wGen);
				if(wGen->isTransmitting){
					HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
					getSquareVal(wGen);
					HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, TX_Bits, samples, DAC_ALIGN_12B_R);
				}
			}
			break;

	}
	SH1106_UpdateScreen();
}
