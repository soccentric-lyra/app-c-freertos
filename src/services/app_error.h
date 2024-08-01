/**
 * @file    app_error.h
 * @brief   Latched application error bits reported on LD3.
 * @details Each producer sets only its own bit; bits stay set until cleared.
 *          The LED task shows FAULT/WATCHDOG solid and HEALTH as a fast blink.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     LED-005, DGEN-004, DGEN-005, HLT-003
 * @ingroup app_error
 */

#ifndef APP_ERROR_H
#define APP_ERROR_H

#include <stdint.h>

/**
 * @defgroup app_error Application error latch
 * @brief Latched error bits that drive LD3 and report health state.
 * @{
 */

/**
 * @brief Saved fault or assert from a previous boot (LED-005).
 * @details Set by main() before the scheduler starts. LD3 is solid while set.
 */
#define APP_ERROR_FAULT (1UL << 0U)

/**
 * @brief Last reset was caused by the IWDG or WWDG (LED-005).
 * @details Set by main() before the scheduler starts.
 */
#define APP_ERROR_WATCHDOG (1UL << 1U)

/**
 * @brief Pending-slot health check failed (HLT-003).
 * @details Set by the health task. LD3 blinks fast while set. Survives
 *          ClearFault so a failed health check is not hidden (LED-005).
 */
#define APP_ERROR_HEALTH (1UL << 2U)

/**
 * @brief Latched mask of APP_ERROR_* bits.
 * @details The LED task reads it without locking; an aligned 32-bit load is
 *          atomic on Cortex-M7.
 * @warning Do not assign directly. Use app_error_set(), app_error_clear() or
 *          app_error_clear_fault() so updates stay atomic.
 */
extern volatile uint32_t app_error_flags;

/**
 * @brief Atomically OR bits into the error latch.
 * @details Task and ISR safe: masks interrupts and restores the caller's PRIMASK.
 *
 * @param[in] flags APP_ERROR_* bits to latch; set only bits the caller owns.
 *
 * @see app_error_clear_fault
 */
void app_error_set(uint32_t flags);

/**
 * @brief Atomically clear every error bit.
 * @details Task and ISR safe: masks interrupts and restores the caller's PRIMASK.
 * @note Also clears HEALTH; ClearFault must use app_error_clear_fault()
 *       (LED-005).
 * @see app_error_clear_fault
 */
void app_error_clear(void);

/**
 * @brief Atomically clear FAULT and WATCHDOG, keeping HEALTH (LED-005).
 * @details Called by the HWLINK ClearFault handler. Task and ISR safe.
 * @warning Keeps only @ref APP_ERROR_HEALTH; any future bit is cleared too
 *          unless it is added to the mask.
 * @see app_error_clear
 */
void app_error_clear_fault(void);

/** @} */

#endif /* APP_ERROR_H */
