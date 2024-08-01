/**
 * @file    rtos_hooks.c
 * @brief   FreeRTOS assert, stack-overflow and malloc-failed hooks.
 * @details Each hook calls app_fault_fatal(), which saves the no-init fault
 *          record (DGEN-004), flashes LD3 and resets; the next boot reports it.
 *          Hooks never return, allocate nothing and take no locks. Register
 *          fields of the record stay zero; @c line holds a reason-specific value.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DRT-002 (assert, stack overflow, malloc failed), DGEN-004
 * @ingroup app_fault
 */

#include "FreeRTOS.h"
#include "task.h"
#include "services/app_fault.h"

/**
 * @brief Return the running task name, or "startup" before the scheduler runs.
 * @return NUL-terminated name, never NULL.
 * @note  Safe from task, ISR and start-up context.
 * @warning Also returns "startup" while the scheduler is suspended.
 */
static const char *current_task_name(void)
{
    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        return pcTaskGetName(NULL);
    }
    return "startup";
}

/**
 * @brief Record @c APP_FAULT_ASSERT and reset (DRT-002).
 * @param[in] file Source file from @c __FILE__; basename kept, max 31 chars.
 * @param[in] line Source line from @c __LINE__; stored in @c line.
 * @note  Any context. The kernel-visible prototype lacks @c noreturn because
 *        kernel units do not include @ref app_fault.h.
 * @see configASSERT
 */
void app_fault_assert(const char *file, int line) { app_fault_fatal(APP_FAULT_ASSERT, current_task_name(), file, (uint32_t)line); }

/**
 * @brief Record @c APP_FAULT_STACK_OVERFLOW and reset (DRT-002).
 * @param[in] task Overflowing task handle; unused.
 * @param[in] name Overflowing task name from the TCB; may be NULL.
 * @note  Called from the context switch (PendSV, ISR context). The stack is
 *        already corrupt, so the hook does no logging or formatting.
 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    app_fault_fatal(APP_FAULT_STACK_OVERFLOW, name, "FreeRTOS", 0U);
}

/**
 * @brief Record @c APP_FAULT_MALLOC_FAILED with the free heap bytes in @c line.
 * @details Called by @c pvPortMalloc() on failure (DRT-002), from the
 *          allocating task or from start-up. The free size is a sizing hint;
 *          fragmentation can make it exceed the failed request.
 * @warning @c line is a byte count here but a source line for
 *          @c APP_FAULT_ASSERT; check @c reason first.
 */
void vApplicationMallocFailedHook(void)
{
    app_fault_fatal(APP_FAULT_MALLOC_FAILED, current_task_name(), "malloc", (uint32_t)xPortGetFreeHeapSize());
}
