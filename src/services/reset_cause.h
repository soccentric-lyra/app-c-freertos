/**
 * @file    reset_cause.h
 * @brief   RCC reset flags captured at start-up and decoded into a cause.
 * @details main() captures once, before HAL_Init(), then the hardware flags are
 *          cleared (DGEN-005). The raw mask goes into HWLINK Status; the text
 *          only into the start-up log line.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-005, LED-005
 * @ingroup reset_cause
 */

#ifndef RESET_CAUSE_H
#define RESET_CAUSE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup reset_cause Reset cause snapshot
 * @brief One-shot RCC reset-cause capture, its decoded text and a watchdog test.
 * @{
 */

/**
 * @brief RCC_CSR reset-cause bits captured at start-up (DGEN-005).
 * @details IWDG, WWDG, LPWR, SFT, POR, BOR and PIN flags only; sent unchanged
 *          as Status.reset_cause. Zero until app_reset_cause_capture() runs.
 * @note A power-on sets POR, BOR and PIN together.
 * @warning Read-only after the capture.
 */
extern uint32_t app_reset_cause_flags;

/**
 * @brief Decoded reset cause as a string literal; never NULL.
 * @details One of "independent watchdog", "window watchdog", "low power",
 *          "software", "power-on", "brown-out", "reset pin" or "unknown".
 */
extern const char *app_reset_cause_text;

/**
 * @brief Snapshot RCC_CSR, decode the cause and clear the hardware flags.
 * @details Priority: IWDG, WWDG, LPWR, SFT, POR, BOR, PIN. POR precedes BOR and
 *          PIN, which a power-on also sets (RM0410 6.3.17).
 * @pre Called once from main(), before HAL_Init(), the scheduler and the IWDG.
 * @warning Not idempotent: a second call reads cleared flags and stores 0 and
 *          "unknown".
 * @see app_reset_was_error
 */
void app_reset_cause_capture(void);

/**
 * @brief Report whether the last reset came from the IWDG or WWDG (LED-005).
 * @details main() latches @ref APP_ERROR_WATCHDOG on true. Reads only the
 *          captured mask, so it is safe from any context.
 * @return true if IWDGRSTF or WWDGRSTF was captured; false otherwise or
 *         before the capture.
 */
bool app_reset_was_error(void);

/** @} */

#endif /* RESET_CAUSE_H */
