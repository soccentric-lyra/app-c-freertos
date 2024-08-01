/**
 * @file    iwdg.h
 * @brief   Independent watchdog: 4 s LSI timeout, single feeder (Health).
 * @details BOOT starts the IWDG at 4 s nominal (LSI /64, reload 1999) before
 *          hand-over (`HND-004`); DEMO only starts it when run stand-alone.
 *          The window is unused. Health refreshes every 100 ms while
 *          supervision holds and stops on loss or a 10 s failed health check
 *          (`HLT-003`); any other feeder would mask a stalled Health task.
 * @warning `DBG_IWDG_STOP` is not set, so a debugger halt longer than the
 *          timeout resets the board.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DBT-002, DRT-012, HLT-003, HND-004
 *
 * @ingroup iwdg
 */

#ifndef IWDG_H
#define IWDG_H

/**
 * @defgroup iwdg Independent watchdog service
 * @brief 4 s LSI watchdog refreshed by the Health task only.
 * @see health_task.c
 * @{
 */

/**
 * @brief   Reload the BOOT-started IWDG; first call in `main()` (`DBT-002`).
 * @details Writes only the reload key; BOOT's timeout is kept. Safe before
 *          clocks, HAL and NVIC set-up. No effect if the IWDG is not running.
 * @warning Start-up only; not a feed source.
 * @see app_iwdg_init
 */
void app_iwdg_early_kick(void);

/**
 * @brief   Start the IWDG at 4 s if unconfigured, then reload it.
 * @details @c PR / @c RLR at reset values (0, 0xFFF) means unconfigured: start
 *          the LSI, enable, program /64 and 1999, reload. Otherwise the
 *          inherited configuration is kept (`HND-004`). Waits are bounded, so a
 *          dead LSI cannot hang start-up.
 * @pre  Once from `main()`, before the scheduler starts.
 * @warning A watchdog started without writing @c PR / @c RLR (512 ms) looks
 *          unconfigured and is reprogrammed.
 * @see app_iwdg_kick
 */
void app_iwdg_init(void);

/**
 * @brief   Reload the IWDG (defers reset by 4 s nominal).
 * @details Unconditional single register write; the feed decision is the
 *          caller's. Silent no-op if the IWDG is stopped.
 * @warning Health task only (`DRT-012`, `HLT-003`); never from an ISR or
 *          another task.
 * @see health_task.c
 */
void app_iwdg_kick(void);

/** @} */

#endif /* IWDG_H */
