/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS V10.2.0 kernel configuration (GCC Cortex-M7 r0p1 port).
 * @details Included by @c FreeRTOS.h before the kernel typedefs exist, so it
 *          holds plain macros and includes only @c <stdint.h>. All objects are
 *          created before vTaskStartScheduler() and nothing allocates later
 *          (DRT-004), which is why a dynamic-only heap_4 setup suffices.
 * @note    Compiled by the kernel: no declarations, code or project includes.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (STM32F767ZIT6)
 * @srs     DRT-001..DRT-004; FreeRTOS V10.2.0 as shipped in the image
 *          (same kernel and port as the validation firmware)
 * @ingroup freertos_config
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* configTICK_RATE_HZ casts to TickType_t; the cast is evaluated at use sites. */
#include <stdint.h>

/**
 * @defgroup freertos_config FreeRTOS kernel configuration
 * @brief Kernel settings for the Health, Comms, Button and LED task set
 *        (DRT-001..DRT-004, DRT-010..DRT-013).
 * @{
 */

/* --- scheduler ----------------------------------------------------------- */
/**
 * Preemptive scheduling (1 = enabled).
 * @details Priorities are Health +5, Comms +4, Button +3, LED +2 (DRT-010);
 *          IWDG supervision (DRT-012) relies on preemption, not yields.
 */
#define configUSE_PREEMPTION 1

/** CLZ-based ready-list search (1 = enabled); limits priorities to 32. */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

/**
 * Tickless idle (0 = disabled).
 * @details Keeps the 1 kHz tick running so tick timeouts and the 100 ms IWDG
 *          refresh are unaffected; the core stays at 216 MHz (DGEN-003).
 */
#define configUSE_TICKLESS_IDLE 0

/**
 * Core clock in Hz (216 MHz, DGEN-003).
 * @details Sets the SysTick reload; must match app_clock_init() in
 *          @ref clock.h or every tick timeout scales.
 */
#define configCPU_CLOCK_HZ 216000000UL /* DGEN-003 */

/**
 * Tick rate in Hz: 1 kHz from SysTick (DRT-001).
 * @details 1 tick = 1 ms, so pdMS_TO_TICKS(n) == n. 32-bit ticks wrap after
 *          about 49.7 days; supervision uses unsigned differences, which survive
 *          the wrap (DRT-012). The HAL time base uses TIM6 (DGEN-006).
 */
#define configTICK_RATE_HZ ((TickType_t)1000) /* DRT-001 */

/**
 * Priority levels: 0 (idle) to 7.
 * @details Used: 0 idle, 2 LED and timer service, 3 Button, 4 Comms, 5 Health
 *          (DRT-010). Higher priorities trip configASSERT.
 */
#define configMAX_PRIORITIES 8

/**
 * Idle task stack depth in words (256 words = 1 KiB).
 * @note All stack depths in this project are StackType_t (32-bit) word counts.
 */
#define configMINIMAL_STACK_SIZE ((uint16_t)256)

/**
 * Task name buffer in bytes, including the NUL.
 * @note Equals APP_FAULT_TASK_LEN in @ref app_fault.h so names copied into the
 *       fault record are never truncated (DGEN-004, DRT-013).
 */
#define configMAX_TASK_NAME_LEN 16

/**
 * TickType_t width (0 = 32 bits).
 * @warning Must stay 0: 16-bit ticks wrap every 65.5 s and break the 2000 ms
 *          check-in comparison (DRT-012).
 */
#define configUSE_16_BIT_TICKS 0

/** Idle task yields to other priority-0 tasks (1 = enabled); none exist today. */
#define configIDLE_SHOULD_YIELD 1

/** Round-robin between equal priorities (1 = enabled); LED and timer task share 2. */
#define configUSE_TIME_SLICING 1

/* --- memory (DRT-001, DRT-004) ------------------------------------------- */
/**
 * Static creation APIs (0 = not compiled).
 * @warning Every object comes from heap_4 (DRT-001); creating one after start-up
 *          allocates, which DRT-004 forbids.
 */
#define configSUPPORT_STATIC_ALLOCATION 0

