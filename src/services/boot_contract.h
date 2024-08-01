/**
 * @file    boot_contract.h
 * @brief   Flash layout, hand-over and metadata contract shared with BOOT.
 * @details BOOT maps the active bank to 0x08000000 (`SYSCFG_MEMRMP.SWP_FB`), so
 *          one image linked at @ref APP_SLOT_BASE runs from slot A or B.
 *          Covers `LAY-001`..`LAY-005`, the Appendix B records, `RTC_BKP2R`
 *          hand-over (`HND-003`) and the `RTC_BKP1R` request (`ENT-002`).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (STM32F767ZIT6), dual-bank flash (nDBANK = 0)
 * @srs     DBT-001, DBT-004, DBT-005, DBT-006, HLT-002, DSAFE-002
 *
 * @warning Every value is a binary contract with BOOT; change both together.
 * @ingroup boot_contract
 */

#ifndef BOOT_CONTRACT_H
#define BOOT_CONTRACT_H

#include <stdint.h>

/**
 * @defgroup boot_contract BOOT flash and hand-over contract
 * @brief Addresses, sizes and magic words shared with the bootloader.
 * @details Each 1 MB bank: BOOT in sectors 0..1, metadata in sector 2, slot
 *          from sector 3 (RM0410):
 *
 *          0x08000000  +---------------------------+  bank 1 base
 *                      | BOOT, sectors 0..1        |
 *          0x08008000  | metadata, 16 KB (sector 2)|
 *          0x0800C000  | slot A, 976 KB            |
 *          0x08100000  +---------------------------+  bank 2 base
 *                      | same layout, mirror       |
 *          0x08108000  | metadata for slot B       |
 *          0x0810C000  | slot B, 976 KB            |
 *          0x08200000  +---------------------------+
 * @{
 */

/**
 * @name Slot geometry and bank layout
 * @{
 */

/**
 * @brief Application link address in bank 1 (`LAY-002`).
 * @details main() loads it into SCB->VTOR (`DBT-002`); ld/app.ld asserts it.
 */
#define APP_SLOT_BASE 0x0800C000UL
/**
 * @brief Application slot size: 976 KB (`DBT-001`).
 * @details Ends at the bank boundary; ld/app.ld fails the build on overflow.
 */
#define APP_SLOT_SIZE 0x000F4000UL

#define APP_BANK_SIZE   0x00100000UL /**< Flash bank size: 1 MB. */
#define APP_META_OFFSET 0x00008000UL /**< Metadata sector offset from bank base. */
#define APP_META_SIZE   0x00004000UL /**< Metadata sector size: 16 KB. */

#define APP_META_A_ADDR 0x08008000UL /**< Physical bank 1 metadata address. */
#define APP_META_B_ADDR 0x08108000UL /**< Physical bank 2 metadata address. */

/**
 * @brief Running slot's metadata address under the bank swap.
 * @warning Use @ref APP_META_A_ADDR or @ref APP_META_B_ADDR for the other slot.
 */
#define APP_RUNNING_META_ADDR 0x08008000UL

/** @} */

/**
 * @name Appendix B metadata records
 * @details Offsets from a metadata base; 32-byte records:
 *
 *          offset  state       magic
 *          0x000   header      (BOOT-owned, word 0)
 *          0x100   VERIFIED    0x56455249
 *          0x120   ACTIVATED   0x41435456
 *          0x140   CONFIRMED   0x434F4E46
 *          0x160   REJECTED    0x52454A45
 *
 * @note DEMO writes only CONFIRMED; the rest are BOOT-owned (`DBT-006`).
 * @{
 */

#define APP_META_HEADER_OFFSET    0x000U /**< BOOT-owned header; unused by DEMO. */
#define APP_META_VERIFIED_OFFSET  0x100U /**< VERIFIED record (BOOT-owned). */
#define APP_META_ACTIVATED_OFFSET 0x120U /**< ACTIVATED record (BOOT-owned). */
#define APP_META_CONFIRMED_OFFSET 0x140U /**< CONFIRMED record, written by DEMO. */
#define APP_META_REJECTED_OFFSET  0x160U /**< REJECTED record (BOOT-owned). */
/** @brief Record size and spacing in bytes. */
#define APP_META_RECORD_SIZE 32U

