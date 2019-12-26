//
//  s7c.h
//  CXG60EWT
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

#ifndef _S7C_h_
#define _S7C_h_

#include <stdint.h>

// 4th digit symbols
#define SYM_CELS 1
#define SYM_MOON 8
#define SYM_SUN 16
#define SYM_SAVE 32
#define SYM_TEMP 64

#ifndef MAXNUMDIGITS
#define MAXNUMDIGITS 8 // Can be increased, but the max number is 2^31
#endif

// Use defines to link the hardware configurations to the correct numbers
#define COMMON_CATHODE 0
#define COMMON_ANODE 1
#define N_TRANSISTORS 2
#define P_TRANSISTORS 3
#define NP_COMMON_CATHODE 1
#define NP_COMMON_ANODE 0

void S7C_init();

void S7C_refreshDisplay(uint32_t ticks);
void S7C_begin(uint8_t hardwareConfig, uint8_t numDigitsIn, uint8_t digitPinsIn[],
               uint8_t segmentPinsIn[], uint8_t resOnSegmentsIn, uint8_t updateWithDelaysIn,
               uint8_t leadingZerosIn, uint8_t disableDecPoint);

void S7C_setNumber(int numToShow, uint8_t decPlaces, uint8_t hex);

void S7C_setSegments(uint8_t segs[]);
void S7C_setChars(char str[]);
void S7C_blank(void);
void S7C_setSymbol(uint8_t digitNum, uint8_t symbol);
void S7C_setDigit(uint8_t digitNum, uint8_t symbol);

void S7C_setNewNum(long numToShow, uint8_t decPlaces, uint8_t hex);
void S7C_findDigits(long numToShow, uint8_t decPlaces, uint8_t hex, uint8_t digits[]);
void S7C_setDigitCodes(uint8_t nums[], uint8_t decPlaces);
void S7C_segmentOn(uint8_t segmentNum);
void S7C_segmentOff(uint8_t segmentNum);
void S7C_digitOn(uint8_t digitNum);
void S7C_digitOff(uint8_t digitNum);

#endif // _S7C_h_
