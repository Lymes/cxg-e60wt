//
//  adc.c
//  cxg-60ewt
//
//  Created by Leonid Mesentsev on 26/06/2019.
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

#include <adc.h>
#include <stm8s.h>

void ADC_init()
{
    /* Configure ADC channel 1 (PB0)  */
    ADC1_CSR |= 0;
    /* Right-align data */
    ADC1_CR2 |= (1 << ADC1_CR2_ALIGN);
    /* Wake ADC from power down */
    ADC1_CR1 |= 1 << ADC1_CR1_ADON;
}

uint16_t ADC_read()
{
    uint8_t adcH, adcL;
    ADC1_CR1 |= (1 << ADC1_CR1_ADON);
    while (!(ADC1_CSR & (1 << ADC1_CSR_EOC)))
        ;
    adcL = ADC1_DRL;
    adcH = ADC1_DRH;
    ADC1_CSR &= ~(1 << ADC1_CSR_EOC); // clear EOC flag
    return (adcL | (adcH << 8));
}
