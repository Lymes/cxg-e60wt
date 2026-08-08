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
#include <main.h>
#include <delay.h>
#include <pwm.h>
#include <s7c.h>
#include <adc.h>
#include <eeprom.h>
#include <clock.h>
#include <menu.h>
#include <buttons.h>
#include <uart.h>

#ifndef F_CPU
#warning "F_CPU not defined, using 16MHz by default"
#define F_CPU 16000000UL
#endif

enum WorkingModes
{
    NORMAL_MODE,
    FORCED_MODE,
    SLEEP_MODE,
    DEEPSLEEP_MODE
};

#define MIN_HEAT 50
#define MAX_HEAT 450
#define MAX_ADC_RT 130
#define MIN_ADC_RT 35

#define PWM_POWER_OFF 100
#define SLEEP_TEMP 100
#define EEPROM_SAVE_TIMEOUT 2000
#define HEATPOINT_DISPLAY_DELAY 2500

// Expected ADC reading at 220V AC: V_peak=311V, divider R14(470k)/R16(3.3k) → V_adc=2.17V → ADC=673
// Used to compensate heater power for different mains voltages (110V / 220V)
#define ADC_NOMINAL_220V 673UL

// Hard temperature limit: above this the heater element (A1316) can be damaged
// Triggers immediately, latched until power cycle
#define MAX_SAFE_TEMP 480
// Thermal runaway: if heater is commanded OFF but temp stays >target+30°C for this long,
// transistor Q1 is likely stuck ON
#define RUNAWAY_TIMEOUT_MS 3000UL

uint32_t _haveToSaveData = 0;
static uint32_t _sleepTimer = 0;
static uint32_t _heatPointDisplayTime = 0;
static uint8_t _currentState = NORMAL_MODE;
static uint8_t _overheatFault = 0; // latched: cleared only by power cycle

struct EEPROM_DATA _eepromData;
struct Button _btnPlus  = {PB7, 0, 0, 0, 0, 0, 0};
struct Button _btnMinus = {PB6, 0, 0, 0, 0, 0, 0};

void deepSleep(void);
uint8_t checkSleep(uint32_t nowTime);
void checkHeatPointValidity(void);

