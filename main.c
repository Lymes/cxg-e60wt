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

// PID controller gains (integer arithmetic, divided by PID_SCALE for actual gain)
// Kp=1.5, Ki=0.2/sample, Kd=1.0 — tune to taste
#define PID_KP         60   // proportional  (×PID_SCALE) — must be high enough to overcome
#define PID_KD        100   // derivative    (×PID_SCALE) — primary brake against overshoot
#define PID_SCALE      10
#define PID_SAMPLE_MS 200   // PID update interval [ms] — long enough for dT to exceed ADC quantization (4°C steps)
// Thermal runaway: if heater is commanded OFF but temp stays >target+60°C for this long,
// transistor Q1 is likely stuck ON. Threshold=60°C accounts for ~40°C thermal inertia overshoot.
#define RUNAWAY_TIMEOUT_MS 8000UL
// Minimum temperature rise above the sliding baseline that is considered a genuine runaway.
// Each ADC step ≈ 4 °C. A stuck IRF840 at full power raises the tip by many tens of °C in
// 8 s — a margin of 20 °C still catches all real faults. A larger value (vs the old 8 °C)
// prevents false OVH from thermal flyback on small tips: at sleep entry the heater element
// is saturated and redistributes ~10-15 °C of stored heat to the thermocouple briefly.
// A stuck IRF840 driving full power raises the tip by >>20 °C over 8 s — still caught.
#define RUNAWAY_RISE_MARGIN 20

uint32_t _haveToSaveData = 0;
static uint32_t _sleepTimer = 0;
static uint32_t _heatPointDisplayTime = 0;
static uint8_t _currentState = NORMAL_MODE;
static uint8_t _overheatFault = 0; // latched: cleared only by power cycle
static uint8_t _tempReached = 0;   // latched: re-armed on setpoint or mode change

struct EEPROM_DATA _eepromData;
struct Button _btnPlus  = {PB7, 0, 0, 0, 0, 0, 0};
struct Button _btnMinus = {PB6, 0, 0, 0, 0, 0, 0};

