/**
 * @file    led_pattern.h
 * @brief   Pure LED pattern engine: segment expansion and phase evaluation.
 * @details The pattern maths lives here, away from FreeRTOS and the HAL, so
 *          the same code can run on the target and in the host tests.  The
 *          LED task owns the queue and the GPIO writes; this module only
 *          answers "is the LED on now, and when is the next edge?".
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6 (and host for tests)
 * @srs     LED-001..LED-007, BTN-002
 * @ingroup led_task
 */

#ifndef LED_PATTERN_H
#define LED_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Sentinel delay meaning "no segment boundary to wait for".
 * @details Returned by @ref led_pattern_evaluate for OFF, ON and a cycle whose
 *          segments sum to zero.  It is the largest @c uint32_t so the
 *          @c if (next < wait) comparisons in the LED task can never select
 *          it.
 */
#define LED_PATTERN_NO_TRANSITION 0xFFFFFFFFUL

/**
 * @brief Maximum number of segments in one pattern cycle.
 * @details The Morse SOS pattern is the longest at 18 segments, so 20 leaves
 *          room for two future additions.
 */
#define LED_PATTERN_MAX_SEGMENTS 20U

/**
 * @brief Pattern state of one LED.
 * @details Holds the pattern, period and the tick that anchors the current
 *          cycle.  @ref led_pattern_evaluate never modifies the structure; it
 *          derives the level and the time to the next edge from @c start and
 *          the current tick count, so a pattern keeps its phase and only a
 *          new request rewrites @c pattern, @c period and @c start.
 */
typedef struct
{
    uint8_t  pattern; /**< One of the APP_PATTERN_* identifiers. */
    uint16_t period;  /**< Configured cycle length in milliseconds. */
    uint32_t start;   /**< Tick at which the current cycle phase began. */
} led_cfg_t;

/**
 * @brief Advance a pattern identifier to the next one in wrap order.
 * @param[in] current Pattern identifier the LED currently uses.
 * @return The next pattern identifier, wrapping to APP_PATTERN_OFF.
 */
uint32_t led_pattern_next(uint32_t current);

/**
 * @brief Expand a pattern into its list of on/off segments.
 * @param[in]  cfg        LED configuration supplying pattern and period.
 * @param[in]  second_led @c false for the LD1/LD3 phase, @c true for LD2.
 * @param[out] dur        Segment durations in milliseconds.
 * @param[out] on         Segment levels.
 * @return Number of segments written, 0..18.
 * @pre Both arrays hold at least @ref LED_PATTERN_MAX_SEGMENTS entries.
 */
uint32_t led_pattern_segments(const led_cfg_t *cfg, bool second_led, uint16_t dur[LED_PATTERN_MAX_SEGMENTS], bool on[LED_PATTERN_MAX_SEGMENTS]);

/**
 * @brief Compute the LED level now and the delay to the next segment edge.
 * @param[in]  cfg        LED configuration, including the cycle anchor.
 * @param[in]  second_led @c false for the LD1/LD3 phase, @c true for LD2.
 * @param[in]  now        Current tick count.
 * @param[out] level      @c true when the LED is lit in the current segment.
 * @param[out] next_ms    Delay to the next edge, or @ref
 *                        LED_PATTERN_NO_TRANSITION when there is none.
 * @pre @c cfg::start was set to a tick count.
 * @post Both output parameters are always written.
 */
void led_pattern_evaluate(const led_cfg_t *cfg, bool second_led, uint32_t now, bool *level, uint32_t *next_ms);

#endif /* LED_PATTERN_H */
