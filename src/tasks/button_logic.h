/**
 * @file    button_logic.h
 * @brief   Pure debounce and press-classification state machine for B1.
 * @details The Button task owns the GPIO and the EXTI line, but the timing
 *          rules are pure: feed the raw level and the tick count, get one
 *          event.  Keeping the rules here makes them testable on the host.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6 (and host for tests)
 * @srs     BTN-001..BTN-003
 * @ingroup button_task
 */

#ifndef BUTTON_LOGIC_H
#define BUTTON_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Debounce interval in milliseconds (30 ms). */
#define BUTTON_DEBOUNCE_MS 30U
/** @brief Upper bound of a short press in milliseconds, exclusive. */
#define BUTTON_SHORT_MAX_MS 1000U
/** @brief Threshold of a long press in milliseconds, inclusive. */
#define BUTTON_LONG_MS 2000U

/**
 * @brief Debounce and press-classification state.
 * @details Initialise with @ref button_logic_init and feed every sample to
 *          @ref button_logic_update.  The structure is private to the caller;
 *          the fields are exposed only so it can live on the task stack.
 */
typedef struct
{
    bool     stable;          /**< Debounced level. */
    bool     candidate;       /**< Level waiting to become stable. */
    bool     pressed;         /**< A debounced press is in progress. */
    bool     long_sent;       /**< The long event was already emitted. */
    uint32_t candidate_since; /**< Tick at which @c candidate was first seen. */
    uint32_t press_start;     /**< Tick at which the press became stable. */
} button_state_t;

/** @brief Result of one sample. */
typedef enum
{
    BUTTON_EVENT_NONE = 0, /**< No press event. */
    BUTTON_EVENT_SHORT,    /**< Short press: cycle the LED pattern. */
    BUTTON_EVENT_LONG      /**< Long press: restore the defaults. */
} button_event_t;

/**
 * @brief Initialise the state from the first raw sample.
 * @param[out] state State to initialise.
 * @param[in]  raw   Raw level, true when pressed.
 * @param[in]  now   Current tick count.
 */
void button_logic_init(button_state_t *state, bool raw, uint32_t now);

/**
 * @brief Feed one raw sample and return the event it produced.
 * @param[in,out] state State from @ref button_logic_init.
 * @param[in]     raw   Raw level, true when pressed.
 * @param[in]     now   Current tick count.
 * @return @ref BUTTON_EVENT_NONE, @ref BUTTON_EVENT_SHORT or
 *         @ref BUTTON_EVENT_LONG.
 */
button_event_t button_logic_update(button_state_t *state, bool raw, uint32_t now);

#endif /* BUTTON_LOGIC_H */
