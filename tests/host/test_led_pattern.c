/*
 * test_led_pattern.c - LED pattern engine tests
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  host (Linux/macOS) and CI
 *
 * The pattern engine is pure: it expands a pattern into segments and derives
 * the level from the absolute tick count.  These tests pin the segment tables,
 * the phase boundaries and the tick-wrap behaviour so a refactor cannot
 * silently change the visible LED rhythm.
 */

#include "tasks/led_pattern.h"
#include "tasks/led_task.h"
#include "test.h"

static uint32_t segment_total(const uint16_t dur[LED_PATTERN_MAX_SEGMENTS], uint32_t n)
{
    uint32_t total = 0U;
    for(uint32_t i = 0U; i < n; i++)
    {
        total += dur[i];
    }
    return total;
}

static int check_segment(uint32_t n, uint32_t index, uint16_t dur, bool on,
                         uint16_t want_dur, bool want_on)
{
    CHECK_U32(index < n, 1U);
    CHECK_U32(dur, want_dur);
    CHECK_U32(on ? 1U : 0U, want_on ? 1U : 0U);
    return 1;
}

int test_led_pattern(void)
{
    uint16_t dur[LED_PATTERN_MAX_SEGMENTS];
    bool     on[LED_PATTERN_MAX_SEGMENTS];
    led_cfg_t cfg = {APP_PATTERN_OFF, 1000U, 0U};
    bool     level = false;
    uint32_t next  = 0U;
    uint32_t n     = 0U;

    /* Short-press cycle wraps through every pattern and back to OFF. */
    CHECK_U32(led_pattern_next(APP_PATTERN_OFF), APP_PATTERN_ON);
    CHECK_U32(led_pattern_next(APP_PATTERN_ON), APP_PATTERN_BLINK);
    CHECK_U32(led_pattern_next(APP_PATTERN_BLINK), APP_PATTERN_HEARTBEAT);
    CHECK_U32(led_pattern_next(APP_PATTERN_HEARTBEAT), APP_PATTERN_FAST);
    CHECK_U32(led_pattern_next(APP_PATTERN_FAST), APP_PATTERN_CHASE);
    CHECK_U32(led_pattern_next(APP_PATTERN_CHASE), APP_PATTERN_MORSE_SOS);
    CHECK_U32(led_pattern_next(APP_PATTERN_MORSE_SOS), APP_PATTERN_OFF);
    CHECK_U32(led_pattern_next(APP_PATTERN_COUNT), APP_PATTERN_ON);

    /* OFF has no segments and no transition; ON is one long segment. */
    cfg.pattern = APP_PATTERN_OFF;
    CHECK_U32(led_pattern_segments(&cfg, false, dur, on), 0U);
    led_pattern_evaluate(&cfg, false, 0U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    CHECK_U32(next, LED_PATTERN_NO_TRANSITION);

    cfg.pattern = APP_PATTERN_ON;
    n = led_pattern_segments(&cfg, false, dur, on);
    CHECK_U32(n, 1U);
    CHECK(check_segment(n, 0U, dur[0], on[0], 0xFFFFU, true));
    led_pattern_evaluate(&cfg, false, 0U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, LED_PATTERN_NO_TRANSITION);

    /* BLINK: 500 ms on, 500 ms off for a 1000 ms period. */
    cfg.pattern = APP_PATTERN_BLINK;
    cfg.period  = 1000U;
    n           = led_pattern_segments(&cfg, false, dur, on);
    CHECK_U32(n, 2U);
    CHECK(check_segment(n, 0U, dur[0], on[0], 500U, true));
    CHECK(check_segment(n, 1U, dur[1], on[1], 500U, false));
    CHECK_U32(segment_total(dur, n), 1000U);

    cfg.start = 0U;
    led_pattern_evaluate(&cfg, false, 0U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 500U);
    led_pattern_evaluate(&cfg, false, 499U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 1U);
    led_pattern_evaluate(&cfg, false, 500U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    CHECK_U32(next, 500U);
    led_pattern_evaluate(&cfg, false, 1000U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 500U);

    /* FAST: period/5 cycle, split in half.  5 ms clamps to a 2 ms cycle. */
    cfg.pattern = APP_PATTERN_FAST;
    cfg.period  = 1000U;
    n           = led_pattern_segments(&cfg, false, dur, on);
    CHECK_U32(n, 2U);
    CHECK(check_segment(n, 0U, dur[0], on[0], 100U, true));
    CHECK(check_segment(n, 1U, dur[1], on[1], 100U, false));
    cfg.period = 5U;
    n          = led_pattern_segments(&cfg, false, dur, on);
    CHECK(check_segment(n, 0U, dur[0], on[0], 1U, true));
    CHECK(check_segment(n, 1U, dur[1], on[1], 1U, false));

    /* HEARTBEAT: 50 on, 150 off, 50 on, remainder off; at least 250 ms. */
    cfg.pattern = APP_PATTERN_HEARTBEAT;
    cfg.period  = 1000U;
    n           = led_pattern_segments(&cfg, false, dur, on);
    CHECK_U32(n, 4U);
    CHECK(check_segment(n, 0U, dur[0], on[0], 50U, true));
    CHECK(check_segment(n, 1U, dur[1], on[1], 150U, false));
    CHECK(check_segment(n, 2U, dur[2], on[2], 50U, true));
    CHECK(check_segment(n, 3U, dur[3], on[3], 750U, false));
    cfg.period = 200U;
    n          = led_pattern_segments(&cfg, false, dur, on);
    CHECK_U32(n, 3U);
    CHECK_U32(segment_total(dur, n), 250U);
    cfg.period = 1000U;
    cfg.start  = 0U;
    led_pattern_evaluate(&cfg, false, 0U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 50U);
    led_pattern_evaluate(&cfg, false, 50U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    CHECK_U32(next, 150U);
    led_pattern_evaluate(&cfg, false, 200U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 50U);
    led_pattern_evaluate(&cfg, false, 250U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    CHECK_U32(next, 750U);
    led_pattern_evaluate(&cfg, false, 999U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    CHECK_U32(next, 1U);
    led_pattern_evaluate(&cfg, false, 1000U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 50U);

    /* CHASE: the second LED is the inverse half of the pair. */
    cfg.pattern = APP_PATTERN_CHASE;
    cfg.period  = 1000U;
    cfg.start   = 0U;
    n           = led_pattern_segments(&cfg, false, dur, on);
    CHECK(check_segment(n, 0U, dur[0], on[0], 500U, true));
    CHECK(check_segment(n, 1U, dur[1], on[1], 500U, false));
    n = led_pattern_segments(&cfg, true, dur, on);
    CHECK(check_segment(n, 0U, dur[0], on[0], 500U, false));
    CHECK(check_segment(n, 1U, dur[1], on[1], 500U, true));
    led_pattern_evaluate(&cfg, false, 0U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    led_pattern_evaluate(&cfg, true, 0U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    led_pattern_evaluate(&cfg, false, 500U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    led_pattern_evaluate(&cfg, true, 500U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);

    /* MORSE_SOS: 18 segments, 3400 ms, fixed regardless of period. */
    cfg.pattern = APP_PATTERN_MORSE_SOS;
    cfg.period  = 1000U;
    n           = led_pattern_segments(&cfg, false, dur, on);
    CHECK_U32(n, 18U);
    CHECK_U32(segment_total(dur, n), 3400U);
    cfg.start = 0U;
    led_pattern_evaluate(&cfg, false, 3399U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 0U);
    CHECK_U32(next, 1U);
    led_pattern_evaluate(&cfg, false, 3400U, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 100U);

    /* An unknown pattern falls back to the SOS rhythm. */
    cfg.pattern = 0xFEU;
    n           = led_pattern_segments(&cfg, false, dur, on);
    CHECK_U32(n, 18U);

    /* Tick wrap: the unsigned delta keeps the phase across 0xFFFFFFFF. */
    cfg.pattern = APP_PATTERN_BLINK;
    cfg.period  = 1000U;
    cfg.start   = 0xFFFFFF00UL;
    led_pattern_evaluate(&cfg, false, 0x00000010UL, &level, &next);
    CHECK_U32(level ? 1U : 0U, 1U);
    CHECK_U32(next, 228U); /* 272 ms into the 500 ms on segment */

    return 1;
}
