//
//  main.c
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
#include <delay.h>
#include <pwm.h>
#include <s7c.h>
#include <adc.h>
#include <eeprom.h>
#include <clock.h>

#ifndef F_CPU
#warning "F_CPU not defined, using 16MHz by default"
#define F_CPU 16000000UL
#endif

enum
{
    NOSLEEP,
    SLEEP,
    DEEPSLEEP
};

#define SHORT_PRESS 700
#define LONG_PRESS 1800
#define FAST_INCREMENT 80
#define MIN_HEAT 50
#define MAX_HEAT 450
#define EEPROM_SAVE_TIMEOUT 2000
#define HEATPOINT_DISPLAY_DELAY 5000
#define SLEEP_TEMP 100
#define MAX_ADC_RT 130
#define MIN_ADC_RT 40

struct EEPROM_DATA
{
    uint16_t heatPoint;
    uint32_t sleepTimeout;
    uint32_t deepSleepTimeout;
};

static uint32_t _sleepTimer = 0;
static uint32_t _buttonTimer = 0;
static uint32_t _longPressTimer = 0;
static uint32_t _haveToSaveData = 0;
static uint32_t _heatPointDisplayTime = 0;

static struct EEPROM_DATA _eepromData;

uint8_t checkSleep(uint32_t nowTime);
uint8_t checkButtons(uint32_t nowTime);
void checkHeatPointValidity();
void checkPendingDataSave(uint32_t nowTime);

