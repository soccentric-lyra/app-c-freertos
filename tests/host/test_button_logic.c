/*
 * test_button_logic.c - B1 debounce and press-classification tests
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  host (Linux/macOS) and CI
 *
 * The rules are: a level is accepted after 30 ms, a release below 1000 ms is
 * short, a press still held at 2000 ms is long exactly once, and the 1 s..2 s
 * gap produces nothing.  The task feeds one sample per call, so these tests
 * drive the same state machine at the same boundaries.
 */

#include "tasks/button_logic.h"
#include "test.h"

static int is_none(button_event_t event) { return event == BUTTON_EVENT_NONE; }

int test_button_logic(void)
{
    button_state_t state;

    /* Bouncing before the 30 ms window produces no event. */
    button_logic_init(&state, false, 0U);
    for(uint32_t t = 5U; t <= 25U; t += 5U)
    {
        CHECK(is_none(button_logic_update(&state, (t % 10U) != 0U, t)));
    }

    /* Short press: press accepted at 130, release at 1030 gives 900 ms. */
    button_logic_init(&state, false, 0U);
    CHECK(is_none(button_logic_update(&state, true, 100U)));
    CHECK(is_none(button_logic_update(&state, true, 129U)));
    CHECK(is_none(button_logic_update(&state, true, 130U)));
    CHECK(is_none(button_logic_update(&state, true, 500U)));
    CHECK(is_none(button_logic_update(&state, false, 1000U)));
    CHECK(is_none(button_logic_update(&state, false, 1029U)));
    CHECK(button_logic_update(&state, false, 1030U) == BUTTON_EVENT_SHORT);

    /* Exactly 1000 ms is no longer short, and below 2000 ms is not long. */
    button_logic_init(&state, false, 0U);
    CHECK(is_none(button_logic_update(&state, true, 100U)));
    CHECK(is_none(button_logic_update(&state, true, 130U)));
    CHECK(is_none(button_logic_update(&state, false, 1100U)));
    CHECK(is_none(button_logic_update(&state, false, 1130U)));

    /* A press held to 2000 ms emits one long event, not another on release. */
    button_logic_init(&state, false, 0U);
    CHECK(is_none(button_logic_update(&state, true, 100U)));
    CHECK(is_none(button_logic_update(&state, true, 130U)));
    CHECK(is_none(button_logic_update(&state, true, 2129U)));
    CHECK(button_logic_update(&state, true, 2130U) == BUTTON_EVENT_LONG);
    CHECK(is_none(button_logic_update(&state, true, 2200U)));
    CHECK(is_none(button_logic_update(&state, true, 2300U)));
    CHECK(is_none(button_logic_update(&state, false, 2300U)));
    CHECK(is_none(button_logic_update(&state, false, 2330U)));

    /* Release fallback: if the 2000 ms sample was missed, report on release. */
    button_logic_init(&state, false, 0U);
    CHECK(is_none(button_logic_update(&state, true, 100U)));
    CHECK(is_none(button_logic_update(&state, true, 130U)));
    CHECK(button_logic_update(&state, false, 2200U) == BUTTON_EVENT_LONG);
    CHECK(is_none(button_logic_update(&state, false, 2230U)));

    /* A button already held when the task starts is a press. */
    button_logic_init(&state, true, 500U);
    CHECK(is_none(button_logic_update(&state, true, 600U)));
    CHECK(is_none(button_logic_update(&state, false, 700U)));
    CHECK(button_logic_update(&state, false, 730U) == BUTTON_EVENT_SHORT);

    /* Tick wrap: unsigned deltas still measure the press correctly. */
    button_logic_init(&state, false, 0xFFFFFF00UL);
    CHECK(is_none(button_logic_update(&state, true, 0xFFFFFF10UL)));
    CHECK(is_none(button_logic_update(&state, true, 0xFFFFFF30UL)));
    CHECK(is_none(button_logic_update(&state, false, 0x00000010UL)));
    CHECK(button_logic_update(&state, false, 0x00000030UL) == BUTTON_EVENT_SHORT);

    return 1;
}
