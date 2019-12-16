//
//  eeprom.h
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

#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <stm8s.h>

#define EEPROM_START_ADDR 0x4000
#define EEPROM_END_ADDR 0x407F

/* Option bytes */
#define OPT0 _MEM_(0x4800)
#define OPT1 _MEM_(0x4801)
#define NOPT1 _MEM_(0x4802)
#define OPT2 _MEM_(0x4803)
#define NOPT2 _MEM_(0x4804)
#define OPT3 _MEM_(0x4805)
#define NOPT3 _MEM_(0x4806)
#define OPT4 _MEM_(0x4807)
#define NOPT4 _MEM_(0x4808)
#define OPT5 _MEM_(0x4809)
#define NOPT5 _MEM_(0x480A)

void eeprom_read(uint16_t addr, void *buf, int len);

void eeprom_write(uint16_t addr, void *buf, int len);

/**
 * Enable write access to EEPROM.
 */
void eeprom_unlock();

/**
 * Enable write access to option bytes.
 * EEPROM must be unlocked first.
 */
void option_bytes_unlock();

/**
 * Disable write access to EEPROM.
 */
void eeprom_lock();

/**
 * Wait until programming is finished.
 * Not necessary on devices with no RWW support.
 */
void eeprom_wait_busy();

#endif /* _EEPROM_H_ */
