/*
 * uart.h
 *
 *  Created on: 6 Apr 2025
 *      Author: Sicris
 */

#ifndef BSP_UART_H_
#define BSP_UART_H_

void UART_init(void (*tx_done_cb)(void), void (*rx_cb)(void));
size_t UART_send(uint8_t * const pBuf, const size_t buflen);
size_t UART_receive(uint8_t * const pBuf, const size_t bufLen);

#endif /* BSP_UART_H_ */