/* Magics are 32-bit constants; ASCII shown MSB first (stored little-endian). */
#define APP_META_VERIFIED_MAGIC  0x56455249UL /**< VERIFIED magic, "VERI". */
#define APP_META_ACTIVATED_MAGIC 0x41435456UL /**< ACTIVATED magic, "ACTV". */
#define APP_META_CONFIRMED_MAGIC 0x434F4E46UL /**< CONFIRMED magic, "CONF". */
#define APP_META_REJECTED_MAGIC  0x52454A45UL /**< REJECTED magic, "REJE". */

/**
 * @brief Running slot's CONFIRMED record: 0x08008140.
 * @details The only address DEMO may program (`DSAFE-002`, `DBT-006`).
 *          32-byte aligned for SCB_InvalidateDCache_by_Addr().
 * @warning Never erase this sector or program another metadata offset.
 */
#define APP_CONFIRMED_ADDR (APP_RUNNING_META_ADDR + APP_META_CONFIRMED_OFFSET)

/** @} */

/**
 * @name Hand-over record (RTC_BKP2R)
 * @details Written by BOOT before the jump (`HND-003`):
 *
 *          bits  0..23  magic `0x444E48` (memory bytes 'H','N','D')
 *          bits 24..25  slot: 1 = A, 2 = B, 0 = unknown
 *          bit  26      trial pending (1 = pending, 0 = confirmed)
 *          bits 27..28  trial counter, 0..3
 *          bits 29..31  not used by DEMO
 *
 * @note A zero word fails the magic check -> slot UNKNOWN (`DBT-004`). The
 *       backup domain survives reset but not a power cycle (VBAT tied to VDD).
 * @warning Bits 29..31 are reserved by BOOT.
 * @{
 */

#define APP_HANDOVER_MAGIC       0x00444E48UL /**< Magic, bits 0..23; bytes "HND". */
#define APP_HANDOVER_MAGIC_MASK  0x00FFFFFFUL /**< Selects the magic bits. */
#define APP_HANDOVER_SLOT_SHIFT  24U          /**< Slot field shift. */
#define APP_HANDOVER_SLOT_MASK   0x3UL        /**< Slot field mask; 0 or 3 = unknown. */
#define APP_HANDOVER_PENDING_BIT 26U          /**< Trial-pending flag bit. */
#define APP_HANDOVER_TRIAL_SHIFT 27U          /**< Trial counter shift. */
#define APP_HANDOVER_TRIAL_MASK  0x3UL        /**< Trial counter mask, 0..3. */

/** @} */

/**
 * @name Stay-in-BOOT request (RTC_BKP1R)
 * @{
 */

/**
 * @brief Magic in RTC_BKP1R that keeps BOOT in update mode (`ENT-002`).
 * @details Written by app_handover_request_bootloader() before a reset
 *          (`DBT-005`); memory bytes read "STAY".
 * @warning Set only immediately before a reset.
 */
#define APP_STAY_IN_BOOT_MAGIC 0x59415453UL /* bytes "STAY" */

/** @} */

/**
 * @name Reserved no-init window (DTCM)
 * @details BOOT uses DTCM 0x20000000..0x2000EFFF, so the fault record lives
 *          in the top 1 KB, which BOOT never writes; the main stack starts
 *          just below it. ld/app.ld and app_fault.c assert the fit
 *          (`DGEN-004`, A-9).
 * @{
 */

#define APP_NOINIT_ADDR 0x2001FC00UL /**< Window start; also the initial MSP. */
#define APP_NOINIT_SIZE 0x00000400UL /**< Window size: 1 KB. */
#define APP_NOINIT_END  0x20020000UL /**< Exclusive window end. */

/** @} */

/**
 * @name Slot identifiers
 * @brief Slot codes; equal to the hand-over slot field and Status values.
 * @{
 */

#define APP_SLOT_UNKNOWN 0U /**< No hand-over record or bad slot (`DBT-004`). */
#define APP_SLOT_A       1U /**< Slot A. */
#define APP_SLOT_B       2U /**< Slot B. */

/** @} */

/** @} */

#endif /* BOOT_CONTRACT_H */
