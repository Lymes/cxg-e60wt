//
//  uart.h
//  cxg-60ewt
//
//  UART1 debug output via PD5 (TX only).
//  All symbols compile away to nothing in release builds.
//  Enable with: make debug
//

#ifndef _UART_H_
#define _UART_H_

#ifdef DEBUG
void uart_init(void);
void uart_printf(const char *fmt, ...);
#define DBG_PRINTF uart_printf
#else
#define uart_init()     /* no-op in release */
#define DBG_PRINTF(...) /* no-op in release */
#endif

#endif /* _UART_H_ */
