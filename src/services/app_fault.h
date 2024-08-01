/**
 * @file    app_fault.h
 * @brief   No-init fault record and fatal reset paths.
 * @details The record lives in the 1 KB .noinit window at the top of DTCM,
 *          which start-up code and BOOT never touch, so it survives resets
 *          (DGEN-004, LED-005). It is undefined at power-on; trust it only when
 *          app_fault_is_valid() is true. Fatal paths use registers only, so
 *          they work before the HAL and with interrupts disabled.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (STM32F767ZIT6)
 * @srs     DGEN-004, DRT-002, LED-005
 * @ingroup app_fault
 */

#ifndef APP_FAULT_H
#define APP_FAULT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup app_fault No-init fault record and fatal paths
 * @brief Retained fault record, boot identification and fatal reset helpers.
 * @{
 */

/**
 * @brief Validity marker for @ref app_fault_record_t.magic ("FAUL" MSB first).
 * @details Written last by app_fault_save() and cleared first, so it never
 *          marks a partial record.
 */
#define APP_FAULT_MAGIC 0x4641554CUL

/** @brief Size of the task name field, including NUL (15 chars kept). */
#define APP_FAULT_TASK_LEN 16U

/** @brief Size of the file basename field, including NUL (31 chars kept). */
#define APP_FAULT_FILE_LEN 32U

/**
 * @brief Reason recorded in the fault record.
 * @note Values index @c fault_reason_names in app_fault.c; keep both in step.
 *       app_fault_is_valid() accepts only NONE..STARTUP.
 */
typedef enum
{
    APP_FAULT_NONE = 0,       /**< No fault recorded. */
    APP_FAULT_HARDFAULT,      /**< HardFault, including escalated faults. */
    APP_FAULT_MEMMANAGE,      /**< MemManage, normally an MPU violation. */
    APP_FAULT_BUSFAULT,       /**< BusFault, normally a bad peripheral access. */
    APP_FAULT_USAGEFAULT,     /**< UsageFault, e.g. undefined instruction. */
    APP_FAULT_ASSERT,         /**< configASSERT failure (DRT-002). */
    APP_FAULT_STACK_OVERFLOW, /**< FreeRTOS stack overflow hook (DRT-002). */
    APP_FAULT_MALLOC_FAILED,  /**< FreeRTOS malloc hook; line holds free bytes. */
    APP_FAULT_STARTUP         /**< Start-up check failed; task holds the stage. */
} app_fault_reason_t;

/**
 * @brief Persistent fault record in .noinit.
 * @note Layout is read by the debugger and Status; do not reorder. Size plus
 *       the boot counter is asserted against APP_NOINIT_SIZE (A-9).
 * @warning Undefined at power-on; ignore until app_fault_is_valid().
 */
typedef struct
{
    uint32_t magic;                    /**< APP_FAULT_MAGIC when valid. */
    uint32_t reason;                   /**< One of @ref app_fault_reason_t. */
    uint32_t cfsr;                     /**< SCB->CFSR; 0 for software faults. */
    uint32_t hfsr;                     /**< SCB->HFSR; 0 for software faults. */
    uint32_t mmfar;                    /**< SCB->MMFAR; valid if MMARVALID. */
    uint32_t bfar;                     /**< SCB->BFAR; valid if BFARVALID. */
    uint32_t pc;                       /**< Stacked PC; 0 if unavailable. */
    uint32_t lr;                       /**< Stacked LR; 0 if unavailable. */
    uint32_t psr;                      /**< Stacked xPSR; 0 if unavailable. */
    uint32_t line;                     /**< Source line; free heap bytes on malloc fail. */
    uint32_t boot_id;                  /**< Boot counter at save time. */
    char     task[APP_FAULT_TASK_LEN]; /**< Task name, NUL-terminated, may be empty. */
    char     file[APP_FAULT_FILE_LEN]; /**< Source basename, NUL-terminated, may be empty. */
} app_fault_record_t;

/**
 * @brief The retained fault record (DGEN-004).
 * @note Must stay uninitialised and in .noinit.
 * @warning Read-only outside app_fault.c; single writer, no lock.
 */
extern app_fault_record_t g_app_fault;

/**
 * @brief Increment the retained boot counter; 0 is skipped.
 * @pre Called once from main() before anything can record a fault.
 * @note The first power-on value is arbitrary; only relative tests are made.
 */
void app_fault_boot_start(void);

/**
 * @brief Report whether the record has the magic and a known reason.
 * @details Ignores the boot id. No checksum, so random power-on RAM could in
 *          principle pass. Safe from any context.
 * @return true if the record is complete and plausible.
 */
bool app_fault_is_valid(void);

/**
 * @brief Report whether a valid record was written during this boot.
 * @return true if valid and stamped with the current boot id.
 * @pre app_fault_boot_start() has run.
 */
bool app_fault_is_current_boot(void);

/**
 * @brief Report whether a valid record was written during the previous boot.
 * @details Used by the health check to fail a trial whose boot faulted.
 * @return true if valid and stamped with the current boot id minus one.
 * @pre app_fault_boot_start() has run.
 */
bool app_fault_is_previous_boot(void);

/**
 * @brief Map a reason to its Status name, e.g. "hard_fault".
 * @param[in] reason Reason code.
 * @return Static string; "unknown" if out of range. Never NULL.
 * @note Names are wire vocabulary; change them only with the host.
 */
const char *app_fault_reason_text(app_fault_reason_t reason);

/**
 * @brief Return the task name stored in the record.
 * @return Pointer into @ref g_app_fault; may be empty, never NULL.
 */
const char *app_fault_task(void);

/**
 * @brief Return the source basename stored in the record (DGEN-004).
 * @return Pointer into @ref g_app_fault; may be empty, never NULL.
 */
const char *app_fault_file(void);

/**
 * @brief Write a fault record for the next boot; does not reset.
 * @details Clears the magic, writes the payload, then sets the magic behind
 *          barriers, so a reset mid-save leaves an invalid record.
 *
 * @param[in] reason Fault reason; values above APP_FAULT_STARTUP save an
 *                   invalid record.
 * @param[in] cfsr   SCB->CFSR, or 0.
 * @param[in] hfsr   SCB->HFSR, or 0.
 * @param[in] mmfar  SCB->MMFAR, or 0.
 * @param[in] bfar   SCB->BFAR, or 0.
 * @param[in] pc     Stacked PC, or 0.
 * @param[in] lr     Stacked LR, or 0.
 * @param[in] psr    Stacked xPSR, or 0.
 * @param[in] task   Task name or NULL; truncated to 15 chars.
 * @param[in] file   Path or label or NULL; basename kept, up to 31 chars.
 * @param[in] line   Source line, or free heap bytes for MALLOC_FAILED.
 *
 * @warning Not reentrant; assumes a single writer.
 * @see app_fault_fatal
 */
void app_fault_save(app_fault_reason_t reason, uint32_t cfsr, uint32_t hfsr, uint32_t mmfar, uint32_t bfar, uint32_t pc, uint32_t lr, uint32_t psr, const char *task, const char *file, uint32_t line);

/**
 * @brief Flash the fault pattern on LD3 for about 2.2 s, then reset.
 * @details Disables interrupts and drives PB14 by register, so it works from
 *          any context, before the HAL (DGEN-004). Reloads the IWDG once so
 *          the pattern fits the 4 s window.
 * @warning Never returns and saves nothing; call app_fault_save() first.
 */
void app_fault_show_and_reset(void) __attribute__((noreturn));

/**
 * @brief Save a software fault (no register snapshot) and reset.
 * @details Sink for configASSERT, the FreeRTOS stack and malloc hooks
 *          (DRT-002) and start-up failures.
 *
 * @param[in] reason ASSERT, STACK_OVERFLOW, MALLOC_FAILED or STARTUP.
 * @param[in] task   Task or stage name or NULL; truncated to 15 chars.
 * @param[in] file   Path or label or NULL; basename kept.
 * @param[in] line   Source line or reason-specific value.
 *
 * @warning Never returns.
 */
void app_fault_fatal(app_fault_reason_t reason, const char *task, const char *file, uint32_t line) __attribute__((noreturn));

/**
 * @brief Invalidate the record by clearing its magic.
 * @details Called by the HWLINK ClearFault handler. Leaves the payload and the
 *          error latch untouched; ClearFault clears those separately.
 * @warning Assumes no save is in progress.
 * @see app_error_clear_fault
 */
void app_fault_clear(void);

/** @} */

#endif /* APP_FAULT_H */
