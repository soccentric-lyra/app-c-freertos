/**
 * @file    button_logic.c
 * @brief   Pure debounce and press-classification state machine for B1.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6 (and host for tests)
 * @srs     BTN-001..BTN-003
 * @ingroup button_task
 */

#include "tasks/button_logic.h"

void button_logic_init(button_state_t *state, bool raw, uint32_t now)
{
    state->stable          = raw;
    state->candidate       = raw;
    state->candidate_since = now;
    state->press_start     = now;
    state->pressed         = raw;
    state->long_sent       = false;
}

button_event_t button_logic_update(button_state_t *state, bool raw, uint32_t now)
{
    button_event_t event = BUTTON_EVENT_NONE;

    if(raw != state->candidate)
    {
        state->candidate       = raw;
        state->candidate_since = now;
    }
    else if(state->candidate != state->stable && (now - state->candidate_since) >= BUTTON_DEBOUNCE_MS)
    {
        state->stable = state->candidate;
        if(state->stable)
        {
            state->pressed     = true;
            state->long_sent   = false;
            state->press_start = now;
        }
        else
        {
            uint32_t duration = now - state->press_start;
            if(state->pressed && !state->long_sent)
            {
                if(duration < BUTTON_SHORT_MAX_MS)
                {
                    event = BUTTON_EVENT_SHORT;
                }
                else if(duration >= BUTTON_LONG_MS)
                {
                    event = BUTTON_EVENT_LONG;
                }
            }
            state->pressed = false;
        }
    }

    if(state->pressed && !state->long_sent && (now - state->press_start) >= BUTTON_LONG_MS)
    {
        state->long_sent = true;
        event            = BUTTON_EVENT_LONG;
    }

    return event;
}
