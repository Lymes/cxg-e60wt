//
//  pwm.c
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
#include <pwm.h>

/******************************************************************************
 *
 *  Initialize PWM
 *  in: mode, channels to use
 * 
 * To generate the PWM signals the TIM2 peripheral must be configured as follows:
 *  ● Output state enabled for each channel
 *  ● Output compare active low for each channel
 *  ● Preload register enabled for each channel
 *  ● PWM output signal frequency = 2 KHz:
 *      – The timer source clock frequency is 2 MHz (fCPU by default) and the prescaler is
 *        set to 1 to obtain a TIM2 counter clock of 2 MHz.
 *      – PWM output signal frequency can be set according to the following equation:
 *           PWM output signal frequency = TIM2 counter clock/(TIM2_ARR + 1)
 *           (in our case TIM2_ARR = 999, so PWM output signal frequency is 2 KHz)
 *  ● PWM mode for each channel. To obtain a different PWM duty cycle value on each channel the TIM2_CCRxx register must be set according to this equation:
 *    Channel x duty cycle = [TIM2_CCRxx/(TIM2_ARR + 1)] * 100
 *  
 *  By default we have:
 *  – Channel 1: TIM2_CCR1x register value is 500, so channel 1 of TIM2 generates a PWM signal with a frequency of 2 KHz and a duty cycle of 50%.
 *  – Channel 2: TIM2_CCR2x register value is 750, so channel 2 of TIM2 generates a PWM signal with a frequency of 2 KHz and a duty cycle of 75%.
 *  – Channel 3: TIM2_CCR3x register value is 250, so channel 3 of TIM2 generates a PWM signal with a frequency of 2 KHz and a duty cycle of 25%.
 * 
 */

void PWM_init(uint8_t ch)
{
    TIM2_PSCR = 3;
    int _TIM2_ARR = 999; /* 2khz */
    TIM2_ARRH = (_TIM2_ARR >> 8) & 0xff;
    TIM2_ARRL = _TIM2_ARR & 0xff;
    if (ch & PWM_CH1)
    {
        TIM2_CCR1H = 0;
        TIM2_CCR1L = 0;
        TIM2_CCMR1 = 0x68;  /* PWM mode 1, use preload register */
        TIM2_CCER1 |= 0x01; /* output enable, normal polarity */
    }
    if (ch & PWM_CH2)
    {
        TIM2_CCR2H = 0;
        TIM2_CCR2L = 0;
        TIM2_CCMR2 = 0x68;
        TIM2_CCER1 |= 0x10;
    }
    if (ch & PWM_CH3)
    {
        TIM2_CCR3H = 0;
        TIM2_CCR3L = 0;
        TIM2_CCMR3 = 0x68;
        TIM2_CCER2 |= 0x01;
    }
    TIM2_CR1 = 0x81; /* use TIM2_ARR preload register, enable */
}

/******************************************************************************
 *
 *  Set duty count
 *  in: channel(s), new duty count (times 1/2 microsecond)
 */

void PWM_duty(uint8_t ch, uint16_t duty)
{
    duty = duty * 10;
    uint8_t dH, dL;

    dH = (duty >> 8) & 0xff;
    dL = duty & 0xff;

    if (ch & PWM_CH1)
    {
        TIM2_CCR1H = dH;
        TIM2_CCR1L = dL;
    }
    if (ch & PWM_CH2)
    {
        TIM2_CCR2H = dH;
        TIM2_CCR2L = dL;
    }
    if (ch & PWM_CH3)
    {
        TIM2_CCR3H = dH;
        TIM2_CCR3L = dL;
    }
}
