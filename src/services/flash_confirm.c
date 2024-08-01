/**
 * @file    flash_confirm.c
 * @brief   Program the CONFIRMED metadata record of the running slot.
 * @details Record states: valid (magic and CRC match), absent (all 0xFF, the
 *          only state ever programmed) and torn (anything else, left for BOOT).
 *          The file keeps no RAM state; idempotency is decided by reading flash.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6, dual-bank flash
 * @srs     HLT-002, HLT-005, DBT-006, DSAFE-002
 * @ingroup flash_confirm
 */

#include "stm32f7xx_hal.h"
#include "services/boot_contract.h"
#include "services/crc32.h"
#include "services/flash_confirm.h"

/** @brief 32-bit words per metadata record (8); the last one holds the CRC. */
#define CONFIRMED_WORDS (APP_META_RECORD_SIZE / 4U)

/**
 * @brief Test whether a CONFIRMED record has the magic and a matching CRC.
 * @param[in] record Record in flash, word aligned; D-cache already invalidated.
 * @return true if word 0 is the magic and word 7 is the CRC of words 0..6.
 */
static bool record_valid(const volatile uint32_t *record)
{
    if(record[0] != APP_META_CONFIRMED_MAGIC)
    {
        return false;
    }
    return app_crc32((const void *)record, APP_META_RECORD_SIZE - 4U) == record[CONFIRMED_WORDS - 1U];
}

/**
 * @brief Test whether every word of a record reads 0xFFFFFFFF.
 * @param[in] record Record in flash, word aligned; D-cache already invalidated.
 * @return true if the record is fully erased.
 */
static bool record_erased(const volatile uint32_t *record)
{
    for(uint32_t i = 0U; i < CONFIRMED_WORDS; i++)
    {
        if(record[i] != 0xFFFFFFFFUL)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Program the running slot's CONFIRMED record if it is erased.
 * @details Flash is cacheable, so the D-cache is invalidated before each
 *          read. Flash is locked again on every path after unlock.
 *
 * @return true if a valid CONFIRMED record is present after the call.
 * @retval false Unlock, program or verify failed, or the record is torn.
 *
 * @pre   @ref APP_CONFIRMED_ADDR is 32-byte aligned for the cache maintenance.
 * @note  No __DSB() follows the cache invalidations; reads rely on program
 *        order and the HAL busy wait.
 * @warning Not reentrant; never erase here (the sector holds BOOT records).
 */
bool app_flash_confirm_running_slot(void)
{
    /* DSAFE-002: fixed address, so the write cannot target another slot. */
    const uint32_t     address = APP_CONFIRMED_ADDR;
    volatile uint32_t *record  = (volatile uint32_t *)address;

    /* Flash keeps default cacheable attributes; drop stale lines. */
    SCB_InvalidateDCache_by_Addr((uint32_t *)address, APP_META_RECORD_SIZE);

    if(record_valid(record))
    {
        return true;
    }
    /* Torn record from an interrupted write: leave recovery to BOOT. */
    if(!record_erased(record))
    {
        return false;
    }

    /* Build the whole record, CRC included, before touching flash. */
    uint32_t words[CONFIRMED_WORDS];
    for(uint32_t i = 0U; i < CONFIRMED_WORDS; i++)
    {
        words[i] = 0U;
    }
    words[0]                    = APP_META_CONFIRMED_MAGIC;
    words[CONFIRMED_WORDS - 1U] = app_crc32((const void *)words, APP_META_RECORD_SIZE - 4U);

    if(HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }
    /* Stale status from an earlier operation would abort the first program. */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_ALL_ERRORS);

    bool ok = true;
    for(uint32_t i = 0U; i < CONFIRMED_WORDS; i++)
    {
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + (i * 4U), words[i]) != HAL_OK)
        {
            ok = false;
            break;
        }
        /* The program bypassed the D-cache; invalidate before reading back. */
        SCB_InvalidateDCache_by_Addr((uint32_t *)address, APP_META_RECORD_SIZE);
        if(*(volatile uint32_t *)(address + (i * 4U)) != words[i])
        {
            ok = false;
            break;
        }
    }

    HAL_FLASH_Lock();
    /* Final verification must read flash, not a refilled cache line. */
    SCB_InvalidateDCache_by_Addr((uint32_t *)address, APP_META_RECORD_SIZE);
    return ok && record_valid(record);
}
