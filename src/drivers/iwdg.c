/**
 * @file    iwdg.c
 * @brief   Independent watchdog start-up detection and refresh.
 * @details Registers are written directly, not through the HAL driver.
 *          Timeout: LSI 32 kHz / 64 = 2 ms per count, (1999 + 1) * 2 ms = 4 s
 *          nominal. All state lives in the peripheral, shared with BOOT.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DBT-002, DRT-012, HLT-003, HND-004
 * @ingroup iwdg
 * @see iwdg.h
 * @see health_task.c
 */

#include <stdbool.h>
#include "stm32f7xx_hal.h"
#include "drivers/iwdg.h"

/** `IWDG->KR` key that unlocks @c PR, @c RLR and @c WIN until @c RLR is written. */
#define IWDG_KEY_ACCESS 0x00005555UL

/**
 * @brief Reload value for 4.0 s nominal at /64 (`TRY-004`, `HND-004`).
 * @warning Part of the hand-over contract; `HLT-003` and `DRT-012` rely on it.
 */
#define IWDG_RELOAD_4S 1999U

/** Poll bound for LSI-ready and update flags (about 40 ms at 216 MHz, as BOOT). */
#define IWDG_WAIT_LOOPS 1000000UL

/**
 * @brief Enable the LSI and poll LSIRDY up to @ref IWDG_WAIT_LOOPS times.
 * @details The IWDG update flags never clear without the LSI. Returns on
 *          timeout too, leaving a possibly inactive watchdog.
 * @warning Busy-waits; start-up only.
 */
static void iwdg_start_lsi(void)
{
    RCC->CSR |= RCC_CSR_LSION;
    for(uint32_t i = 0U; i < IWDG_WAIT_LOOPS; i++)
    {
        if((RCC->CSR & RCC_CSR_LSIRDY) != 0U)
        {
            return;
        }
    }
}

/**
 * @brief Poll until PVU, RVU and WVU clear, up to @ref IWDG_WAIT_LOOPS times.
 * @details Registers ignore writes while their update flag is set. Returns on
 *          timeout too.
 * @warning Busy-waits; start-up only.
 */
static void iwdg_wait_ready(void)
{
    for(uint32_t i = 0U; i < IWDG_WAIT_LOOPS; i++)
    {
        if((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU)) == 0U)
        {
            return;
        }
    }
}

/** Reload the BOOT-started IWDG without touching its configuration. */
void app_iwdg_early_kick(void) { IWDG->KR = IWDG_KEY_RELOAD; }

/** Start the IWDG at 4 s if unconfigured, then reload it. */
void app_iwdg_init(void)
{
    /* Anything but the reset values means BOOT already configured it. */
    bool running = (IWDG->PR != 0U) || (IWDG->RLR != 0x0FFFU);

    if(!running)
    {
        iwdg_start_lsi(); /* update flags need the LSI */
        IWDG->KR  = IWDG_KEY_ENABLE;
        IWDG->KR  = IWDG_KEY_ACCESS;
        IWDG->PR  = IWDG_PRESCALER_64;
        IWDG->RLR = IWDG_RELOAD_4S;
        iwdg_wait_ready();
        IWDG->KR = IWDG_KEY_RELOAD;
    }

    app_iwdg_kick();
}

/** Reload the IWDG; Health task only. */
void app_iwdg_kick(void) { IWDG->KR = IWDG_KEY_RELOAD; }
