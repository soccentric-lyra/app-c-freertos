/**
 * @file    stm32f7xx_hal_conf.h
 * @brief   STM32F7 HAL configuration: clocks, tick priority, module set, assert hook.
 * @details Included by every HAL unit via @c stm32f7xx_hal.h. Drivers are used
 *          as shipped from STM32CubeF7 v1.17.4 (DCON-002); only this file is
 *          project code. Keep the module list in step with @c HAL_SOURCES in
 *          CMakeLists.txt, and the layout identical to the HWVAL copy
 *          (`DGEN-007`, OPEN-D02).
 * @note    CMakeLists.txt also defines `HSE_VALUE=8000000U`; both definitions
 *          must stay token-identical or the compiler reports a redefinition.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6, NUCLEO-F767ZI
 * @srs     DCON-002 (drivers used as shipped; this is the project-owned
 *          configuration file)
 * @ingroup hal_conf
 */

#ifndef STM32F7xx_HAL_CONF_H
#define STM32F7xx_HAL_CONF_H

/**
 * @defgroup hal_conf HAL configuration
 * @brief Clock sources, VDD, tick priority, HAL module set and assert hook.
 * @note  Enable a module only when the SRS needs the peripheral, and add its
 *        source to @c HAL_SOURCES in the same change; the image is
 *        footprint-checked (128 KB flash / 64 KB RAM).
 * @see stm32f7xx_hal.h, clock.c, hal_timebase.c, uart3.c, iwdg.c
 * @{
 */
/**
 * @name Oscillator and external clock values
 * @brief Nominal frequencies in Hz for the RCC driver; only HSE is used at run
 *        time (`DGEN-003`).
 * @{
 */
/**
 * @brief HSE frequency in Hz: 8 MHz ST-LINK MCO on PH0, bypass mode.
 * @details @ref app_clock_init derives 216 MHz from it (PLLM 8, PLLN 432,
 *          PLLP 2, PLLQ 9) (`DGEN-003`, `DCON-005`, Hardware.md section 6).
 * @warning Never reconfigure PH0/PH1 while the HSE is in use (`DCON-005`).
 */
#define HSE_VALUE 8000000U
/** HSE ready timeout in ms; expiry is a fatal `APP_FAULT_STARTUP`. */
#define HSE_STARTUP_TIMEOUT 100U
/** HSI frequency in Hz; reset clock, SYSCLK until @ref app_clock_init switches. */
#define HSI_VALUE 16000000U
/**
 * @brief Nominal LSI frequency in Hz; IWDG clock (`DRT-012`).
 * @note  The real LSI is inaccurate; the 4 s watchdog timeout is nominal.
 */
#define LSI_VALUE 32000U
/** LSE frequency in Hz; unused (no LSE crystal fitted), Cube template value. */
#define LSE_VALUE 32768U
/** LSE ready timeout in ms; unused. */
#define LSE_STARTUP_TIMEOUT 5000U
/** External SAI1 clock in Hz; unused, Cube template value. */
#define EXTERNAL_SAI1_CLOCK_VALUE 48000U
/** External SAI2 clock in Hz; unused, Cube template value. */
#define EXTERNAL_SAI2_CLOCK_VALUE 48000U
/** External I2S/SAI clock input in Hz; unused, Cube template value. */
#define EXTERNAL_CLOCK_VALUE 12288000U

/** @} */
/**
 * @name Supply, tick and runtime model
 * @{
 */
/**
 * @brief Supply voltage in mV (3.3 V).
 * @note  Voltage scale 1 with over-drive at 216 MHz assumes this rail
 *        (`DGEN-003`); keep in step with Hardware.md.
 */
#define VDD_VALUE 3300U
/**
 * @brief TIM6 HAL time base priority: 15, the least urgent.
 * @details Applied by the @c HAL_InitTick override in @ref hal_timebase.c.
 *          Above `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5), so it is masked in
 *          FreeRTOS critical sections (`DRT-003`); the handler calls no RTOS API.
 * @note  Masked ticks can be lost, so @c HAL_GetTick() uptime may lag real
 *        time. It stays monotonic.
 */
