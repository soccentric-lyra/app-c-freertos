/**
 * @file    handover.h
 * @brief   BOOT hand-over record in the RTC backup domain.
 * @details BOOT leaves one word in @c RTC_BKP2R (HND-003):
 *          - bits  0..23  magic @c 0x444E48 ("HND")
 *          - bits 24..25  slot: 0 unknown, 1 A, 2 B
 *          - bit  26      trial pending
 *          - bits 27..28  trial counter (0..3)
 *
 *          A missing record means DEMO was not started by BOOT: slot
 *          @c UNKNOWN, no confirmation (DBT-004). VBAT is tied to VDD, so the
 *          record survives a reset but not a power cycle.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DBT-004, DBT-005, HND-003, ENT-002
 * @ingroup handover
 */

#ifndef HANDOVER_H
#define HANDOVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup handover BOOT hand-over record
 * @brief Decoded @c RTC_BKP2R state and the "stay in BOOT" request.
 * @details Bit packing is shared with BOOT in @ref boot_contract.h.
 * @{
 */

/**
 * @brief Decoded hand-over record.
 * @note  @c slot and @c trial use the HWLINK @c Slot / @c TrialState values,
 *        so @ref comms_task casts them directly. A BOOT snapshot, not live
 *        flash state.
 * @see app_handover_read
 */
typedef struct
{
    bool     present;     /**< True when the magic matched and the slot is A/B. */
    uint8_t  slot;        /**< @c APP_SLOT_UNKNOWN, @c APP_SLOT_A or @c APP_SLOT_B. */
    uint8_t  trial;       /**< 0 unknown, 1 confirmed, 2 pending (hwlink_TrialState). */
    uint8_t  trial_count; /**< BOOT trial counter, 0..3; 0 when record absent. */
    uint32_t raw;         /**< Raw @c RTC_BKP2R word, magic and unused bits included. */
} app_handover_t;

/**
 * @brief Decoded hand-over record, written once at start-up.
 * @note  Only @ref health_task writes it afterwards (@c trial 2 -> 1 after
 *        confirmation); that is a single byte store, so readers need no lock.
 */
extern app_handover_t g_handover;

/**
 * @brief Decode @c RTC_BKP2R into @ref g_handover.
 * @details A bad magic or a slot other than A/B (including an erased register)
 *          yields slot @c UNKNOWN, trial 0, @c present false (DBT-004). Enables
 *          no clock and no backup-domain access.
 * @pre  Start-up thread only, after @c app_clock_init() and before the
 *       scheduler starts.
 * @see app_handover_request_bootloader, boot_contract.h
 */
void app_handover_read(void);

/**
 * @brief Write the "stay in BOOT" magic to @c RTC_BKP1R (DBT-005).
 * @details Enables the PWR/RTC clocks and backup-domain access, writes
 *          @c APP_STAY_IN_BOOT_MAGIC and issues @c __DSB(). Does not reset.
 * @pre  Thread context; normally @ref comms_task after the @c EnterBootloader
 *       @c Ack (COM-006).
 * @warning Leaves the backup domain writable; the caller must reset next.
 * @see APP_STAY_IN_BOOT_MAGIC
 */
void app_handover_request_bootloader(void);

/** @} */

#endif /* HANDOVER_H */