/** Dynamic creation APIs (1 = compiled); required while static allocation is 0. */
#define configSUPPORT_DYNAMIC_ALLOCATION 1

/**
 * heap_4 arena size in bytes.
 * @details Set by CMake from APP_HEAP_KB (default 24 KB) via APP_RTOS_HEAP_SIZE.
 *          At least 4096 bytes must remain free at the 5 s health check
 *          (DRT-012).
 * @warning Not defined in this header; building without APP_RTOS_HEAP_SIZE
 *          fails in heap_4.c.
 * @see configAPPLICATION_ALLOCATED_HEAP
 */
#define configTOTAL_HEAP_SIZE APP_RTOS_HEAP_SIZE

/** Heap array owner (0 = heap_4.c defines ucHeap in @c .bss). */
#define configAPPLICATION_ALLOCATED_HEAP 0

/* --- kernel objects ------------------------------------------------------ */
/** Mutexes with priority inheritance (1 = enabled); none created today. */
#define configUSE_MUTEXES 1

/** Recursive mutexes (0 = not compiled). */
#define configUSE_RECURSIVE_MUTEXES 0

/** Counting semaphores (0 = not compiled); queues and notifications only (DRT-011). */
#define configUSE_COUNTING_SEMAPHORES 0

/** Queue sets (0 = not compiled); no task waits on more than one queue. */
#define configUSE_QUEUE_SETS 0

/** Queue registry entries (0 = no debugger queue names). */
#define configQUEUE_REGISTRY_SIZE 0

/**
 * Task notifications (1 = enabled, DRT-011).
 * @details Carry Health check-in bits (DRT-012) and wake the Comms task on
 *          USART3 TX completion.
 */
#define configUSE_TASK_NOTIFICATIONS 1 /* DRT-011 */

/* --- timers -------------------------------------------------------------- */
/**
 * Software timers and the timer service task (1 = enabled).
 * @details No application timer exists; the "Tmr Svc" task is one of the six
 *          counted by DRT-010 and its stack and queue come from the heap.
 */
#define configUSE_TIMERS 1

/** Timer service task priority; equal to the LED task (time-sliced). */
#define configTIMER_TASK_PRIORITY 2

/** Timer command queue length in messages. */
#define configTIMER_QUEUE_LENGTH 8

/** Timer service task stack depth in words (1 KiB). */
#define configTIMER_TASK_STACK_DEPTH 256

/* --- hooks (DRT-002) ----------------------------------------------------- */
/**
 * Call vApplicationMallocFailedHook() on allocation failure (DRT-002).
 * @details The hook in @c rtos_hooks.c records APP_FAULT_MALLOC_FAILED and
 *          resets, so the next boot reports it (DGEN-004).
 */
#define configUSE_MALLOC_FAILED_HOOK 1

/**
 * Stack overflow check method 2 (DRT-002).
 * @details Checks the stack pointer and the last 20 bytes of fill pattern on
 *          each context switch; vApplicationStackOverflowHook() records
 *          APP_FAULT_STACK_OVERFLOW and resets (DGEN-004).
 */
#define configCHECK_FOR_STACK_OVERFLOW 2

/** Idle hook (0 = none); the IWDG is refreshed by the Health task (DRT-012). */
#define configUSE_IDLE_HOOK 0

/** Tick hook (0 = none). */
#define configUSE_TICK_HOOK 0

/* --- stack statistics (DRT-013) ------------------------------------------ */
/** Trace facility (1 = enabled); Comms uses uxTaskGetSystemState() for Status. */
#define configUSE_TRACE_FACILITY 1

/** Store each stack's end address in the TCB (1 = enabled); debugger aid only. */
#define configRECORD_STACK_HIGH_ADDRESS 1

/**
 * Stack depth and high-water-mark type.
 * @warning Depths above 65535 words are truncated silently.
 */
#define configSTACK_DEPTH_TYPE uint16_t

/* --- interrupt priorities (DRT-003) -------------------------------------- */
/* Every ISR that calls a FromISR API uses priority number >= 5. TIM6 = 15. */
/**
 * NVIC priority bits implemented by the STM32F767.
 * @note Must match @c __NVIC_PRIO_BITS in stm32f7xx.h.
 */