void setup(void)
{
    // Configure the clock for maximum speed on the 16MHz HSI oscillator
    // At startup the clock output is divided by 8
    CLK_CKDIVR = 0x0;
    disable_interrupts();
    TIM4_init();
    enable_interrupts();

    uart_init(); // MUST be after CLK_CKDIVR=0 (BRR calculated for 16MHz, 9600 baud)
#ifdef DEBUG
    DBG_PRINTF("B hp=%d c=%d al=%u ah=%u sl=%u ds=%u\r\n",
               _eepromData.heatPoint, _eepromData.calibrationValue,
               _eepromData.adcMinRT, _eepromData.adcMaxRT,
               _eepromData.sleepTimeout, _eepromData.deepSleepTimeout);
#endif

    // Configure mercury sensor and button pins
    pinMode(PB5, INPUT);
    pinMode(PB6, INPUT);
    pinMode(PB7, INPUT);

    // Configure 7-segments display
    S7C_init();
    // Boot splash: all 8s for 1 second — confirms display is alive
    S7C_setDigit(0, 8);
    S7C_setDigit(1, 8);
    S7C_setDigit(2, 8);
    S7C_setSymbol(3, 0);
    {
        uint16_t i;
        for (i = 0; i < 1000; i++) { S7C_refreshDisplay(i); delay_ms(1); }
    }

    // Configure PWM
    pinMode(PD4, OUTPUT);
    PWM_init(PWM_CH1);
    PWM_duty(PWM_CH1, 100); // set heater OFF

    _sleepTimer = currentMillis();
    _heatPointDisplayTime = _sleepTimer + HEATPOINT_DISPLAY_DELAY;

    // EEPROM
    eeprom_read(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
    // First launch, eeprom empty OR -button pressed when power the device
    if (_eepromData.heatPoint == 0 || getPin(PB6) == LOW)
    {
        _eepromData.heatPoint = 270;
        _eepromData.enableSound = 1;
        _eepromData.calibrationValue = 0;
        _eepromData.sleepTimeout = 3;       // 3 min, heatPoint 100C
        _eepromData.deepSleepTimeout = 10;  // 10 min, heatPoint 0
        _eepromData.forceModeIncrement = 0; // 0 degrees
        _eepromData.adcMinRT = MIN_ADC_RT;
        _eepromData.adcMaxRT = MAX_ADC_RT;
        eeprom_write(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
        DBG_PRINTF("EEd\r\n"); // EEPROM defaults written
    }
    // Migration: if new fields are zero (old firmware EEPROM), set defaults
    if (_eepromData.adcMinRT == 0) { _eepromData.adcMinRT = MIN_ADC_RT; DBG_PRINTF("EEm al\r\n"); }
    if (_eepromData.adcMaxRT == 0) { _eepromData.adcMaxRT = MAX_ADC_RT; DBG_PRINTF("EEm ah\r\n"); }

    beepAlarm();
    // Press +button when power the device will enter to Setup Menu
    if (getPin(PB7) == LOW)
    {
        setup_menu();
    }

    uart_init(); // no-op in release; activates PD5/TX in debug builds
    // (already called at top of setup — repeated call is harmless)
    // B=boot, hp=heatPoint, c=cal, al=adcMin, ah=adcMax, sl=sleep1, ds=sleep2
    DBG_PRINTF("B hp=%d c=%d al=%u ah=%u sl=%u ds=%u\r\n",
               _eepromData.heatPoint, _eepromData.calibrationValue,
               _eepromData.adcMinRT, _eepromData.adcMaxRT,
               _eepromData.sleepTimeout, _eepromData.deepSleepTimeout);

    // Now we can switch ON the heater at 50%
    PWM_duty(PWM_CH1, 50);
}

void mainLoop(void)
{
    static uint16_t localCnt = 0;

    uint8_t displaySymbol = 0;
    uint32_t nowTime = currentMillis();

    // Input power sensor
    static uint16_t oldADCUI = 0;
    uint16_t adcUIn = ADC_read(ADC1_CSR_CH1);
    adcUIn = ((oldADCUI * 7) + adcUIn) >> 3; // noise filter
    oldADCUI = adcUIn;

    // Temperature sensor
    static uint16_t oldADCVal = MIN_ADC_RT;
    uint16_t adcVal = ADC_read(ADC1_CSR_CH0);
    adcVal = ((oldADCVal * 7) + adcVal) >> 3; // noise filter
    oldADCVal = adcVal;

    // Degrees value — use per-tip calibrated ADC range
    uint16_t adcRaw = adcVal; // raw filtered ADC — saved before clamping for debug
    adcVal = (adcVal < _eepromData.adcMinRT) ? _eepromData.adcMinRT : adcVal;
    // Cast to uint32_t before the multiply: on STM8/SDCC int is 16-bit, so
    // (MAX_HEAT-MIN_HEAT)*N overflows uint16_t when N>163, silently wrapping
    // currentDegrees to a small value and defeating the OVH protection check.
    int16_t currentDegrees = (int16_t)((uint32_t)(MAX_HEAT - MIN_HEAT) * (adcVal - _eepromData.adcMinRT) / (_eepromData.adcMaxRT - _eepromData.adcMinRT));
    currentDegrees += _eepromData.calibrationValue;

    // ER1: short on sensor  (ADC well below the calibrated cold point)
    // ER2: sensor is broken (ADC above hardware open-circuit threshold)
    // Debounce: require 500 consecutive bad readings (~500ms) to avoid false
    // errors caused by loose mechanical contact on smaller tips
    static uint16_t errorCount = 0;
    static uint8_t _dbgRawErr = 0;    // last logged rawError value
    static uint8_t _dbgErrLogged = 0; // one-shot: confirmed error printed
    uint8_t rawError = (adcVal < (_eepromData.adcMinRT >> 1)) ? 1 : (adcVal > 1000) ? 2 : 0;
    // Log only on transition: "Er<type> adc=<val>" (Er0 = cleared)
    if (rawError != _dbgRawErr)
    {
        DBG_PRINTF("Er%d adc=%u\r\n", rawError, adcVal);
        _dbgRawErr = rawError;
        if (!rawError) _dbgErrLogged = 0; // allow re-log if error re-fires
    }
    if (rawError)
        errorCount = (errorCount < 500) ? errorCount + 1 : 500;
    else
        errorCount = 0;
    uint8_t error = (errorCount >= 500) ? rawError : 0;
    if (error)
    {
        if (!_dbgErrLogged) { DBG_PRINTF("E!%d\r\n", error); _dbgErrLogged = 1; }
        // Periodic print even in error state so UART keeps flowing
        {
            static uint32_t _dbgErrLast = 0;
            if (nowTime - _dbgErrLast >= 500)
            {
                _dbgErrLast = nowTime;
                DBG_PRINTF("ER a=%u v=%u\r\n", adcRaw, adcUIn);
            }
        }
        PWM_duty(PWM_CH1, 100); // switch OFF the heater
        S7C_setChars("ER");
        S7C_setDigit(2, error);
        S7C_refreshDisplay(nowTime);
        beep();
        return;
    }

    // Check for sleep
    static uint8_t oldSleepState = 0;
    uint8_t sleepState = checkSleep(nowTime);
    if (sleepState != oldSleepState)
    {
        // Sl0=normal,1=forced,2=sleep,3=deepsleep
        DBG_PRINTF("Sl%d\r\n", sleepState);
        beepAlarm();
        _currentState = sleepState;
        oldSleepState = sleepState;
    }

    // Check for buttons
    static uint8_t oldAction = 0;
    uint16_t oldHeatPoint = _eepromData.heatPoint;
    uint8_t action = checkButton(&_btnPlus, &_eepromData.heatPoint, 1, nowTime) +  // ADD button
                     checkButton(&_btnMinus, &_eepromData.heatPoint, -1, nowTime); // MINUS button
    if (action)
    {
        // when any buttons were pressed we will display target temperature
        _heatPointDisplayTime = nowTime + HEATPOINT_DISPLAY_DELAY;
        // reset sleep timer on any user interaction
        _sleepTimer = nowTime;
        if (action != oldAction && action > 1) // two butons were pressed
        {
            beepAlarm();
            _currentState = (_currentState == FORCED_MODE) ? NORMAL_MODE : FORCED_MODE;
            DBG_PRINTF("Fm%d\r\n", _currentState == FORCED_MODE ? 1 : 0);
        }
        if (oldHeatPoint != _eepromData.heatPoint)
        {
            checkHeatPointValidity();
            _haveToSaveData = nowTime;
            DBG_PRINTF("Sp%d\r\n", _eepromData.heatPoint);
        }
    }
    oldAction = action;

    // Set target temperature
    int16_t targetHeatPoint = 0;
    switch (_currentState)
    {
    case SLEEP_MODE:
        targetHeatPoint = SLEEP_TEMP;
        break;
    case DEEPSLEEP_MODE:
        targetHeatPoint = 0;
        break;
    case FORCED_MODE:
        targetHeatPoint = _eepromData.heatPoint + _eepromData.forceModeIncrement;
        targetHeatPoint = targetHeatPoint > MAX_HEAT ? MAX_HEAT : targetHeatPoint;
        break;
    case NORMAL_MODE:
    default:
        targetHeatPoint = _eepromData.heatPoint;
    }

    // Setup heater with voltage compensation
    // Power delivered: P = (100-pwmVal)/100 * Vdc^2 / R
    // To keep constant max power regardless of mains voltage (110V or 220V):
    //   minPwm = 100 - 50 * (ADC_220 / adcUIn)^2
    // At 220V (ADC≈673): minPwm=50  (50% power cap, as original)
    // At 110V (ADC≈337): minPwm=0   (100% power, full on)
    int16_t minPwm = 50; // default for 220V in case adcUIn not yet stable
    if (adcUIn > 10)
    {
        uint32_t pwr = (ADC_NOMINAL_220V * ADC_NOMINAL_220V * 50UL) /
                       ((uint32_t)adcUIn * (uint32_t)adcUIn);
        minPwm = (pwr >= 100) ? 0 : (int16_t)(100 - (int16_t)pwr);
    }

    // Control law: linear ramp from minPwm (at diff>=50) to 90 (at diff=0), off when overshot
    // Fixes discontinuity in original formula (90-diff had a jump at diff=50)
    int16_t diff = targetHeatPoint - currentDegrees;
    int16_t pwmVal = (diff < 0)  ? PWM_POWER_OFF :
                     (diff > 50) ? minPwm :
                     minPwm + (int16_t)((90 - minPwm) * (50 - diff) / 50);

    // Debug: periodic snapshot every 500 ms (no-op in release)
    // T=temp(C) S=setpoint e=error a=ADC_raw(temp) v=ADC_raw(vin) p=pwmVal
    {
        static uint32_t _dbgLast = 0;
        if (nowTime - _dbgLast >= 500)
        {
            _dbgLast = nowTime;
            DBG_PRINTF("T=%d S=%d e=%d a=%u v=%u p=%d\r\n",
                       currentDegrees, targetHeatPoint, diff, adcRaw, adcUIn, pwmVal);
        }
    }

    // --- OVERTEMPERATURE PROTECTION ---
    // Hard limit: temperature exceeded safe maximum
    if (currentDegrees > MAX_SAFE_TEMP)
    {
        if (!_overheatFault) DBG_PRINTF("OV%d\r\n", currentDegrees); // one-shot: fault latches
        _overheatFault = 1;
    }
    // Thermal runaway: heater OFF for too long but still way above target
    // (Q1/IRF840 transistor stuck in conduction)
    static uint32_t _heaterOffStart = 0;
    if (diff < -30)
    {
        if (!_heaterOffStart) _heaterOffStart = nowTime;
        if ((nowTime - _heaterOffStart) > RUNAWAY_TIMEOUT_MS)
        {
            if (!_overheatFault) DBG_PRINTF("Rw T=%d\r\n", currentDegrees);
            _overheatFault = 1;
        }
    }
    else
    {
        _heaterOffStart = 0;
    }
    if (_overheatFault)
    {
        PWM_duty(PWM_CH1, PWM_POWER_OFF); // immediate OFF — never skip this
        S7C_setChars("OVH");
        S7C_setDigit(2, 0);
        S7C_refreshDisplay(nowTime);
        beepAlarm();
        return;
    }
    // --- END OVERTEMPERATURE PROTECTION ---

    PWM_duty(PWM_CH1, pwmVal);

    // Setup display value
    // We will show the current heatPoint
    //   * if any button is pressed
    //   * till _heatPointDisplayTime timeout is reached
    //   * when the current temperature is in range ±10 degrees
    uint16_t displayVal = (currentDegrees < 0) ? 0 : currentDegrees;
    uint8_t tempInRange = (displayVal >= targetHeatPoint - 10) && (displayVal <= targetHeatPoint + 10);
    if (nowTime < _heatPointDisplayTime || tempInRange)
    {
        displayVal = targetHeatPoint;
        displaySymbol |= SYM_TEMP;
    }

    // Setup status symbol, flashing using local counter overflow
    displaySymbol |= (_currentState >= SLEEP_MODE) && ((localCnt / 500) % 2) ? SYM_MOON : 0; // 1Hz flashing moon
    displaySymbol |= pwmVal < 100 && ((localCnt / 50) % 2) ? SYM_SUN : 0;                    // 10Hz flashing heater
    displaySymbol |= (_currentState == FORCED_MODE) ? SYM_FARS : 0;                          // F

    if (_currentState != DEEPSLEEP_MODE)
    {
        displaySymbol |= SYM_CELS;
        S7C_setDigit(0, displayVal / 100);
        S7C_setDigit(1, (displayVal / 10) % 10);
        S7C_setDigit(2, displayVal % 10);
    }
    else
    {
        // Set blank display
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

uint8_t checkSleep(uint32_t nowTime)
{
    static uint8_t oldSensorState = 0;
    uint8_t sensorState = getPin(PB5);
    if (sensorState != oldSensorState)
    {
        _sleepTimer = nowTime;
        oldSensorState = sensorState;
        return NORMAL_MODE;
    }
    else if ((nowTime - _sleepTimer) > (uint32_t)_eepromData.deepSleepTimeout * 60000UL)
    {
        //deepSleep();
        return DEEPSLEEP_MODE;
    }
    else if ((nowTime - _sleepTimer) > (uint32_t)_eepromData.sleepTimeout * 60000UL)
    {
        return SLEEP_MODE;
    }
    return NORMAL_MODE;
}

void checkHeatPointValidity(void)
{
    if (_eepromData.heatPoint > MAX_HEAT)
        _eepromData.heatPoint = MAX_HEAT;
    if (_eepromData.heatPoint < MIN_HEAT)
        _eepromData.heatPoint = MIN_HEAT;
}

void checkPendingDataSave(uint32_t nowTime)
{
    if (_haveToSaveData && (nowTime - _haveToSaveData) > EEPROM_SAVE_TIMEOUT)
    {
        DBG_PRINTF("EEs\r\n");
        S7C_setSymbol(3, SYM_SAVE);
        eeprom_write(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
        _haveToSaveData = 0;
    }
}

void deepSleep(void)
{
    static uint16_t localCnt = 0;
    PWM_duty(PWM_CH1, 100); // set heater OFF
    // Set blank display
    S7C_setSymbol(0, 0);
    S7C_setSymbol(1, 0);
    S7C_setSymbol(2, 0);
    while (1)
    {
        uint8_t displaySymbol = ((localCnt / 500) % 2) ? SYM_MOON : 0; // 1Hz flashing moon
        S7C_setSymbol(3, displaySymbol);
        S7C_refreshDisplay(localCnt++);
        delay_ms(1);
    }
}

void main(void)
{
    setup();
    while (1)
    {
        mainLoop();
    }
}