void setup()
{
    // Configure the clock for maximum speed on the 16MHz HSI oscillator
    // At startup the clock output is divided by 8
    CLK_CKDIVR = 0x0;
    disable_interrupts();
    TIM4_init();
    enable_interrupts();

    // Configure 7-segments display
    S7C_init();

    // Configure PWM
    pinMode(PD4, OUTPUT);
    PWM_init(PWM_CH1);
    PWM_duty(PWM_CH1, 50);

    beepAlarm();
    _sleepTimer = currentMillis();
    _heatPointDisplayTime = _sleepTimer + HEATPOINT_DISPLAY_DELAY;

    // EEPROM
    eeprom_read(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
    if (_eepromData.heatPoint == 0) // First launch, eprom empty
    {
        _eepromData.heatPoint = 270;
        _eepromData.sleepTimeout = 60000;      // 1 min, heatPoint 100C
        _eepromData.deepSleepTimeout = 600000; // 10 min, heatPoint 0
        eeprom_write(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
    }
}

void mainLoop()
{
    static uint8_t oldSleep = 0;
    static uint16_t oldADCVal = 0;
    static uint16_t oldADCUI = 0;
    static uint16_t localCnt = 0;
    uint8_t displaySymbol = SYM_CELS;
    static uint16_t oldDisplayValue = 0;
    uint32_t nowTime = currentMillis();

    // Input power sensor
    uint16_t adcUIn = ADC_read(ADC1_CSR_CH1);
    adcUIn = ((oldADCUI * 7) + adcUIn) >> 3; // noise filter
    oldADCUI = adcUIn;

    // Temperature sensor
    uint16_t adcVal = ADC_read(ADC1_CSR_CH0);
    adcVal = ((oldADCVal * 7) + adcVal) >> 3; // noise filter
    oldADCVal = adcVal;

    // ER1: short on sensor
    // ER2: sensor is broken
    uint8_t error = (adcVal < 10) ? 1 : (adcVal > 1000) ? 2 : 0;
    if (error)
    {
        PWM_duty(PWM_CH1, 100); // switch OFF the heater
        S7C_setChars("ER");
        S7C_setDigit(2, error);
        S7C_refreshDisplay(nowTime);
        beep();
        return;
    }

    uint8_t sleep = checkSleep(nowTime);
    if (oldSleep != sleep)
    {
        beepAlarm();
    }
    oldSleep = sleep;

    // Degrees value
    uint16_t displayVal = (MAX_HEAT - MIN_HEAT) * (adcVal - MIN_ADC_RT) / (MAX_ADC_RT - MIN_ADC_RT);

    // 50 degrees before the heatPoint we start to slow down the heater
    // before that we keep the heater at 50%
    // if the diff is negative, we'll stop the heater
    int16_t diff = (sleep == SLEEP) ? SLEEP_TEMP - displayVal : _eepromData.heatPoint - displayVal;
    int16_t pwmVal = (sleep == DEEPSLEEP || diff < 0) ? 100 : (diff > 50) ? 50 : 100 - diff;
    PWM_duty(PWM_CH1, pwmVal);

    uint8_t action = checkButtons(nowTime);
    checkHeatPointValidity();
    if (action || nowTime < _heatPointDisplayTime)
    {
        displayVal = _eepromData.heatPoint;
        displaySymbol |= SYM_TERM;
    }

    displaySymbol |= sleep && ((localCnt / 500) % 2) ? SYM_MOON : 0;      // 1Hz flashing moon
    displaySymbol |= pwmVal < 100 && ((localCnt / 50) % 2) ? SYM_SUN : 0; // 10Hz flashing heater

    //displayVal = ((oldDisplayValue * 7) + displayVal) >> 3; // noise filter
    //oldDisplayValue = displayVal;

    if (sleep != DEEPSLEEP)
    {
        S7C_setDigit(0, displayVal / 100);
        S7C_setDigit(1, (displayVal % 100) / 10);
        S7C_setDigit(2, displayVal % 10);
    }
    else
    {
        S7C_setSymbol(0, 0);
        S7C_setSymbol(1, 0);
        S7C_setSymbol(2, 0);
    }
    S7C_setSymbol(3, displaySymbol);

    checkPendingDataSave(nowTime);
    S7C_refreshDisplay(nowTime);
    localCnt++;
    delay_ms(1);
}

void main()
{
    setup();
    while (1)
    {
        mainLoop();
    }
}

uint8_t checkSleep(uint32_t nowTime)
{
    static uint8_t oldSensorState = 0;
    uint8_t sensorState = getPin(PB5);
    if (sensorState != oldSensorState)
    {
        _sleepTimer = nowTime;
        return LOW;
    }
    if ((nowTime - _sleepTimer) > _eepromData.deepSleepTimeout)
    {
        return DEEPSLEEP;
    }
    else if ((nowTime - _sleepTimer) > _eepromData.sleepTimeout)
    {
        return SLEEP;
    }
    return NOSLEEP;
}

void checkHeatPointValidity()
{
    if (_eepromData.heatPoint > MAX_HEAT)
        _eepromData.heatPoint = MAX_HEAT;
    if (_eepromData.heatPoint < MIN_HEAT)
        _eepromData.heatPoint = MIN_HEAT;
}

uint8_t checkButtons(uint32_t nowTime)
{
    static uint8_t skipCounter = 0;
    if (getPin(PB7) == LOW) // PLUS BUTTON
    {
        if (!_buttonTimer)
            _buttonTimer = nowTime;
        if (!_longPressTimer)
            _longPressTimer = _buttonTimer;
        if ((nowTime - _longPressTimer) > LONG_PRESS)
        {
            if (!(skipCounter++ % FAST_INCREMENT))
            {
                _eepromData.heatPoint++;
                _haveToSaveData = nowTime;
            }
        }
        else if ((nowTime - _buttonTimer) > SHORT_PRESS)
        {
            _eepromData.heatPoint++;
            _haveToSaveData = nowTime;
            _buttonTimer = 0;
            beep();
        }
        _heatPointDisplayTime = nowTime + HEATPOINT_DISPLAY_DELAY;
    }
    else if (getPin(PB6) == LOW) // MINUS BUTTON
    {
        if (!_buttonTimer)
            _buttonTimer = nowTime;
        if (!_longPressTimer)
            _longPressTimer = _buttonTimer;
        if ((nowTime - _longPressTimer) > LONG_PRESS)
        {
            if (!(skipCounter++ % FAST_INCREMENT))
            {
                _eepromData.heatPoint--;
                _haveToSaveData = nowTime;
            }
        }
        else if ((nowTime - _buttonTimer) > SHORT_PRESS)
        {
            _eepromData.heatPoint--;
            _haveToSaveData = nowTime;
            _buttonTimer = 0;
            beep();
        }
        _heatPointDisplayTime = nowTime + HEATPOINT_DISPLAY_DELAY;
    }
    else
    {
        _buttonTimer = 0;
        _longPressTimer = 0;
        return LOW;
    }
    return HIGH;
}

void checkPendingDataSave(uint32_t nowTime)
{
    if (_haveToSaveData && (nowTime - _haveToSaveData) > EEPROM_SAVE_TIMEOUT)
    {
        S7C_setSymbol(3, SYM_SAVE);
        S7C_refreshDisplay(nowTime);
        eeprom_write(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
        _haveToSaveData = 0;
    }
}
