//
//  eeprom.c
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

#include <eeprom.h>
#include <string.h>
#include <s7c.h>
#include <clock.h>

void eeprom_unlock()
{
    if (!(FLASH_IAPSR & (1 << FLASH_IAPSR_DUL)))
    {
        FLASH_DUKR = FLASH_DUKR_KEY1;
        FLASH_DUKR = FLASH_DUKR_KEY2;
        while (!(FLASH_IAPSR & (1 << FLASH_IAPSR_DUL)))
            ;
    }
}

void option_bytes_unlock()
{
    FLASH_CR2 |= (1 << FLASH_CR2_OPT);
    FLASH_NCR2 &= ~(1 << FLASH_NCR2_NOPT);
}

void eeprom_lock()
{
    FLASH_IAPSR &= ~(1 << FLASH_IAPSR_DUL);
}

void eeprom_wait_busy()
{
    while (!(FLASH_IAPSR & (1 << FLASH_IAPSR_EOP)))
        ;
}

void eeprom_read(uint16_t addr, void *buf, int len)
{
    /* read EEPROM data into buffer */
    for (int i = 0; i < len; i++, addr++)
        ((uint8_t *)buf)[i] = _MEM_(addr);
}

void eeprom_write(uint16_t addr, void *buf, int len)
{
    eeprom_unlock();
    uint8_t *eeAddress = (uint8_t *)addr;
    uint8_t *storage = (uint8_t *)buf;
    for (uint8_t i = 0; i < len; i++)
    {
        *eeAddress++ = storage[i];
        S7C_refreshDisplay(currentMillis());
    }
    //eeprom_lock();
}