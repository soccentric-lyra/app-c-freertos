/**
 * @file    main.c
 * @brief   DEMO entry point: BOOT hand-over, driver bring-up, task start-up.
 * @details Every task and queue is created before the scheduler starts, so
 *          nothing allocates afterwards (`DRT-004`). The step order in
 *          @ref main is contractual.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (STM32F767ZIT6), started by BOOT from slot A or B
 * @srs     DBT-001..DBT-006, DGEN-001..DGEN-006, DRT-004, section 5
 * @ingroup main
 * @see     boot_contract.h
 * @see     handover.h
 * @see     health_task.h
 */

#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "services/app_error.h"
#include "services/app_fault.h"
#include "services/boot_contract.h"
#include "tasks/button_task.h"
#include "drivers/cache_mpu.h"
#include "drivers/clock.h"
#include "tasks/comms_task.h"
#include "services/app_log.h"
#include "app/handover.h"
#include "tasks/health_task.h"
#include "drivers/iwdg.h"
#include "tasks/led_task.h"
#include "services/reset_cause.h"
#include "drivers/uart3.h"

/**
 * @defgroup main Application entry point
 * @brief Ordered start-up of the DEMO image and its fatal failure path.
 * @{
 */

/** Fatal start-up path; see the definition. */
static void startup_fail(const char *where) __attribute__((noreturn));

/**
 * @brief   Log a start-up failure, save an @ref APP_FAULT_STARTUP record and reset.
 * @details Flushes the log for up to 50 ms with interrupts masked (`DLOG-004`),
 *          then @ref app_fault_fatal shows the LD3 pattern (about 2.2 s) and
 *          resets (`DGEN-004`, `LED-005`). Works without the scheduler.
 * @param[in] where Stage label, non-NULL; stored in
 *                  @ref app_fault_record_t.file (first 31 chars kept).
 * @pre   Reset thread, after @ref app_log_init.
 * @warning Never returns; BOOT decides what runs next (`DBT-006`).
 * @see   app_fault_fatal
 */
static void startup_fail(const char *where)
{
    LOG_E("start-up failed: %s", where);
    app_log_flush(50U);
    app_fault_fatal(APP_FAULT_STARTUP, "main", where, 0U);
}

/**
 * @brief   Bring the image up in the contractual order and start FreeRTOS.
 * @details Order: VTOR, early IWDG kick, boot counter, reset cause (before the
 *          HAL can disturb it), HAL/clock/caches, fault handler enables,
 *          hand-over read, log, LEDs/IWDG, fault latches, USART3, tasks
 *          (Health last), scheduler. Any failure ends in @ref startup_fail.
 * @return Never returns.
 * @pre    Started by BOOT; a stand-alone start reads no hand-over record and
 *         starts the IWDG here.
 * @warning Do not reorder: VTOR first, reset cause before the HAL, fault
 *          enables before the hand-over read, USART3 before tasks (`DBT-002`,
 *          `DGEN-005`, `DGEN-004`, `COM-002`).
 * @see    iwdg.h, reset_cause.h
 */
int main(void)
{
    /* DBT-002: linked for slot A, may run from B after BOOT's bank swap. */
    SCB->VTOR = APP_SLOT_BASE;
    app_iwdg_early_kick();
    app_fault_boot_start();

    /* DGEN-005: keep the flags for Status, then clear them. */
    app_reset_cause_capture();

    /* HAL_Init status ignored: the clock tree is rewritten next. */
    (void)HAL_Init();
    /* DGEN-003 / DBT-003: reset clock state -> 216 MHz, then the caches. */
    app_clock_init();
    app_cache_mpu_init();

    /* DGEN-004: take MemManage/BusFault/UsageFault to their own handlers so
     * the saved reason is specific; HardFault catches escalations. */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
    __DSB();
    __ISB();

    /* DBT-004: decode the BOOT hand-over record. */
    app_handover_read();

    /* DLOG-001: debug lines on USART6 from here on. */
    app_log_init();
    LOG_I("start v" APP_VERSION_STR " " APP_GIT_COMMIT ", slot %c %s, "
          "reset %s",
          g_handover.slot == APP_SLOT_A   ? 'A'
          : g_handover.slot == APP_SLOT_B ? 'B'
                                          : '?',
          g_handover.trial == 2U   ? "on trial"
          : g_handover.trial == 1U ? "confirmed"
                                   : "no hand-over",
          app_reset_cause_text);

    /* DSAFE-003: LEDs dark; HND-004: keep BOOT's IWDG timeout. */
    app_leds_init();
    app_iwdg_init();

    /* LED-005: latch LD3 for the last fault or watchdog reset. */
    if(app_fault_is_valid())
    {
        app_error_set(APP_ERROR_FAULT);
        LOG_W("last fault: reason %lu in %.16s at %.32s:%lu, pc 0x%08lx", (unsigned long)g_app_fault.reason, g_app_fault.task, g_app_fault.file, (unsigned long)g_app_fault.line, (unsigned long)g_app_fault.pc);
    }
    if(app_reset_was_error())
    {
        app_error_set(APP_ERROR_WATCHDOG);
    }

    /* COM-001/002: USART3 must be running before the health check. */
    if(!app_uart3_init())
    {
        startup_fail("uart3 init");
    }

    /* DRT-004: every task and queue exists before vTaskStartScheduler(). */
    if(!led_task_create())
    {
        startup_fail("led task");
    }
    if(!button_task_create())
    {
        startup_fail("button task");
    }
    if(!comms_task_create())
    {
        startup_fail("comms task");
    }
    if(!health_task_create())
    {
        startup_fail("health task");
    }

    vTaskStartScheduler();

    /* Only reached when the idle or timer task could not be created. */
    startup_fail("scheduler");
}

/** @} */