static uint16_t _adcRange = MAX_ADC_RT - MIN_ADC_RT; // calibrated ADC range, updated from EEPROM in setup()
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

    // Configure mercury sensor and button pins
    pinMode(PB5, INPUT);
    pinMode(PB6, INPUT);
    pinMode(PB7, INPUT);

    // Configure 7-segments display
    S7C_init();

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
        _eepromData.heatPoint = 250;
        _eepromData.enableSound = 1;
        _eepromData.calibrationValue = 0;
        _eepromData.sleepTimeout = 3;       // 3 min, heatPoint 100C
        _eepromData.deepSleepTimeout = 10;  // 10 min, heatPoint 0
        _eepromData.forceModeIncrement = 0; // 0 degrees
        _eepromData.adcMinRT = MIN_ADC_RT;
        _eepromData.adcMaxRT = MAX_ADC_RT;
        eeprom_write(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
        (void)0; // EEPROM defaults written
    }
    // Migration: if new fields are zero (old firmware EEPROM), set defaults
    if (_eepromData.adcMinRT == 0) _eepromData.adcMinRT = MIN_ADC_RT;
    if (_eepromData.adcMaxRT == 0) _eepromData.adcMaxRT = MAX_ADC_RT;
    // heaterType has no sentinel value (0 is valid = 220V default); no migration needed
    _adcRange = _eepromData.adcMaxRT - _eepromData.adcMinRT;

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

    // PD controller state
    static int16_t  _pidPrevError  = 0;
    static uint32_t _pidLastTime   = 0;
    static uint8_t  _pidLastState  = 0xFF;
    static int16_t  pwmVal         = PWM_POWER_OFF;
    static uint8_t  _pidResetDeriv = 0; // set on setpoint change, cleared in PID block
    static int16_t  minPwm         = 50; // voltage compensation, updated every PID_SAMPLE_MS
    static uint32_t _heaterOffStart = 0; // runaway timer start; reset on mode change

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
    int16_t currentDegrees = (int16_t)((uint32_t)(MAX_HEAT - MIN_HEAT) * (adcVal - _eepromData.adcMinRT) / _adcRange);
    currentDegrees += _eepromData.calibrationValue;

    // ER1: short on sensor  (ADC well below the calibrated cold point)
    // ER2: sensor is broken (ADC above hardware open-circuit threshold)
    // Debounce: require 500 consecutive bad readings (~500ms) to avoid false
    // errors caused by loose mechanical contact on smaller tips
    static uint16_t errorCount = 0;
    uint8_t rawError = (adcVal < (_eepromData.adcMinRT >> 1)) ? 1 : (adcVal > 1000) ? 2 : 0;
    if (rawError)
        errorCount = (errorCount < 500) ? errorCount + 1 : 500;
    else
        errorCount = 0;
    uint8_t error = (errorCount >= 500) ? rawError : 0;
    if (error)
    {
        DBG_PRINTF("E!%d a=%u\r\n", error, adcRaw);
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
        beepAlarm();
        _currentState = sleepState;
        oldSleepState = sleepState;
        _tempReached = 0; // re-arm temp-reached beep for new mode target
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
        }
        if (oldHeatPoint != _eepromData.heatPoint)
        {
            checkHeatPointValidity();
            _haveToSaveData = nowTime;
            DBG_PRINTF("Sp%d\r\n", _eepromData.heatPoint);
            // Suppress derivative kick: setpoint step must not look like a
            // temperature measurement change to the D term.
            _pidResetDeriv = 1;
            _tempReached = 0; // re-arm temp-reached beep for new setpoint
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

    // PID controller — evaluated every PID_SAMPLE_MS milliseconds.
    // Positive error = too cold → more heat; negative = too hot → less heat.
    // Heat output [0..100%] maps to PWM duty (inverted: 100=off, minPwm=full power).
    int16_t diff = targetHeatPoint - currentDegrees;

    // On mode transition reset derivative state and runaway timer to avoid false fault
    // (e.g. entering deep sleep with a hot iron: setpoint drops to 0 but iron needs
    // minutes to cool — without this reset the 8s runaway would trip immediately).
    if (_currentState != _pidLastState)
    {
        _pidPrevError = diff;
        _pidLastState = _currentState;
        _heaterOffStart = 0;
    }

    if (nowTime - _pidLastTime >= PID_SAMPLE_MS)
    {
        _pidLastTime = nowTime;

        // Voltage compensation: mains voltage is stable; 200ms update is sufficient.
        // P = (100-pwm)/100 * Vdc^2/R — cap max power for heater rating.
        // pwrConst=50 → 220V heater (A1326, ~800Ω): minPwm=50 at 220V.
        // pwrConst=12 → 110V heater (A1316, ~200Ω): minPwm=50 at 220V, same ~60W at hot.
        if (adcUIn > 10)
        {
            uint32_t pwrConst = (_eepromData.heaterType == 1) ? 12UL : 50UL;
            uint32_t pwr = (ADC_NOMINAL_220V * ADC_NOMINAL_220V * pwrConst) /
                           ((uint32_t)adcUIn * (uint32_t)adcUIn);
            minPwm = (pwr >= 100) ? 0 : (int16_t)(100 - (int16_t)pwr);
        }

        if (_pidResetDeriv) { _pidPrevError = diff; _pidResetDeriv = 0; }

        int16_t derivative = diff - _pidPrevError;
        _pidPrevError = diff;

        if (diff <= 0)
        {
            // Over or at setpoint: cut off immediately.
            // Still update _pidPrevError so the derivative is correct
            // when re-entering the positive zone after an overshoot.
            _pidPrevError = diff;
            pwmVal = PWM_POWER_OFF;
        }
        else
        {
            // PD controller:
            //   P — reduces heat proportionally as target approaches
            //   D — brakes when temperature is rising fast (negative derivative)
            //       anticipates crossing and cuts early without needing to measure overshoot
            int32_t heatScaled = (int32_t)PID_KP * diff
                               + (int32_t)PID_KD * derivative;
            int16_t heatPct = (int16_t)(heatScaled / PID_SCALE);
            if (heatPct > 100) heatPct = 100;
            if (heatPct <   0) heatPct =   0;

            // Power taper: cap max heat proportionally to distance from target.
            // Floor of 15% ensures enough power to overcome thermal losses at
            // steady state — without it the iron stalls below setpoint.
            // At diff=50 → 65% max, diff=10 → 25%, diff=0 → cut off (diff<=0 branch above).
            {
                int16_t maxHeat = diff + 15;
                if (heatPct > maxHeat) heatPct = maxHeat;
            }

            pwmVal = (int16_t)(100 - (int32_t)heatPct * (100 - minPwm) / 100);
            if (pwmVal < minPwm)        pwmVal = minPwm;
            if (pwmVal > PWM_POWER_OFF) pwmVal = PWM_POWER_OFF;
        }
    }

    // Debug: periodic snapshot every 200 ms (no-op in release)
    // T=temp S=setpoint e=error d=derivative a=ADC_raw(temp) v=ADC_raw(vin) p=pwmVal
    {
        static uint32_t _dbgLast = 0;
        if (nowTime - _dbgLast >= 200)
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
    // Thermal runaway: heater OFF but temperature RISING → Q1/IRF840 transistor stuck.
    // Logic:
    //   • baseline = temperature when the window starts (or last time T fell)
    //   • if T falls below baseline → iron is cooling, slide baseline down, reset clock
    //   • if 8s elapses AND T rose above baseline → genuine stuck transistor → fault
    //   • if 8s elapses AND T unchanged → very slow cooling (ADC quantization); extend window
    // A stuck IRF840 drives full ~60W so T will rise several °C/s — easily caught.
    // Natural cooling can legitimately hold one ADC step (4°C) for >8s; we must not fault.
    static int16_t _runawayBaseTemp = 0;
    if (diff < -60)
    {
        if (!_heaterOffStart)
        {
            _heaterOffStart = nowTime;
            _runawayBaseTemp = currentDegrees;
        }
        else if (currentDegrees < _runawayBaseTemp)
        {
            // Temperature fell — iron is cooling; slide window forward
            _heaterOffStart = nowTime;
            _runawayBaseTemp = currentDegrees;
        }
        else if ((nowTime - _heaterOffStart) > RUNAWAY_TIMEOUT_MS)
        {
            if (currentDegrees > _runawayBaseTemp + RUNAWAY_RISE_MARGIN)
            {
                // Temperature rose clearly above baseline — transistor Q1 is stuck ON.
                // Margin of RUNAWAY_RISE_MARGIN (5 ADC steps, 20 °C) is large enough to
                // absorb thermal flyback on small tips at sleep entry, while still catching
                // a stuck IRF840 which raises the tip by many tens of °C in 8 s.
                if (!_overheatFault) DBG_PRINTF("Rw T=%d\r\n", currentDegrees);
                _overheatFault = 1;
            }
            else
            {
                // Temperature unchanged or within noise floor — extend window
                _heaterOffStart = nowTime;
            }
        }
    }
    else
    {
        _heaterOffStart = 0;
    }
    if (_overheatFault)
    {
        static uint8_t _faultBeeped = 0;
        PWM_duty(PWM_CH1, PWM_POWER_OFF); // immediate OFF — never skip this
        S7C_setChars("OVH");
        S7C_refreshDisplay(nowTime);
        if (!_faultBeeped) { _faultBeeped = 1; beepAlarm(); }
        return;
    }
    // --- END OVERTEMPERATURE PROTECTION ---

    // Re-clamp minPwm here: pwmVal is updated every PID_SAMPLE_MS but minPwm
    // is recalculated every ms from live adcUIn — enforce voltage limit at callsite.
    {
        int16_t safeVal = pwmVal;
        if (safeVal < minPwm) safeVal = minPwm;
        PWM_duty(PWM_CH1, safeVal);
    }

    // Setup display value
    // We will show the current heatPoint
    //   * if any button is pressed
    //   * till _heatPointDisplayTime timeout is reached
    //   * when the current temperature is in range ±10 degrees
    uint16_t displayVal = (currentDegrees < 0) ? 0 : currentDegrees;
    uint8_t tempInRange = (displayVal >= targetHeatPoint - 10) && (displayVal <= targetHeatPoint + 10);
    // Temp-reached beep: fire once when iron first hits setpoint.
    // _tempReached is re-armed (cleared) on every setpoint or mode change,
    // so the beep fires exactly once per new target — even if the temperature
    // later oscillates around the setpoint.
    // Skip in DEEPSLEEP_MODE (target = 0, iron just cooling down).
    if (tempInRange && !_tempReached
        && (_currentState == NORMAL_MODE || _currentState == FORCED_MODE))
    {
        _tempReached = 1;
        beep();
    }
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
        S7C_setSymbol(3, SYM_SAVE);
        eeprom_write(EEPROM_START_ADDR, &_eepromData, sizeof(_eepromData));
        _haveToSaveData = 0;
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
