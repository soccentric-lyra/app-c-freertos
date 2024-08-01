/**
 * @file    hal_timebase.c
 * @brief   HAL 1 ms time base on TIM6 instead of SysTick (DGEN-006).
 * @details Overrides the weak HAL tick functions; SysTick stays with FreeRTOS
 *          (`DRT-001`). The tick ISR runs at priority 15, is masked by FreeRTOS
 *          critical sections and calls no RTOS API.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DGEN-006 - the HAL time base uses a hardware timer other than
 *          SysTick (SysTick belongs to FreeRTOS, DRT-001).  Same scheme as
 *          the validation firmware (HWVAL GEN-012).
 * @ingroup clock
 */

#include "stm32f7xx_hal.h"

/** TIM6 update interrupt handler; defined below. */
void TIM6_DAC_IRQHandler(void);

/**
 * @brief   Start TIM6 at a 1 MHz count with a 1 ms update interrupt.
 * @details Timer clock is PCLK1, or 2 x PCLK1 when the APB1 prescaler is not 1
 *          (RM0410). Called by @c HAL_Init() and again by
 *          @c HAL_RCC_ClockConfig() after the clock switch. URS is set so the
 *          UG reload raises no tick.
 * @param[in] TickPriority NVIC priority 0..15 for TIM6_DAC_IRQn; also stored
 *            in uwTickPrio. Not range-checked (the HAL default does).
 * @return HAL_OK always.
 * @note  Below a 1 MHz timer clock the divider clamps to 1 and the period
 *        exceeds 1 ms. Not thread-safe; start-up use only.
 * @warning Period is fixed at 1 ms; @c HAL_SetTickFreq() would mis-scale uwTick.
 * @warning Never call @c HAL_Delay() from an ISR: the tick cannot preempt it.
 * @see TIM6_DAC_IRQHandler, app_clock_init
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    /* APB timers run at PCLK x2 unless the APB prescaler is 1 (RM0410). */
    uint32_t timclk  = ((RCC->CFGR & RCC_CFGR_PPRE1) == 0U) ? pclk1 : pclk1 * 2U;
    uint32_t divider = timclk / 1000000U;
    if(divider == 0U)
    {
        divider = 1U; /* < 1 MHz: avoid underflow */
    }
    uint32_t prescaler = divider - 1U;

    __HAL_RCC_TIM6_CLK_ENABLE();

    TIM6->CR1  = 0U;
    TIM6->PSC  = prescaler;  /* 1 MHz count */
    TIM6->ARR  = 1000U - 1U; /* 1000 counts = 1 ms */
    TIM6->EGR  = TIM_EGR_UG; /* latch PSC/ARR, reset counter */
    TIM6->SR   = 0U;
    TIM6->DIER = TIM_DIER_UIE;
    TIM6->CR1  = TIM_CR1_CEN | TIM_CR1_URS;

    uwTickPrio = TickPriority;
    NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    return HAL_OK;
}

/**
 * @brief   Stop the HAL tick by masking the TIM6 update interrupt.
 * @note    TIM6 keeps counting; suspended time is not added to uwTick.
 *          Currently unused.
 * @see HAL_ResumeTick
 */
void HAL_SuspendTick(void) { TIM6->DIER &= ~TIM_DIER_UIE; }

/**
 * @brief   Clear a stale update flag and unmask the TIM6 update interrupt.
 * @pre  @ref HAL_InitTick has run. Currently unused.
 * @see HAL_SuspendTick
 */
void HAL_ResumeTick(void)
{
    TIM6->SR = 0U;
    TIM6->DIER |= TIM_DIER_UIE;
}

/**
 * @brief   Clear UIF and add uwTickFreq to uwTick (1 ms per update).
 * @note    ISR at priority 15. Coalesced updates count once, so uptime can lag.
 * @warning The vector is shared with the DAC; every interrupt is assumed to be
 *          a TIM6 update.
 */
void TIM6_DAC_IRQHandler(void)
{
    if((TIM6->SR & TIM_SR_UIF) != 0U)
    {
        TIM6->SR = ~TIM_SR_UIF;
        uwTick += uwTickFreq;
    }
}
