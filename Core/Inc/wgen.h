/*
 *
 *  Created on: 06/18/2024
 *  Author: Michael Kurta
 *  ----------------------------------------------------------------------
   	Copyright (C) Michael Kurta, 2024

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
   ----------------------------------------------------------------------
 */


#ifndef FGEN_H_
#define FGEN_H_

#define SWITCH_DEBOUNCE_MS 			50
#define BTN_CLICKED 				(btnClickTrigger)

#define CURSOR_DELAY_MS				100

#define ROTARY_COUNTER_START		0x3FFD
#define ROTARY_PULSES_PER_TICK		2

#define MAIN_MENU_OPTIONS			7
#define NUMBER_SELECT_DIVS			10

#define CURSOR_WAVEFORM_XPOS		16
#define CURSOR_FREQ_HUNDRED_XPOS	36
#define CURSOR_FREQ_TENS_XPOS		43
#define CURSOR_FREQ_ONES_XPOS		50
#define CURSOR_FREQ_UNITS_XPOS		70
#define CURSOR_PERCENT_XPOS			105
#define CURSOR_TX_YPOS				18
#define CURSOR_TX_XPOS				85

#define DEFAULT_HZ					100
#define MAX_FREQ_KHZ				200

#define MAX_SAMPLES_PER_REV			4000

#define RESOLUTION_12BIT			4096

#define ROTARY_DIRECTION_ANTICLOCK	-1
#define ROTARY_DIRECTION_NONE		0
#define ROTARY_DIRECTION_CLOCK		1

#define TX_BUF_SIZE_MAX_250_HZ		4000
#define TX_BUF_SIZE_MAX_500_HZ		2000
#define TX_BUF_SIZE_MAX_5000_HZ		1000
#define TX_BUF_SIZE_MAX_10000_HZ	500
#define TX_BUF_SIZE_MAX_20000_HZ	250
#define TX_BUF_SIZE_MAX_100000_HZ	100
#define TX_BUF_SIZE_MAX_200000_HZ	50
#define TX_BUF_SIZE_MAX_999000_HZ	10

#define TIMX_UG_BIT					0


#define DISPLAY_UNITS_KHZ			1
#define DISPLAY_UNITS_HZ			0


#include "stm32h7xx_hal.h"
#include "stdio.h"
#include "main.h"

typedef struct {

	uint8_t		clickConsumed;			// Flag to indicate button press
	uint32_t 	counter;				// Rotary counter
	uint16_t	currentBufSize;			// Output buffer size
    int8_t		currentStateBtn;		// Button state
    uint16_t 	currentStateClk;		// Input from rotary encoder (CLK)
    int8_t		currentMenuPos;			// Main menu Pos
    uint8_t		currentWaveSelected;	// Wave type
    uint8_t		currentPercent;			// Pulse duty % -> square wave //
    uint32_t	frequency;
    uint8_t		isPressed;
    uint8_t		isTransmitting;
    uint32_t 	lastUpdate;
    uint32_t	lastPress;
    uint8_t		longPressTrigger;
    uint8_t		menuMode;
    uint32_t 	millisStart;			// Timer start millis
    int8_t		previousMenuPos;
    int8_t     	previousPressState;
    uint32_t 	previousPressTime;
    uint16_t 	previousStateClk;
    int8_t 		rotaryDir;
    int8_t 		stateChange;
    uint8_t		unitDisplay;

} wGen_HandleTypeDef;

wGen_HandleTypeDef wGen_create();



//*********************Display Strings********************//


void lcdInit(wGen_HandleTypeDef * wGen);

void buttonUpdate(wGen_HandleTypeDef * wGen);

void checkSampleChange(wGen_HandleTypeDef * wGen);

void consumeClick(wGen_HandleTypeDef * wGen);

void eraseCursor(wGen_HandleTypeDef * wGen);

void exitToMain(wGen_HandleTypeDef * wGen);

void getRampVal(wGen_HandleTypeDef * wGen);

void getSamples(wGen_HandleTypeDef * wGen);

void getSquareVal(wGen_HandleTypeDef * wGen);

void getSineVal(wGen_HandleTypeDef * wGen);

void loopUpdate(wGen_HandleTypeDef * wGen);

void ramp(wGen_HandleTypeDef * wGen);

void selectHundreds(wGen_HandleTypeDef * wGen);

void selectTens(wGen_HandleTypeDef * wGen);

void selectOnes(wGen_HandleTypeDef * wGen);

void selectUnits(wGen_HandleTypeDef * wGen);

void selectPercent(wGen_HandleTypeDef * wGen);

void selectTransmit(wGen_HandleTypeDef * wGen);

void selectWaveform(wGen_HandleTypeDef * wGen);

void sine(wGen_HandleTypeDef * wGen);

void square(wGen_HandleTypeDef * wGen);

void updateBitmap(wGen_HandleTypeDef * wGen);

void updateOutputFrequency(wGen_HandleTypeDef * wGen);

void updateHundreds(wGen_HandleTypeDef * wGen);

void updateTens(wGen_HandleTypeDef * wGen);

void updateOnes(wGen_HandleTypeDef * wGen);

void updatePercent(wGen_HandleTypeDef * wGen);

void updateRotarySel(wGen_HandleTypeDef * wGen);

void updateTimerPeriod(wGen_HandleTypeDef * wGen);

void updateWaveform(wGen_HandleTypeDef * wGen);

#endif
