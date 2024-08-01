/**
 * @file    task_report.h
 * @brief   Once-a-second "task <n> <name>: <purpose>" line on the debug log.
 * @details Each application task calls task_report() every loop iteration; it
 *          prints at most once per second per task, so one terminal shows that
 *          all tasks are scheduled.
 */
#ifndef TASK_REPORT_H
#define TASK_REPORT_H

#include <stdint.h>

/** @brief Per-task report state; zero-initialise it. */
typedef struct
{
    uint32_t last_ms; /**< Time of the last printed line, ms. */
    uint8_t  primed;  /**< Non-zero once the first line was printed. */
} task_report_t;

/**
 * @brief Log the task's status line if 1 s has passed since the last one.
 * @param[in,out] state   Calling task's own report state.
 * @param[in]     number  Task number, 1 to 4.
 * @param[in]     name    Task name as given to xTaskCreate().
 * @param[in]     purpose One-sentence description of the task.
 */
void task_report(task_report_t *state, unsigned number, const char *name, const char *purpose);

#endif /* TASK_REPORT_H */
