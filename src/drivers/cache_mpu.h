/**
 * @file    cache_mpu.h
 * @brief   Cortex-M7 cache enable and the non-cacheable SRAM2 DMA window.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-003 (caches on), COM-002 (DMA buffers)
 * @ingroup cache_mpu
 */

#ifndef CACHE_MPU_H
#define CACHE_MPU_H /**< Include guard for @ref cache_mpu.h. */

/**
 * @defgroup cache_mpu Cache and MPU configuration
 * @brief Cortex-M7 cache enable and the non-cacheable SRAM2 DMA window.
 * @details The D-cache is not coherent with DMA. All DMA buffers live in
 *          @c .dma_buffer (SRAM2, 0x2007C000, 16 KB), which MPU region 6 makes
 *          non-cacheable, so no clean/invalidate is needed (COM-002).
 * @{
 */

/**
 * @brief Make SRAM2 non-cacheable via MPU region 6, then enable I/D caches.
 * @details Region 6: normal, non-cacheable, non-shareable, XN, full access.
 *          @c PRIVDEFENA keeps the default map elsewhere.
 * @pre  Privileged thread mode, once, after @ref app_clock_init and before any
 *       DMA is enabled; caches off on entry (as left by BOOT).
 * @warning Only @c .dma_buffer objects are covered; DMA on stack, @c .bss or
 *          heap buffers is cache-incoherent.
 * @warning Call once: re-running @c SCB_EnableDCache() discards dirty lines.
 * @see app_clock_init, app_uart3_init
 */
void app_cache_mpu_init(void);

/** @} */

#endif /* CACHE_MPU_H */
