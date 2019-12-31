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

#define MENU_DISPLAY_DELAY 1000
#define MULTICLICK_TIME 250
#define MINUS_SYM 0x40

#define MAX_CALIB_VAL 99
#define MAX_SLEEP_MINS 30
#define MAX_DEEPSLEEP_MINS 60
#define MAX_FORCE_VAL 100

enum
{
    ENABLE_SOUND,
    CALIBRATION_VAL,
    SLEEP1_VAL,
    SLEEP2_VAL,
    FORCE_VAL,
};

static uint32_t _menuDisplayTime = 0;
static char *_menuNames[] = {"SOU", "CAL", "SL1", "SL2", "FRC"};

extern struct Button _btnPlus;
extern struct Button _btnMinus;
extern uint32_t _haveToSaveData;
extern struct EEPROM_DATA _eepromData;

//
//  TODO:
//
void setup_menu()
{
    int16_t menuIndex = 0;
    _menuDisplayTime = currentMillis() + MENU_DISPLAY_DELAY;

    while (1)
    {
        uint32_t nowTime = currentMillis();
        uint8_t menuAction = checkDoubleClick(&_btnPlus, &menuIndex, 1, nowTime) ||
                             checkDoubleClick(&_btnMinus, &menuIndex, -1, nowTime);
        if (menuAction)
        {
            _menuDisplayTime = nowTime + MENU_DISPLAY_DELAY;
            menuIndex = menuIndex > FORCE_VAL ? 0 : menuIndex < 0 ? FORCE_VAL : menuIndex;
        }

        if (nowTime < _menuDisplayTime)
        {
            S7C_setChars(_menuNames[menuIndex]);
        }
        else
        {
            uint16_t oldSoundValue = _eepromData.enableSound;
            int16_t oldCalibrationValue = _eepromData.calibrationValue;
            uint16_t oldSleepTimeout = _eepromData.sleepTimeout;
            uint16_t oldDeepSleepTimeout = _eepromData.deepSleepTimeout;
            uint16_t oldforceModeIncrement = _eepromData.forceModeIncrement;
            switch (menuIndex)
            {
            case ENABLE_SOUND: // ENABLE SOUND: values 0 or 1
                S7C_setSymbol(0, 0);
                S7C_setSymbol(1, 0);
                checkButton(&_btnPlus, &_eepromData.enableSound, 1, nowTime);  // ADD button
                checkButton(&_btnMinus, &_eepromData.enableSound, 1, nowTime); // MINUS button
                if (oldSoundValue != _eepromData.enableSound)
                {
                    _eepromData.enableSound = (_eepromData.enableSound > 1) ? 0 : _eepromData.enableSound;
                    _haveToSaveData = nowTime;
                }
                S7C_setDigit(2, _eepromData.enableSound);
                break;
            case CALIBRATION_VAL: // CALIBRATION: values from -MAX_CALIB_VAL to MAX_CALIB_VAL
                checkButton(&_btnPlus, &_eepromData.calibrationValue, 1, nowTime);
                checkButton(&_btnMinus, &_eepromData.calibrationValue, -1, nowTime);
                if (oldCalibrationValue != _eepromData.calibrationValue)
                {
                    _eepromData.calibrationValue = (_eepromData.calibrationValue < -MAX_CALIB_VAL) ? -MAX_CALIB_VAL : (_eepromData.calibrationValue > MAX_CALIB_VAL) ? MAX_CALIB_VAL : _eepromData.calibrationValue;
                    _haveToSaveData = nowTime;
                }
                S7C_setSymbol(0, _eepromData.calibrationValue < 0 ? MINUS_SYM : 0);
                S7C_setDigit(1, abs(_eepromData.calibrationValue / 10));
                S7C_setDigit(2, abs(_eepromData.calibrationValue % 10));
                break;
            case SLEEP1_VAL: // SLEEP: values 1..MAX_SLEEP_MINS minutes
                checkButton(&_btnPlus, &_eepromData.sleepTimeout, 1, nowTime);
                checkButton(&_btnMinus, &_eepromData.sleepTimeout, -1, nowTime);
                if (oldSleepTimeout != _eepromData.sleepTimeout)
                {
                    _eepromData.sleepTimeout = (_eepromData.sleepTimeout < 1) ? 1 : (_eepromData.sleepTimeout > MAX_SLEEP_MINS) ? MAX_SLEEP_MINS : _eepromData.sleepTimeout;
                    _haveToSaveData = nowTime;
                }
                S7C_setSymbol(0, 0);
                S7C_setDigit(1, _eepromData.sleepTimeout / 10);
                S7C_setDigit(2, _eepromData.sleepTimeout % 10);
                break;
            case SLEEP2_VAL: // DEEP SLEEP: values SLEEP..MAX_DEEPSLEEP_MINS minutes
                checkButton(&_btnPlus, &_eepromData.deepSleepTimeout, 1, nowTime);
                checkButton(&_btnMinus, &_eepromData.deepSleepTimeout, -1, nowTime);
                _eepromData.deepSleepTimeout = (_eepromData.deepSleepTimeout < _eepromData.sleepTimeout) ? _eepromData.sleepTimeout : (_eepromData.deepSleepTimeout > MAX_DEEPSLEEP_MINS) ? MAX_DEEPSLEEP_MINS : _eepromData.deepSleepTimeout;
                if (oldDeepSleepTimeout != _eepromData.deepSleepTimeout)
                {
                    _haveToSaveData = nowTime;
                }
                S7C_setSymbol(0, 0);
                S7C_setDigit(1, _eepromData.deepSleepTimeout / 10);
                S7C_setDigit(2, _eepromData.deepSleepTimeout % 10);
                break;
            case FORCE_VAL: // FORCE MODE INCREMENT: values 0..100 degrees
                checkButton(&_btnPlus, &_eepromData.forceModeIncrement, 1, nowTime);
                checkButton(&_btnMinus, &_eepromData.forceModeIncrement, -1, nowTime);
                _eepromData.forceModeIncrement = _eepromData.forceModeIncrement > MAX_FORCE_VAL ? MAX_FORCE_VAL : _eepromData.forceModeIncrement;
                if (oldforceModeIncrement != _eepromData.forceModeIncrement)
                {
                    _haveToSaveData = nowTime;
                }
                S7C_setDigit(0, _eepromData.forceModeIncrement / 100);
                S7C_setDigit(1, (_eepromData.forceModeIncrement / 10) % 10);
                S7C_setDigit(2, _eepromData.forceModeIncrement % 10);
                break;
            default:
                S7C_setChars("ERR");
            }
            S7C_setSymbol(3, 0);
        }

        checkPendingDataSave(nowTime);
        S7C_refreshDisplay(nowTime);
        delay_ms(1);
    }
}
