//
//  menu.c
//  cxg-60ewt
//
//  Created by Leonid Mesentsev on 26/11/2019.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include <stm8s.h>
#include <stm8s_pins.h>
#include <main.h>
#include <delay.h>
#include <eeprom.h>
#include <clock.h>
#include <s7c.h>
#include <buttons.h>

#define abs(x) (((x) < 0) ? -(x) : (x))

#define MENU_DISPLAY_DELAY 5000
#define MULTICLICK_TIME 250

extern struct EEPROM_DATA _eepromData;

static char *_menuNames[] = {"SOU", "CAL", "SL1", "SL2"};

static uint32_t _menuDisplayTime = 0;
extern struct Button _btnPlus;
extern struct Button _btnMinus;
extern uint32_t _haveToSaveData;

//
//  TODO:
//
void setup_menu()
{
    _menuDisplayTime = currentMillis() + MENU_DISPLAY_DELAY;

    int16_t menuIndex = 0;

    while (1)
    {
        uint32_t nowTime = currentMillis();

        uint8_t menuAction = checkDoubleClick(&_btnPlus, &menuIndex, 1, nowTime) ||
                             checkDoubleClick(&_btnMinus, &menuIndex, -1, nowTime);
        if (menuAction)
        {
            _menuDisplayTime = nowTime + MENU_DISPLAY_DELAY;
            menuIndex = menuIndex > 3 ? 0 : menuIndex < 0 ? 3 : menuIndex;
        }

        if (nowTime < _menuDisplayTime)
        {
            S7C_setChars(_menuNames[menuIndex]);
        }
        else
        {
            uint8_t changed = false;
            uint16_t oldSoundValue = _eepromData.enableSound;
            int16_t oldCalibrationValue = _eepromData.calibrationValue;
            uint16_t oldSleepTimeout = _eepromData.sleepTimeout;
            uint16_t oldDeepSleepTimeout = _eepromData.deepSleepTimeout;
            switch (menuIndex)
            {
            case 0: // ENABLE SOUND: values 0 or 1
                S7C_setSymbol(0, 0);
                S7C_setSymbol(1, 0);
                checkButton(&_btnPlus, &_eepromData.enableSound, 1, nowTime);  // ADD button
                checkButton(&_btnMinus, &_eepromData.enableSound, 1, nowTime); // MINUS button
                if (oldSoundValue != _eepromData.enableSound)
                {
                    _eepromData.enableSound = (_eepromData.enableSound > 1) ? 0 : _eepromData.enableSound;
                    changed = true;
                }
                S7C_setDigit(2, _eepromData.enableSound);
                break;
            case 1:                                                                  // CALIBRATION: values from -99 to 99
                checkButton(&_btnPlus, &_eepromData.calibrationValue, 1, nowTime);   // ADD button
                checkButton(&_btnMinus, &_eepromData.calibrationValue, -1, nowTime); // MINUS button
                if (oldCalibrationValue != _eepromData.calibrationValue)
                {
                    _eepromData.calibrationValue = (_eepromData.calibrationValue < -99) ? -99 : (_eepromData.calibrationValue > 99) ? 99 : _eepromData.calibrationValue;
                    changed = true;
                }
                S7C_setSymbol(0, _eepromData.calibrationValue < 0 ? 0x40 : 0);
                S7C_setDigit(1, abs(_eepromData.calibrationValue / 10));
                S7C_setDigit(2, abs(_eepromData.calibrationValue % 10));
                break;
            case 2:                                                              // SLEEP: values 0-30 minutes
                checkButton(&_btnPlus, &_eepromData.sleepTimeout, 1, nowTime);   // ADD button
                checkButton(&_btnMinus, &_eepromData.sleepTimeout, -1, nowTime); // MINUS button
                if (oldSleepTimeout != _eepromData.sleepTimeout)
                {
                    _eepromData.sleepTimeout = (_eepromData.sleepTimeout < 1) ? 1 : (_eepromData.sleepTimeout > 30) ? 30 : _eepromData.sleepTimeout;
                    changed = true;
                }
                S7C_setSymbol(0, 0);
                S7C_setDigit(1, _eepromData.sleepTimeout / 10);
                S7C_setDigit(2, _eepromData.sleepTimeout % 10);
                break;
            case 3:                                                                  // DEEP SLEEP: values SLEEP-60 minutes
                checkButton(&_btnPlus, &_eepromData.deepSleepTimeout, 1, nowTime);   // ADD button
                checkButton(&_btnMinus, &_eepromData.deepSleepTimeout, -1, nowTime); // MINUS button
                if (oldDeepSleepTimeout != _eepromData.deepSleepTimeout)
                {
                    _eepromData.deepSleepTimeout = (_eepromData.deepSleepTimeout < _eepromData.sleepTimeout) ? _eepromData.sleepTimeout : (_eepromData.deepSleepTimeout > 60) ? 60 : _eepromData.deepSleepTimeout;
                    changed = true;
                }
                S7C_setSymbol(0, 0);
                S7C_setDigit(1, _eepromData.deepSleepTimeout / 10);
                S7C_setDigit(2, _eepromData.deepSleepTimeout % 10);
                break;
            default:
                S7C_setChars("ERR");
            }
            if (changed)
            {
                _haveToSaveData = nowTime;
                changed = false;
            }
            S7C_setSymbol(3, 0);
        }

        checkPendingDataSave(nowTime);
        S7C_refreshDisplay(nowTime);
        delay_ms(1);
    }
}
