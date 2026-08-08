//
//  uart.c
//  cxg-60ewt
//
//  Minimal UART1 TX driver + printf-compatible formatter.
//  Compiled only when DEBUG is defined (make debug).
//  Supports: %d (int16_t), %u (uint16_t), %s, %c, %%
//
//  Hardware: PD5 = UART1_TX (5V level, 115200 8N1 @ 16 MHz)
//  Connect: PD5 → RX of USB-UART dongle, GND → GND, VDD+ → VREF(5V)
//

/* Suppress ISO C "empty translation unit" warning in release builds */
typedef int _uart_dummy;

#ifdef DEBUG

#include <stdarg.h>
#include <stm8s.h>

// ---------------------------------------------------------------------------
// Low-level TX
// ---------------------------------------------------------------------------

static void _uart_putchar(char c)
{
    while (!(UART1_SR & (1 << UART1_SR_TXE)))
        ;
    UART1_DR = (uint8_t)c;
}

static void _uart_puts(const char *s)
{
    while (*s)
        _uart_putchar(*s++);
}

static void _uart_print_i16(int16_t v)
{
    char buf[7]; /* "-32768\0" */
    uint8_t i = 6;
    uint8_t neg = 0;
    buf[i] = '\0';
    if (v < 0) { neg = 1; v = -v; }
    if (v == 0) { buf[--i] = '0'; }
    else { while (v > 0) { buf[--i] = (char)('0' + v % 10); v /= 10; } }
    if (neg) buf[--i] = '-';
    _uart_puts(&buf[i]);
}

// ---------------------------------------------------------------------------
// uart_printf — minimal subset: %d, %u, %s, %c, %%
// On STM8/SDCC int is 16-bit, so va_arg(ap, int) == int16_t.
// ---------------------------------------------------------------------------

void uart_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    while (*fmt)
    {
        if (*fmt == '%')
        {
            ++fmt;
            switch (*fmt)
            {
            case 'd': _uart_print_i16((int16_t)va_arg(ap, int));          break;
            case 'u': _uart_print_i16((int16_t)(uint16_t)va_arg(ap, int)); break;
            case 's': _uart_puts(va_arg(ap, char *));                       break;
            case 'c': _uart_putchar((char)va_arg(ap, int));                 break;
            case '%': _uart_putchar('%');                                    break;
            default:  _uart_putchar('%'); _uart_putchar(*fmt);              break;
            }
        }
        else
        {
            _uart_putchar(*fmt);
        }
        ++fmt;
    }
    va_end(ap);
}

// ---------------------------------------------------------------------------
// uart_init
// 16 MHz / 115200 = 138.9 → DIV=139=0x008B  BRR2=0x0B BRR1=0x08
// 16 MHz /   9600 = 1666  → DIV=1666=0x0682 BRR2=0x02 BRR1=0x68  (more noise-tolerant)
// BRR2 MUST be written before BRR1 (BRR1 write latches the divider).
// ---------------------------------------------------------------------------

void uart_init(void)
{
    UART1_BRR2 = 0x02; /* 9600 baud @ 16MHz */
    UART1_BRR1 = 0x68;
    UART1_CR2  = (1 << UART1_CR2_TEN); /* TX only, no IRQ */
}

#endif /* DEBUG */
