/**
 * @file    clock.c
 * @brief   216 MHz system clock tree bring-up (DGEN-003).
 * @details Copy of the validation firmware's `src/clock.c`; a shared copy is
 *          pending (`DGEN-007`, OPEN-D02).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6 rev Z @ 3.3 V, 8 MHz HSE bypass, voltage scale 1
 * @srs     DGEN-003 (same configuration as HWVAL GEN-004), DBT-003
 * @ingroup clock
 * @see     clock.h, app_board.h
 */

#include "stm32f7xx_hal.h"
#include "app_board.h"
#include "services/app_fault.h"
#include "drivers/clock.h"

/**
 * @brief   Record @c APP_FAULT_STARTUP ("clock", "HSE/PLL/over-drive") and reset.
 * @note    All three failure points share one label.
 * @warning Never returns; works with the PLL unlocked.
 */
static void clock_fail(void) __attribute__((noreturn));
static void clock_fail(void) { app_fault_fatal(APP_FAULT_STARTUP, "clock", "HSE/PLL/over-drive", 0U); }

/**
 * @brief   Switch from the reset clock (HSI 16 MHz) to the 216 MHz PLL tree.
 * @details Over-drive is enabled before the SYSCLK switch because scale 1
 *          alone allows only 180 MHz. HAL_RCC_ClockConfig() also re-runs
 *          HAL_InitTick(), rescaling TIM6 for the new PCLK1 (DGEN-006).
 * @warning @c configCPU_CLOCK_HZ must match this tree (DRT-001). Changing
 *          PLLN, PLLP or the AHB divider needs a matching flash latency.
 */
void app_clock_init(void)
{
    /* PWR_CR1 writes are dropped without the PWR interface clock. */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Scale 1 before raising the frequency. VOSRDY is not polled: it does not
     * set before the PLL is up (CL-009). */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* BOOT owns the reset state, so every field is set explicitly (DBT-003).
     * HSIState is ignored: OscillatorType selects HSE only. */
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType     = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState           = RCC_HSE_BYPASS; /* ST-LINK MCO 8 MHz (DCON-005) */
    osc.HSIState           = RCC_HSI_OFF;
    osc.PLL.PLLState       = RCC_PLL_ON;
    osc.PLL.PLLSource      = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM           = 8;             /* VCO in: 1 MHz */
    osc.PLL.PLLN           = 432;           /* VCO: 432 MHz */
    osc.PLL.PLLP           = RCC_PLLP_DIV2; /* SYSCLK: 216 MHz */
    osc.PLL.PLLQ           = 9;             /* PLLQ: 48 MHz */

    /* Fails when the ST-LINK MCO is missing or the PLL does not lock. */
    if(HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        clock_fail();
    }

    /* Over-drive must be active before SYSCLK exceeds 180 MHz ([DS11532]). */
    if(HAL_PWREx_EnableOverDrive() != HAL_OK)
    {
        clock_fail();
    }

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType          = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource       = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider      = RCC_SYSCLK_DIV1;    /* HCLK 216 MHz */
    clk.APB1CLKDivider     = APP_APB1_PRESCALER; /* 54 MHz, timers 108 MHz */
    clk.APB2CLKDivider     = APP_APB2_PRESCALER; /* 108 MHz, timers 216 MHz */

    /* Latency is raised before the switch; HAL_InitTick() re-runs (DGEN-006). */
    if(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_7) != HAL_OK)
    {
        clock_fail();
    }

    /* DGEN-003: enable after the final latency is set. */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_ART_ENABLE();
}
