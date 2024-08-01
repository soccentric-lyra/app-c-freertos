/**
 * @file    cache_mpu.c
 * @brief   Cortex-M7 cache enable and the non-cacheable SRAM2 DMA window.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-003 (caches on), COM-002 (DMA buffers)
 * @ingroup cache_mpu
 * @see     cache_mpu.h
 */

#include "stm32f7xx_hal.h"
#include "drivers/cache_mpu.h"

/** MPU region used for the DMA window; the only region programmed. */
#define DMA_MPU_REGION 6U
/**
 * @brief DMA window base: @c ORIGIN(SRAM2) in @c ld/app.ld, 16 KB aligned.
 * @warning Literal, not derived from the linker script; moving SRAM2 there
 *          without updating this silently makes DMA buffers cacheable.
 */
#define DMA_MPU_BASE 0x2007C000UL
/**
 * @brief @c MPU_RASR SIZE field for 16 KB: log2(bytes) - 1 = 13.
 * @warning Not log2(bytes) despite the name.
 */
#define DMA_MPU_SIZE_LOG2 13U /* 2^(13+1) = 16 KB */

/**
 * @brief Make SRAM2 non-cacheable via MPU region 6, then enable I/D caches.
 * @details The MPU is disabled while the region is programmed, and the region
 *          is active before the D-cache is enabled, so the window is never
 *          cached even transiently.
 */
void app_cache_mpu_init(void)
{
    MPU->CTRL = 0U;
    __DSB();
    __ISB();

    MPU->RNR  = DMA_MPU_REGION;
    MPU->RBAR = DMA_MPU_BASE | (DMA_MPU_REGION << MPU_RBAR_REGION_Pos) | MPU_RBAR_VALID_Msk;
    /* XN=1 (not executable), AP=full access, TEX=1, C=0, B=0, S=0. */
    MPU->RASR = MPU_RASR_XN_Msk | (3UL << MPU_RASR_AP_Pos) | (1UL << MPU_RASR_TEX_Pos) | (DMA_MPU_SIZE_LOG2 << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;

    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();

    SCB_EnableICache();
    SCB_EnableDCache();
}
