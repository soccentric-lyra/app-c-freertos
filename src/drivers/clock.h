/**
 * @file    clock.h
 * @brief   216 MHz system clock tree bring-up (DGEN-003).
 * @details Resulting tree:
 *
 *          | Domain | Derivation          | Frequency | Notes             |
 *          |--------|---------------------|-----------|-------------------|
 *          | HSE    | ST-LINK MCO, bypass | 8 MHz     | `DCON-005`        |
 *          | PLL in | HSE / PLLM (8)      | 1 MHz     | PLL reference     |
 *          | VCO    | 1 MHz * PLLN (432)  | 432 MHz   |                   |
 *          | SYSCLK | VCO / PLLP (2)      | 216 MHz   | PLLCLK            |
 *          | HCLK   | SYSCLK / 1          | 216 MHz   | AHB, core, flash  |
 *          | PCLK1  | HCLK / 4            | 54 MHz    | timers at 108 MHz |
 *          | PCLK2  | HCLK / 2            | 108 MHz   | timers at 216 MHz |
 *          | PLLQ   | VCO / 9             | 48 MHz    | USB/SDMMC/RNG     |
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (STM32F767ZIT6)
 * @srs     DGEN-003, DBT-003
 *
 * @ingroup clock
 */

#ifndef CLOCK_H
#define CLOCK_H

/**
 * @defgroup clock System clock tree
 * @brief 216 MHz core clock derived from the 8 MHz ST-LINK HSE.
 * @{
 */

/**
 * @brief   Switch from the reset clock (HSI 16 MHz) to the 216 MHz PLL tree.
 * @details Sets voltage scale 1, HSE bypass + PLL, over-drive, dividers,
 *          7 flash wait states, then prefetch and ART ([DS11532]). Every field
 *          is written explicitly because BOOT owns the reset state (`HND-001`).
 * @pre   Once, from start-up after HAL_Init(), before any PCLK-clocked
 *        peripheral or timing-dependent set-up (UART, IWDG, SysTick).
 * @post  Caches and MPU still off; app_cache_mpu_init() follows.
 * @warning On HAL error, records @c APP_FAULT_STARTUP and resets; never
 *          returns with a partial tree.
 * @note  VOSRDY is not polled; it does not assert before the PLL is up
 *        (`CL-009`). Duplicates HWVAL `GEN-004` until `DGEN-007` (OPEN-D02).
 * @see    app_cache_mpu_init
 */
void app_clock_init(void);

/** @} */

#endif /* CLOCK_H */
