//
//  stm8s_pins.h
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

#ifndef _STM8S_PINS_H_
#define _STM8S_PINS_H_

#include <stm8s.h>
#include <stdint.h>

#define LOW 0
#define HIGH 1

#define false 0
#define true 1

enum MODE
{
    INPUT,
    OUTPUT
};

enum PIN
{
    PA0,
    PA1,
    PA2,
    PA3,
    PA4,
    PA5,
    PA6,
    PA7,

    PB0,
    PB1,
    PB2,
    PB3,
    PB4,
    PB5,
    PB6,
    PB7,

    PC0,
    PC1,
    PC2,
    PC3,
    PC4,
    PC5,
    PC6,
    PC7,

    PD0,
    PD1,
    PD2,
    PD3,
    PD4,
    PD5,
    PD6,
    PD7,

    PE0,
    PE1,
    PE2,
    PE3,
    PE4,
    PE5,
    PE6,
    PE7,
};

#define BIT(pin) ((pin) % 8)
#define ODR(pin) (_SFR_(PA_BASE_ADDRESS + ((pin) >> 3) * 5 + 0x00))
#define IDR(pin) (_SFR_(PA_BASE_ADDRESS + ((pin) >> 3) * 5 + 0x01))
#define DDR(pin) (_SFR_(PA_BASE_ADDRESS + ((pin) >> 3) * 5 + 0x02))
#define CR1(pin) (_SFR_(PA_BASE_ADDRESS + ((pin) >> 3) * 5 + 0x03))
#define CR2(pin) (_SFR_(PA_BASE_ADDRESS + ((pin) >> 3) * 5 + 0x04))

#define configure_as_input(pin) (DDR(pin) &= ~(1 << BIT(pin)), CR1(pin) |= (1 << BIT(pin)))
#define configure_as_output(pin) (DDR(pin) |= (1 << BIT(pin)), CR1(pin) |= (1 << BIT(pin)), CR2(pin) |= (1 << BIT(pin)))

#define set_high(pin) (ODR(pin) |= (1 << BIT(pin)))
#define set_low(pin) (ODR(pin) &= ~(1 << BIT(pin)))

#define pinMode(pin, mode) (((mode) == INPUT) ? configure_as_input(pin) : configure_as_output(pin))
#define setPin(pin, val) (((val) == 0) ? set_low(pin) : set_high(pin))
#define getPin(pin) (IDR(pin) & (1 << BIT(pin)))

#endif // _STM8S_PINS_H_