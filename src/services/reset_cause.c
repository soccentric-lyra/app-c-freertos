/**
 * @file    reset_cause.c
 * @brief   RCC reset flags captured at start-up (implementation).
 * @details Touches only RCC->CSR and two globals, so it runs before HAL_Init().
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-005, LED-005
 * @ingroup reset_cause
 */

#include "stm32f7xx_hal.h"
#include "services/reset_cause.h"

/** @brief RCC_CSR reset-cause bits captured at start-up (DGEN-005). */
uint32_t app_reset_cause_flags;

/** @brief Decoded reset cause as a string literal; "unknown" until captured. */
const char *app_reset_cause_text = "unknown";

/**
 * @brief Snapshot RCC_CSR, decode the cause and clear the hardware flags.
 * @pre Called once from main() after app_fault_boot_start(), before HAL_Init().
 * @warning Not idempotent: a second call reads cleared flags.
 */
void app_reset_cause_capture(void)
{
    app_reset_cause_flags = RCC->CSR & (RCC_CSR_LPWRRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_SFTRSTF | RCC_CSR_PORRSTF | RCC_CSR_PINRSTF | RCC_CSR_BORRSTF);

    /* Most specific cause first.  A power-on sets POR, BOR and PIN, so
     * POR must be tested before BOR, and BOR before PIN, or a normal
     * power-on is reported as a brown-out (RM0410 6.3.17). */
    if((app_reset_cause_flags & RCC_CSR_IWDGRSTF) != 0U)
    {
        app_reset_cause_text = "independent watchdog";
    }
    else if((app_reset_cause_flags & RCC_CSR_WWDGRSTF) != 0U)
    {
        app_reset_cause_text = "window watchdog";
    }
    else if((app_reset_cause_flags & RCC_CSR_LPWRRSTF) != 0U)
    {
        app_reset_cause_text = "low power";
    }
    else if((app_reset_cause_flags & RCC_CSR_SFTRSTF) != 0U)
    {
        app_reset_cause_text = "software";
    }
    else if((app_reset_cause_flags & RCC_CSR_PORRSTF) != 0U)
    {
        app_reset_cause_text = "power-on";
    }
    else if((app_reset_cause_flags & RCC_CSR_BORRSTF) != 0U)
    {
        app_reset_cause_text = "brown-out";
    }
    else if((app_reset_cause_flags & RCC_CSR_PINRSTF) != 0U)
    {
        app_reset_cause_text = "reset pin";
    }
    else
    {
        app_reset_cause_text = "unknown";
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/**
 * @brief Report whether the last reset came from the IWDG or WWDG (LED-005).
 * @return true if IWDGRSTF or WWDGRSTF was captured.
 */
bool app_reset_was_error(void) { return (app_reset_cause_flags & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0U; }
