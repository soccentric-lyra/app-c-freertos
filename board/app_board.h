/**
 * @file    app_board.h
 * @brief   NUCLEO-F767ZI LED/button pin map and APB prescalers.
 * @details Constants only: no hardware access, safe from tasks, ISRs and
 *          pre-scheduler code, so the fault path in @ref app_fault.h shares
 *          the same pins (DGEN-004). LEDs and B1 are active high.
 * @note    PB0, PB7, PB14 and PC13 are the only plain-GPIO pins; USART3
 *          (PD8/PD9) and the debug UART (PG14) belong to their peripherals
 *          (DSAFE-003).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (STM32F767ZIT6)
 * @srs     DASM-002, DSAFE-003; pins from Hardware.md section 9
 * @ingroup app_board
 */

/**
 * @defgroup app_board Board pin map and APB prescalers
 * @brief LED and button pins plus clock-tree prescalers (Hardware.md section 9).
 * @note  Untyped macros with no storage; they must stay usable before the HAL
 *        and C run-time are set up (DGEN-004).
 * @{
 */

/** Include guard of @ref app_board.h. */
#ifndef APP_BOARD_H
#define APP_BOARD_H

#include "stm32f7xx_hal.h"

/**
 * @brief GPIO port of LD1 (green).
 * @note  LD1 is on PB0 with the default solder bridge, PA5 with the
 *        alternative (OPEN-D03); PA5 needs a new port macro and clock enable.
 */
#define APP_LED1_PORT GPIOB

/** LD1 pin mask (PB0); push-pull, high = on. Starts in heartbeat (DASM-002). */
#define APP_LED1_PIN GPIO_PIN_0

/** GPIO port of LD2 (blue). */
#define APP_LED2_PORT GPIOB

/** LD2 pin mask (PB7); push-pull, high = on. Paired with LD1 for chase (DASM-002). */
#define APP_LED2_PIN GPIO_PIN_7

/**
 * @brief GPIO port of LD3 (red status/error LED).
 * @warning The register-level fault path in @ref app_fault.c hard-codes GPIOB;
 *          moving LD3 needs a matching edit there.
 */
#define APP_LED3_PORT GPIOB

/**
 * @brief LD3 pin mask (PB14); push-pull, high = on (DASM-002).
 * @note  Reserved for error/health: 100 ms blink on health failure (HLT-003),
 *        solid on latched fault or watchdog reset (LED-005), three flashes
 *        before a fault reset (DGEN-004). Requested patterns apply only when
 *        @ref led_post_set passes @c force.
 */
#define APP_LED3_PIN GPIO_PIN_14

/** GPIO port of B1 (blue user button). */
#define APP_BTN1_PORT GPIOC

/**
 * @brief B1 pin mask (PC13); input, high when pressed (DASM-002).
 * @note  Routed to EXTI13 (both edges, BTN-001), serviced by
 *        @c EXTI15_10_IRQHandler() at NVIC priority 6, below
 *        @c configMAX_SYSCALL_INTERRUPT_PRIORITY (DRT-003). Debounce (30 ms)
 *        is in software.
 * @warning Must keep a pull-down; a pull-up reads as a permanent press.
 */
#define APP_BTN1_PIN GPIO_PIN_13

/**
 * @brief APB1 prescaler: HCLK / 4 -> PCLK1 54 MHz, timers 108 MHz (DGEN-003).
 * @note  54 MHz is the APB1 maximum (PWR, IWDG, USART3).
 */
#define APP_APB1_PRESCALER RCC_HCLK_DIV4

/**
 * @brief APB2 prescaler: HCLK / 2 -> PCLK2 108 MHz, timers 216 MHz (DGEN-003).
 * @note  108 MHz is the APB2 maximum. SYSCFG (EXTI13 routing) is on APB2.
 */
#define APP_APB2_PRESCALER RCC_HCLK_DIV2

#endif /* APP_BOARD_H */

/** @} */
