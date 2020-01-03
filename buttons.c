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

#include <buttons.h>
#include <stm8s.h>
#include <stm8s_pins.h>
#include <delay.h>
#include <clock.h>
#include <string.h>

#define DEBOUNCE_TIME 10
#define MULTICLICK_TIME 180
#define SHORT_PRESS 300
#define LONG_PRESS 900
#define FAST_INCREMENT 40

uint8_t checkButton(struct Button *btn, int16_t *value, int8_t increment, uint32_t nowTime)
{
    static uint8_t skipCounter = 0;
    if (getPin(btn->pin) == LOW)
    {
        if (!btn->lastBounceTime)
            btn->lastBounceTime = nowTime;
        if (!btn->longPressTimer)
            btn->longPressTimer = btn->lastBounceTime;
        if ((nowTime - btn->longPressTimer) > LONG_PRESS)
        {
            if (!(skipCounter++ % FAST_INCREMENT))
            {
                *value += increment;
            }
        }
        else if ((nowTime - btn->lastBounceTime) > SHORT_PRESS)
        {
            *value += increment;
            btn->lastBounceTime = 0;
            beep();
        }
        if (nowTime - btn->lastBounceTime > DEBOUNCE_TIME)
        {
            return HIGH;
        }
    }
    else
    {
        btn->lastBounceTime = 0;
        btn->longPressTimer = 0;
    }
    return LOW;
}

uint8_t checkDoubleClick(struct Button *btn, int16_t *value, int8_t increment, uint32_t nowTime)
{
    uint8_t clicks = 0;
    uint8_t btnState = getPin(btn->pin);
    // Make the button logic active-high in code
    btnState = !btnState;

    // If the switch changed, due to noise or a button press, reset the debounce timer
    if (btnState != btn->lastState)
    {
        btn->lastBounceTime = nowTime;
    }

    // Debounce the button (check if a stable, changed state has occured)
    if (nowTime - btn->lastBounceTime > DEBOUNCE_TIME && btnState != btn->depressed)
    {
        btn->depressed = btnState;
        if (!btn->depressed)
            btn->clickCount++;
    }
    btn->lastState = btnState;

    // If the button released state is stable, report number of clicks and start new cycle
    if (btn->depressed && (nowTime - btn->lastBounceTime) > MULTICLICK_TIME)
    {
        // positive count for released buttons
        clicks = btn->clickCount;
        btn->clickCount = 0;
        if (clicks > 1)
        {
            *value += increment;
            beep();
            return HIGH;
        }
    }
    return LOW;
}