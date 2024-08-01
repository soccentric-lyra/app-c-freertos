/**
 * @file    app_log.h
 * @brief   Non-blocking debug text output on USART6 (PG14, Arduino D1).
 * @details 115200 8N1, TX only, shared with BOOT so one terminal shows the
 *          whole boot chain. Line format: `[   5012] C-FRT I health: ...`,
 *          i.e. HAL_GetTick() in ms (7 columns), tag, level, message, CR LF.
 *          Levels are labels only; nothing is filtered.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-06
 * @target  STM32F767ZIT6
 * @srs     DLOG-001..DLOG-004
 * @ingroup app_log
 */

#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdint.h>

/**
 * @defgroup app_log Application debug log
 * @brief Non-blocking, interrupt-driven debug text on USART6.
 * @details Lines go into a 1 KB ring drained by the USART6 TXE interrupt
 *          (DLOG-002). A line that does not fit is dropped whole and counted
 *          (DLOG-003).
 * @{
 */

/**
 * @brief Configure PG14 as USART6_TX at 115200 baud and enable its IRQ.
 * @details Until this returns, app_log() and app_log_flush() do nothing.
 *
 * @pre  Thread context, after HAL_Init() and the clock tree setup.
 * @note Pin, baud rate, tag and IRQ priority must match BOOT (DLOG-001).
 *       Creates no FreeRTOS object, so it is valid before the scheduler.
 * @warning Call once. A second call stops the transmitter and clears TXEIE,
 *          leaving queued bytes until the next app_log().
 */
void app_log_init(void);

/**
 * @brief Format one debug line and queue it on USART6.
 * @details Never blocks, allocates or calls FreeRTOS. Own formatter (newlib
 *          snprintf would need malloc): `%%`, `%c`, `%s` with `.N`, `%d`,
 *          `%i`, `%u`, `%x`, optional `l` and zero-padded width. Formatting
 *          stops at the first unsupported conversion. Lines truncate at 126
 *          chars plus CR LF.
 *
 * @param[in] level Label character, 'E', 'W', 'I' or 'D' by convention.
 * @param[in] fmt   printf-style format; must not be NULL.
 * @param[in] ...   Arguments for @p fmt.
 *
 * @note Task, ISR and pre-scheduler safe. Masks interrupts for one line copy
 *       only. Uses about 128 bytes of the caller's stack.
 * @warning Lines from several contexts may interleave in any order but are
 *          never split.
 * @see app_log_flush, app_log_dropped
 */
void app_log(char level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief Busy-wait until queued bytes have left the wire.
 * @details Polls USART6 with interrupts masked, since the tick and TXE IRQ do
 *          not run on the fault path. The budget is a loop count of
 *          @p timeout_ms * 50000 (saturating), not a timer.
 *
 * @param[in] timeout_ms Approximate wait limit in ms; 0 only clears TXEIE.
 *
 * @post TXEIE is clear and PRIMASK is restored.
 * @note For start-up failure, fault and pre-reset paths.
 * @warning Blocks every interrupt, including the RTOS tick, for the wait.
 *          Bytes left after a timeout go out only on the next app_log().
 */
void app_log_flush(uint32_t timeout_ms);

/**
 * @brief Return the number of lines dropped because the ring was full.
 * @return Lines dropped since reset, wrapping at 2^32 (DLOG-003).
 * @note Single 32-bit read; safe from any context.
 */
uint32_t app_log_dropped(void);

/**
 * @brief Log at error level ('E').
 * @param[in] ... Format string and arguments for app_log().
 * @note Arguments are always evaluated; no level is compiled out.
 */
#define LOG_E(...) app_log('E', __VA_ARGS__)

/**
 * @brief Log at warning level ('W').
 * @param[in] ... Format string and arguments for app_log().
 */
#define LOG_W(...) app_log('W', __VA_ARGS__)

/**
 * @brief Log at info level ('I').
 * @param[in] ... Format string and arguments for app_log().
 */
#define LOG_I(...) app_log('I', __VA_ARGS__)

/**
 * @brief Log at debug level ('D').
 * @param[in] ... Format string and arguments for app_log().
 * @note Not compiled out in any build; keep it off hot paths.
 */
#define LOG_D(...) app_log('D', __VA_ARGS__)

/** @} */

#endif /* APP_LOG_H */