#define APP_NVIC_PRIO_BITS 4U

/** Priority bits known to the port (vPortValidateInterruptPriority()). */
#define configPRIO_BITS APP_NVIC_PRIO_BITS

/** Lowest urgency, unshifted (15); also the HAL TIM6 TICK_INT_PRIORITY. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15

/**
 * Most urgent priority number allowed to call FromISR APIs (DRT-003).
 * @details USART3, DMA1 streams 1/3 and EXTI15_10 use 6; the USART6 log uses 15.
 * @warning ISRs at 0..4 must not call FreeRTOS APIs; critical sections cannot
 *          mask them.
 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/** PendSV/SysTick priority written to SHPR3 (0xF0). */
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/** BASEPRI value for critical sections (0x50); masks priority numbers >= 5. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* --- debug (DRT-002) ----------------------------------------------------- */
/**
 * @brief Record an assertion fault and reset; does not return.
 * @details Declared here, not via @ref app_fault.h, so kernel sources pull in
 *          no project headers. Implemented in @c rtos_hooks.c.
 * @param[in] file Source file, from @c __FILE__.
 * @param[in] line Source line, from @c __LINE__.
 * @see app_fault_fatal
 */
extern void app_fault_assert(const char *file, int line);

/**
 * Kernel assertion (DRT-002).
 * @param x Condition; evaluated once.
 * @details On failure calls app_fault_assert(), which resets via the no-init
 *          fault path so the next boot reports file and line (DGEN-004).
 * @note Not affected by NDEBUG; checks stay in release builds.
 * @warning Does not halt in a debugger: LD3 shows the fault pattern for about
 *          2 s, then the MCU resets.
 * @see app_fault_show_and_reset
 */
#define configASSERT(x)                                                                                                                                                                                                                                                                                    \
    do                                                                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                      \
        if((x) == 0)                                                                                                                                                                                                                                                                                       \
        {                                                                                                                                                                                                                                                                                                  \
            app_fault_assert(__FILE__, __LINE__);                                                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                                                                                  \
    } while(0)

/* --- port mapping (CM7 GCC port, DRT-001) -------------------------------- */
/**
 * Map the port SVC handler onto the CMSIS vector.
 * @warning The application must not define @c SVC_Handler.
 */
#define vPortSVCHandler SVC_Handler

/** Map the port PendSV (context switch) handler onto the CMSIS vector. */
#define xPortPendSVHandler PendSV_Handler

/** Map the port tick handler onto SysTick; the HAL tick uses TIM6 (DGEN-006). */
#define xPortSysTickHandler SysTick_Handler

/* --- optional API includes ----------------------------------------------- */
/* Unlisted INCLUDE_* default to 0; listed here to record the decision. */
/** vTaskPrioritySet() (0 = not compiled); priorities are fixed (DRT-010). */
#define INCLUDE_vTaskPrioritySet 0

/** uxTaskPriorityGet() (0 = not compiled); Status uses TaskStatus_t. */
#define INCLUDE_uxTaskPriorityGet 0

/** vTaskDelete() (0 = not compiled); tasks never exit. */
#define INCLUDE_vTaskDelete 0

/** vTaskSuspend()/vTaskResume() (1 = compiled); needed for portMAX_DELAY blocks. */
#define INCLUDE_vTaskSuspend 1

/** vTaskDelayUntil() (1 = compiled); currently unused. */
#define INCLUDE_vTaskDelayUntil 1

/** vTaskDelay() (1 = compiled); currently unused. */
#define INCLUDE_vTaskDelay 1

/** xTaskGetSchedulerState() (1 = compiled); used by the hooks in @c rtos_hooks.c. */
#define INCLUDE_xTaskGetSchedulerState 1

/** xTaskGetCurrentTaskHandle() (1 = compiled); used by @ref uart3.c. */
#define INCLUDE_xTaskGetCurrentTaskHandle 1

/** uxTaskGetStackHighWaterMark() (1 = compiled); kept for debugging. */
#define INCLUDE_uxTaskGetStackHighWaterMark 1

/** @} */

#endif /* FREERTOS_CONFIG_H */
