//
//  clock.c
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

#include <clock.h>
#include <main.h>

#define BEEP_DURATION 100

extern struct EEPROM_DATA _eepromData;

// internal clock counter, will overflow every 49 days ;)
volatile uint32_t _currentMsecs = 0;

volatile uint8_t _beep1 = 0;
volatile uint8_t _beep2 = 0;
volatile uint8_t _duration = 0;

//void TIM1_overflow_handler() __interrupt(11)
void TIM4_overflow_handler() __interrupt(TIM4_UPD_OVF)
{
    static uint8_t localCnt = 0;
    _currentMsecs++;
    //    TIM1_SR1 &= ~1;
    TIM4_SR &= ~1;

    if (_beep1 < BEEP_DURATION)
    {
        if (localCnt++ % 2) // 500Hz
            PA_ODR ^= (1 << 3);
        _beep1++;
    }
    else if (_beep2 < BEEP_DURATION) // 1kHz
    {
        PA_ODR ^= (1 << 3);
        _beep2++;
    }
    else if (_duration) // repeat
    {
        _beep1 = _beep2 = 0;
        _duration--;
    }
}

void TIM4_init()
{
    PA_DDR |= (1 << 3); // configure PA3 as output
    PA_CR1 |= (1 << 3); // push-pull mode
    PA_CR2 |= (1 << 3); // push-pull mode

    // TIM1_PSCRH = 0x00; // Configure timer
    // TIM1_PSCRL = 0x07;
    // TIM1_ARRH = 0x03;
    // TIM1_ARRL = 0xe7;
    // TIM1_CR1 = 0x01; // Enable timer
    // TIM1_IER = 0x01; // Enable interrupt - update event

    // F = F_CPU / ( 2 * Prescaler * ( 1 + ARR ) )
    // F = 16 MHz / ( 2 * 64 * ( 1 + 124 ) ) = 1000Hz
    TIM4_PSCR = 6;
    TIM4_ARR = 0x7c;
    TIM4_IER = (1 << TIM4_IER_UIE);
    TIM4_CR1 = (1 << TIM4_CR1_CEN);
}

void beep()
{
    if (!_eepromData.enableSound)
        return;
    _beep1 = 0;
    _beep2 = 0;
    _duration = 0;
}

void beepAlarm()
{
    if (!_eepromData.enableSound)
        return;
    _beep1 = 0;
    _beep2 = 0;
    _duration = 3;
}

uint32_t currentMillis()
{
    return _currentMsecs;
}
