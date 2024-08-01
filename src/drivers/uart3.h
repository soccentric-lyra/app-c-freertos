/**
 * @file    uart3.h
 * @brief   USART3 on the ST-LINK VCP: DMA ring reception and DMA transmit.
 * @details HWLINK transport (DCON-004): PD8 TX / PD9 RX on AF7, 8N1, no flow
 *          control, 16x oversampling (COM-001). Transport only; framing lives
 *          in @ref comms_task.h.
 *
 *          RX: DMA1 Stream1 ch4 fills a 4 KB circular ring in non-cacheable
 *          @c .dma_buffer; IDLE, half/full and error interrupts only post a
 *          wake-up hint. TX: DMA1 Stream3 ch4, normal mode, completion by task
 *          notification (COM-002). Line errors are cleared via @c USART3->ICR
 *          and a stopped receiver is re-armed with @ref app_uart3_rx_restart,
 *          never by a board reset (HLT-001).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (PD8 TX, PD9 RX)
 * @srs     COM-001, COM-002, DCON-004
 * @ingroup uart3
 */

#ifndef UART3_H
#define UART3_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup uart3 USART3 ST-LINK VCP transport
 * @brief DMA ring receiver and DMA transmitter for the HWLINK serial link.
 * @note  @ref app_uart3_init runs once before the scheduler; the rest is for
 *        the Comms task only. Nothing except @ref comms_post_rx_from_isr is
 *        ISR-safe or safe to call from two tasks.
 * @see comms_task.h
 * @{
 */

/** Default link rate in baud (COM-001). */
#define APP_UART_DEFAULT_BAUD 115200U

/** Fast link rate in baud, selected by @c SetBaud; revert lives in Comms (COM-001). */
#define APP_UART_FAST_BAUD 921600U

/**
 * @brief Initialise USART3 at @ref APP_UART_DEFAULT_BAUD and arm the RX ring.
 * @details The MSP hook configures clocks, PD8/PD9, both DMA streams and the
 *          three NVIC lines (priority 6). Ring counters are reset.
 * @return true when initialised and RX is armed; false otherwise (no retry).
 * @pre  Once from @c main, before the scheduler, after clocks, cache/MPU and
 *       HAL tick set-up.
 * @warning Not reentrant; calling again while running breaks live reception.
 * @see app_uart3_rx_running
 */
bool app_uart3_init(void);

/**
 * @brief Re-initialise USART3 at @p baud and re-arm RX.
 * @details Discards unread ring bytes, so send the @c SetBaud @c Ack at the
 *          old rate first and reset the parser after (COM-001). On failure the
 *          old rate is restored (best effort).
 * @param[in] baud @ref APP_UART_DEFAULT_BAUD or @ref APP_UART_FAST_BAUD.
 * @return true when the link runs at @p baud (including when already active).
 * @retval false Unsupported rate or re-init failed; old rate kept.
 * @pre  Comms task, no transmit in flight.
 * @note If the rollback init also fails, USART3 stays de-initialised while
 *       @ref app_uart3_baud reports the old rate.
 * @see app_uart3_baud
 */
bool app_uart3_set_baud(uint32_t baud);

/**
 * @brief Return the configured (cached) rate in baud.
 * @return Rate in baud; @ref APP_UART_DEFAULT_BAUD until a switch succeeds.
 */
uint32_t app_uart3_baud(void);

/**
 * @brief Copy unread bytes from the RX ring, oldest first.
 * @details If the DMA lapped the reader, the backlog is dropped and 0 is
 *          returned. Non-blocking; reception continues during the copy.
 * @param[out] dst     Destination buffer.
 * @param[in]  max_len Capacity of @p dst in bytes.
 * @return Bytes copied; 0 when empty, @p max_len is 0, or after a lap.
 * @pre  Single consumer (Comms task).
 * @see comms_task.h
 */
uint32_t app_uart3_rx_read(uint8_t *dst, uint32_t max_len);

/**
 * @brief Report whether the RX DMA ring is armed and receiving.
 * @details True only when the HAL RX state, the RX DMA state and
 *          @c USART3->CR1 RE all agree. Used by the health check (HLT-001).
 * @return true when receiving; a snapshot, not atomic with the ISRs.
 * @see app_uart3_rx_restart
 */
bool app_uart3_rx_running(void);

/**
 * @brief Re-arm the RX ring after an error or abort; no-op while running.
 * @details Clears ORE/FE/NE/PE/IDLE via @c ICR, aborts the HAL receive and
 *          restarts with an empty ring (COM-002, HLT-001).
 * @return true when RX is armed afterwards.
 * @pre  Comms task (shares the ring indices with @ref app_uart3_rx_read).
 * @note Flags are cleared here because @c HAL_UART_IRQHandler aborts the
 *       circular receiver on overrun.
 */
bool app_uart3_rx_restart(void);

/**
 * @brief Send one frame by DMA and block until it has left the shift register.
 * @details Waits for the TX-complete notification, then up to 10 ms for @c TC
 *          so a following reset or rate switch does not cut the frame
 *          (DBT-005). A stale notification is cleared first. @p data is not
 *          copied.
 * @param[in] data       Frame bytes; may be NULL only when @p len is 0.
 * @param[in] len        Length in bytes.
 * @param[in] timeout_ms Completion timeout in ms; below one tick does not block.
 * @return true when sent or nothing to send (a missed @c TC is not an error).
 * @retval false DMA start failed or timed out; TX aborted.
 * @pre  Task context, scheduler running.
 * @warning Single sender (Comms task); one stored task handle.
 * @see app_uart3_set_baud
 */
bool app_uart3_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Post an "RX may be ready" hint to the Comms task from an ISR.
 * @details Defined in @ref comms_task.c; declared here so @ref uart3.c need
 *          not include @ref comms_task.h. No-op before the queue exists. Hints
 *          coalesce; a dropped hint loses no data.
 * @warning ISR context only (FromISR API).
 */
void comms_post_rx_from_isr(void);

/** @} */

#endif /* UART3_H */
