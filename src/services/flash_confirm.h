/**
 * @file    flash_confirm.h
 * @brief   Program the CONFIRMED metadata record of the running slot.
 * @details The only flash write in DEMO (`DBT-006`). Record: word 0
 *          @ref APP_META_CONFIRMED_MAGIC, words 1..6 zero, word 7 CRC-32 of
 *          bytes 0..27. Never erases: the sector also holds BOOT's records.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6, dual-bank flash
 * @srs     HLT-002, HLT-005, DBT-006, DSAFE-002
 * @see     boot_contract.h
 * @ingroup flash_confirm
 */

#ifndef FLASH_CONFIRM_H
#define FLASH_CONFIRM_H

#include <stdbool.h>

/**
 * @defgroup flash_confirm CONFIRMED metadata writer
 * @brief Idempotent single-record flash write that confirms the running slot.
 * @{
 */

/**
 * @brief Program the running slot's CONFIRMED record if it is erased.
 * @details Address is fixed at @ref APP_CONFIRMED_ADDR (`DSAFE-002`). A valid
 *          record returns true without unlocking flash; a torn record
 *          (neither valid nor all 0xFF) returns false untouched. Each word is
 *          read back, and the whole record is re-verified at the end.
 *
 * @return true if a valid CONFIRMED record is present after the call.
 * @retval false Unlock, program or verify failed, or the record is torn.
 *
 * @pre   Task context, after the health check passed (only the Health task
 *        calls it, `HLT-002`, `HLT-005`).
 * @post  The hand-over snapshot is not updated; the caller sets trial state.
 * @note  Blocks for up to eight word programs.
 * @warning Not reentrant or ISR-safe. A failed write leaves a torn record that
 *          only a BOOT erase can clear; do not add an erase here.
 * @see app_crc32
 */
bool app_flash_confirm_running_slot(void);

/** @} */

#endif /* FLASH_CONFIRM_H */
