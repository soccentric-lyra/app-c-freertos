/**
 * @file    handover.c
 * @brief   BOOT hand-over record decoding and the "stay in BOOT" request.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DBT-004, DBT-005, HND-003, ENT-002
 * @ingroup handover
 * @see     handover.h, boot_contract.h
 */

#include "stm32f7xx_hal.h"
#include "services/boot_contract.h"
#include "app/handover.h"

/** Decoded hand-over record; see @ref handover.h. */
app_handover_t g_handover;

/**
 * @brief Decode @c RTC_BKP2R into @ref g_handover.
 * @details Rejects a bad magic or a reserved slot (0, 3) as absent. Always
 *          stores the raw word for diagnostics.
 */
void app_handover_read(void)
{
    /* Reading needs no DBP; leaving DBP off protects BOOT's trial counter and
     * the RTC from stray writes. */
    uint32_t raw = RTC->BKP2R;

    g_handover.raw     = raw;
    g_handover.present = (raw & APP_HANDOVER_MAGIC_MASK) == APP_HANDOVER_MAGIC;
    if(!g_handover.present)
    {
        g_handover.slot        = APP_SLOT_UNKNOWN;
        g_handover.trial       = 0U;
        g_handover.trial_count = 0U;
        return;
    }

    g_handover.slot = (uint8_t)((raw >> APP_HANDOVER_SLOT_SHIFT) & APP_HANDOVER_SLOT_MASK);
    if(g_handover.slot != APP_SLOT_A && g_handover.slot != APP_SLOT_B)
    {
        g_handover.slot        = APP_SLOT_UNKNOWN;
        g_handover.present     = false;
        g_handover.trial       = 0U;
        g_handover.trial_count = 0U;
        return;
    }

    g_handover.trial       = ((raw >> APP_HANDOVER_PENDING_BIT) & 1U) != 0U ? 2U : 1U;
    g_handover.trial_count = (uint8_t)((raw >> APP_HANDOVER_TRIAL_SHIFT) & APP_HANDOVER_TRIAL_MASK);
}

/**
 * @brief Write the "stay in BOOT" magic to @c RTC_BKP1R (DBT-005).
 * @details The @c __DSB() completes the store before the caller's reset.
 */
void app_handover_request_bootloader(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_RTC_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP1R = APP_STAY_IN_BOOT_MAGIC;
    __DSB();
}