#define TICK_INT_PRIORITY 15U
/** HAL built without RTOS awareness; FreeRTOS owns SysTick (`DRT-001`). */
#define USE_RTOS 0U
/**
 * @brief Prefetch required at 216 MHz (`DGEN-003`).
 * @note  Informational: @ref app_clock_init enables prefetch and ART itself;
 *        caches are enabled by `app_cache_mpu_init()`.
 */
#define PREFETCH_ENABLE 1U

/** @} */
/**
 * @name Enabled HAL modules
 * @brief Minimum set for the clock tree, LEDs, button, HWLINK UART and IWDG.
 * @note  Each module needs a `HAL_SOURCES` entry and costs flash checked by
 *        `check_footprint.py`.
 * @{
 */
/** Legacy top-level switch; kept to stay diffable with HWVAL (`DGEN-007`). */
#define HAL_MODULE_ENABLED
/** CMSIS core and NVIC helpers (`DRT-003`). */
#define HAL_CORTEX_MODULE_ENABLED
/** Reset and clock control: 216 MHz tree (`DGEN-003`, `DBT-003`), PCLK1 query. */
#define HAL_RCC_MODULE_ENABLED
/**
 * @brief GPIO: LEDs PB0/PB7/PB14, B1 PC13, USART3 PD8/PD9 (`DCON-004`).
 * @note  The fault path uses raw register writes instead (`DGEN-004`).
 */
#define HAL_GPIO_MODULE_ENABLED
/** Power control: voltage scale 1 and over-drive for 216 MHz (`DGEN-003`). */
#define HAL_PWR_MODULE_ENABLED
/** Flash: latency definitions, CONFIRMED record write (`HLT-002`, [BOOT] LAY-005). */
#define HAL_FLASH_MODULE_ENABLED
/** DMA: USART3 RX ring and TX streams in the non-cacheable `.dma_buffer` (`COM-002`). */
#define HAL_DMA_MODULE_ENABLED
/** UART: USART3 HWLINK on the ST-LINK VCP (`COM-001`, `DCON-004`). */
#define HAL_UART_MODULE_ENABLED
/** IWDG definitions; @ref iwdg.c writes registers directly (`DRT-012`, `HLT-003`). */
#define HAL_IWDG_MODULE_ENABLED

/** @} */
/**
 * @brief Module headers for every HAL unit.
 * @note  Order follows the Cube/HWVAL template (`DGEN-007`); do not reorder.
 *        Each entry needs its `HAL_*_MODULE_ENABLED` switch and source file.
 */
#include "stm32f7xx_hal_rcc.h"
#include "stm32f7xx_hal_gpio.h"
#include "stm32f7xx_hal_cortex.h"
#include "stm32f7xx_hal_pwr.h"
#include "stm32f7xx_hal_pwr_ex.h"
#include "stm32f7xx_hal_flash.h"
#include "stm32f7xx_hal_flash_ex.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_uart.h"
#include "stm32f7xx_hal_iwdg.h"

#ifdef USE_FULL_ASSERT
/**
 * @brief Report a failed HAL argument check.
 * @param[in] file Source file (`__FILE__`).
 * @param[in] line Source line (`__LINE__`).
 * @warning Declared only: no definition exists, so defining `USE_FULL_ASSERT`
 *          fails at link time. An implementation must not return (`DGEN-004`).
 */
void assert_failed(uint8_t *file, uint32_t line);
#endif

/**
 * @brief HAL argument-check hook.
 * @param expr Check expression; not evaluated unless `USE_FULL_ASSERT` is set.
 * @note  Not defined by the build, so HAL drivers do no argument checking;
 *        @c configASSERT() is the active check.
 * @warning No side effects in @p expr.
 */
#ifdef USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define assert_param(expr) ((void)0U)
#endif

/** @} */
#endif /* STM32F7xx_HAL_CONF_H */
