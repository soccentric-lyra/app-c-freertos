/**
 * @file    app_error.c
 * @brief   Latched application error state implementation.
 * @details Every mutator saves PRIMASK, disables interrupts, does the
 *          read-modify-write and restores PRIMASK, so it is safe from tasks and
 *          ISRs and never re-enables interrupts the caller had masked.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (STM32F767ZIT6)
 * @srs     LED-005, DGEN-004, DGEN-005, HLT-003
 * @ingroup app_error
 */

#include "stm32f7xx_hal.h"
#include "services/app_error.h"

/**
 * @brief Latched APP_ERROR_* mask.
 * @note Lives in .bss, so it is zero after every reset; persistent fault
 *       details are in @ref g_app_fault.
 */
volatile uint32_t app_error_flags;

/**
 * @brief Atomically OR bits into the error latch.
 * @param[in] flags APP_ERROR_* bits to latch; 0 is a no-op.
 * @note Called by main() before the scheduler (LED-005) and by the health task.
 */
void app_error_set(uint32_t flags)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    app_error_flags |= flags;
    __set_PRIMASK(primask);
}

/**
 * @brief Atomically clear every error bit.
 * @warning Also drops @ref APP_ERROR_HEALTH; ClearFault uses
 *          app_error_clear_fault() instead (LED-005).
 * @note No production caller in the current tree.
 */
void app_error_clear(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    app_error_flags = 0U;
    __set_PRIMASK(primask);
}

/**
 * @brief Atomically clear FAULT and WATCHDOG, keeping HEALTH (LED-005).
 * @details Called by the comms task's ClearFault handler alongside
 *          app_fault_clear() and led_post_clear_error().
 * @warning The mask hard-codes @ref APP_ERROR_HEALTH; review it when adding bits.
 */
void app_error_clear_fault(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    app_error_flags &= APP_ERROR_HEALTH;
    __set_PRIMASK(primask);
}
