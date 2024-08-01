/**
 * @file    uart3.c
 * @brief   USART3 VCP driver: circular DMA RX, DMA TX and interrupt handling.
 * @details The CPU never reads @c RDR: the RX write position comes from the
 *          DMA remaining count plus a wrap counter. Line errors are cleared via
 *          @c ICR in @ref USART3_IRQHandler because @c HAL_UART_IRQHandler
 *          aborts the circular receiver on overrun (RM0410 30.6.9).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (PD8 TX, PD9 RX)
 * @srs     COM-001, COM-002, DCON-004
 * @ingroup uart3
 *
 * @note Sole owner of @ref HAL_UART_MspInit, the USART3 HAL callbacks and
 *       the USART3/DMA1 Stream1/Stream3 IRQ handlers.
 * @see uart3.h, cache_mpu.h, comms_task.h
 */

#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "drivers/uart3.h"

/** RX ring size in bytes; also the circular DMA length (COM-002 needs >= 2 KB). */
#define UART_RX_RING_SIZE 4096U
/** Nominal TX timeout in ms; unused here (callers pass their own). */
#define UART_TX_TIMEOUT_MS 200U

/**
 * @brief USART3 HAL handle.
 * @note  Zero-initialised, so the first @c HAL_UART_Init runs the MSP hook.
 *        Used from start-up, the Comms task and ISRs without a lock.
 */
static UART_HandleTypeDef huart3;
/** RX DMA: DMA1 Stream1 ch4, circular, peripheral-to-memory. */
static DMA_HandleTypeDef hdma_usart3_rx;
/** TX DMA: DMA1 Stream3 ch4, normal, memory-to-peripheral. */
static DMA_HandleTypeDef hdma_usart3_tx;

/**
 * @brief Circular RX ring filled by DMA1 Stream1.
 * @warning Must stay in non-cacheable @c .dma_buffer; otherwise the reader sees
 *          stale bytes.
 */
static uint8_t uart_rx_ring[UART_RX_RING_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
/** Ring offset of the next unread byte (0..UART_RX_RING_SIZE-1); task-only. */
static uint32_t uart_rx_read;
/**
 * @brief Bytes consumed since arming; same domain as the produced count, so
 *        backlog = produced - consumed.
 */
static uint32_t uart_rx_consumed;
/** Completed full-ring DMA transfers since arming; incremented in ISR context. */
static volatile uint32_t uart_rx_wraps;
/** Last successfully configured rate in baud. */
static uint32_t uart_baud = APP_UART_DEFAULT_BAUD;
/**
 * @brief Task awaiting TX completion, or NULL; read in ISR context.
 * @warning Single slot: only one sender (the Comms task) is supported.
 */
static volatile TaskHandle_t uart_tx_task;

/** Configure PD8 (TX) / PD9 (RX) as AF7 push-pull, pull-up, very high speed. */
static void uart_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9; /* PD8 TX, PD9 RX */
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);
}

/**
 * @brief Initialise both USART3 DMA streams, link them and enable their IRQs.
 * @details Byte transfers, FIFO off; NVIC priority 6 permits FromISR calls
 *          (DRT-003). Re-run on every @c HAL_UART_Init; no MSP de-init exists.
 * @note  @c HAL_DMA_Init results are ignored; a failure surfaces when RX is
 *        started.
 */
static void uart_dma_init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_usart3_rx.Instance                 = DMA1_Stream1;
    hdma_usart3_rx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart3_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart3_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart3_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart3_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart3_rx.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
    hdma_usart3_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    hdma_usart3_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
    hdma_usart3_rx.Init.MemBurst            = DMA_MBURST_SINGLE;
    hdma_usart3_rx.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    (void)HAL_DMA_Init(&hdma_usart3_rx);

    hdma_usart3_tx.Instance                 = DMA1_Stream3;
    hdma_usart3_tx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart3_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart3_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart3_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart3_tx.Init.Mode                = DMA_NORMAL;
    hdma_usart3_tx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_usart3_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    hdma_usart3_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_1QUARTERFULL;
    hdma_usart3_tx.Init.MemBurst            = DMA_MBURST_SINGLE;
    hdma_usart3_tx.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    (void)HAL_DMA_Init(&hdma_usart3_tx);

    __HAL_LINKDMA(&huart3, hdmarx, hdma_usart3_rx);
    __HAL_LINKDMA(&huart3, hdmatx, hdma_usart3_tx);

    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
}

/**
 * @brief HAL MSP hook: USART3 clock, pins, DMA and NVIC (priority 6).
 * @param[in] huart Handle being initialised; other instances are ignored.
 * @warning Priority 6 must stay below @c configMAX_SYSCALL_INTERRUPT_PRIORITY
 *          (DRT-003).
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART3)
    {
        return;
    }

    __HAL_RCC_USART3_CLK_ENABLE();
    uart_gpio_init();
    uart_dma_init();

    HAL_NVIC_SetPriority(USART3_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

/**
 * @brief Reset ring counters, start circular RX DMA, enable ERR and IDLE IRQs.
 * @details Counters are cleared before arming so no stale wrap or backlog is
 *          seen.
 * @return true when RX DMA started.
 * @note  Must not race with @ref app_uart3_rx_read.
 */
static bool uart_start_receiver(void)
{
    uart_rx_read     = 0U;
    uart_rx_consumed = 0U;
    uart_rx_wraps    = 0U;
    if(HAL_UART_Receive_DMA(&huart3, uart_rx_ring, UART_RX_RING_SIZE) != HAL_OK)
    {
        return false;
    }
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_ERR);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
    return true;
}

/** Initialise USART3 at the default rate, 8N1, and arm the RX ring. */
bool app_uart3_init(void)
{
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = APP_UART_DEFAULT_BAUD;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    if(HAL_UART_Init(&huart3) != HAL_OK)
    {
        return false;
    }
    uart_baud = APP_UART_DEFAULT_BAUD;
    return uart_start_receiver();
}

/**
 * @brief Re-initialise USART3 at @p baud; roll back to the old rate on failure.
 * @note  The rollback receiver restart result is ignored; check
 *        @ref app_uart3_rx_running.
 */
bool app_uart3_set_baud(uint32_t baud)
{
    if(baud != APP_UART_DEFAULT_BAUD && baud != APP_UART_FAST_BAUD)
    {
        return false;
    }
    if(baud == uart_baud)
    {
        return true;
    }

    uint32_t old_baud = uart_baud;
    (void)HAL_UART_DMAStop(&huart3);
    (void)HAL_UART_DeInit(&huart3);

    huart3.Init.BaudRate = baud;
    bool ok              = HAL_UART_Init(&huart3) == HAL_OK && uart_start_receiver();
    if(!ok)
    {
        (void)HAL_UART_DeInit(&huart3);
        huart3.Init.BaudRate = old_baud;
        if(HAL_UART_Init(&huart3) == HAL_OK)
        {
            (void)uart_start_receiver();
        }
        return false;
    }

    uart_baud = baud;
    return true;
}

/** Return the cached link rate in baud. */
uint32_t app_uart3_baud(void) { return uart_baud; }

/**
 * @brief Copy unread bytes from the RX ring.
 * @details produced = wraps * size + (size - DMA counter) % size. A backlog
 *          above the ring size means a lap: resync and return 0. A backwards
 *          delta (counter reloaded before the wrap increment) returns 0 and is
 *          resolved on the next call.
 */
uint32_t app_uart3_rx_read(uint8_t *dst, uint32_t max_len)
{
    /* Retry if a wrap lands between the two reads, keeping them consistent. */
    uint32_t wraps   = 0U;
    uint32_t counter = 0U;
    do
    {
        wraps   = uart_rx_wraps;
        counter = __HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
    } while(wraps != uart_rx_wraps);

    uint32_t head     = (UART_RX_RING_SIZE - counter) % UART_RX_RING_SIZE;
    uint32_t produced = (wraps * UART_RX_RING_SIZE) + head;

    uint32_t delta = produced - uart_rx_consumed;
    if(delta == 0U || max_len == 0U)
    {
        return 0U;
    }
    if(delta > (0xFFFFFFFFUL - UART_RX_RING_SIZE))
    {
        /* Counter reloaded before the wrap was counted; not a lap. Retry later. */
        return 0U;
    }
    if(delta > UART_RX_RING_SIZE)
    {
        /* Lapped: bytes were overwritten. Resync rather than return stale data. */
        uart_rx_read     = head;
        uart_rx_consumed = produced;
        return 0U;
    }

    uint32_t count = delta;
    if(count > max_len)
    {
        count    = max_len;
        produced = uart_rx_consumed + count;
    }
    for(uint32_t i = 0U; i < count; i++)
    {
        dst[i] = uart_rx_ring[(uart_rx_read + i) % UART_RX_RING_SIZE];
    }
    uart_rx_read     = (uart_rx_read + count) % UART_RX_RING_SIZE;
    uart_rx_consumed = produced;
    return count;
}

/** True when the HAL RX state, RX DMA state and @c CR1.RE all show reception. */
bool app_uart3_rx_running(void) { return huart3.RxState == HAL_UART_STATE_BUSY_RX && hdma_usart3_rx.State == HAL_DMA_STATE_BUSY && (USART3->CR1 & USART_CR1_RE) != 0U; }

/** Clear RX error flags, abort the HAL receive and re-arm; no-op while running. */
bool app_uart3_rx_restart(void)
{
    if(app_uart3_rx_running())
    {
        return true;
    }
    USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF | USART_ICR_IDLECF;
    (void)HAL_UART_AbortReceive(&huart3);
    return uart_start_receiver();
}

/**
 * @brief Send one frame by DMA, then wait up to 10 ms for @c TC (DBT-005).
 * @warning NULL @p data with non-zero @p len returns true without sending.
 */
bool app_uart3_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if(len == 0U || data == NULL)
    {
        return true;
    }

    uart_tx_task = xTaskGetCurrentTaskHandle();
    (void)ulTaskNotifyTake(pdTRUE, 0U); /* drop a stale completion */

    if(HAL_UART_Transmit_DMA(&huart3, (uint8_t *)data, len) != HAL_OK)
    {
        uart_tx_task = NULL;
        return false;
    }

    if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) == 0U)
    {
        (void)HAL_UART_AbortTransmit(&huart3);
        uart_tx_task = NULL;
        return false;
    }

    /* DBT-005: wait for the shift register to empty, not just the DMA. */
    uint32_t start = HAL_GetTick();
    while((USART3->ISR & USART_ISR_TC) == 0U)
    {
        if((HAL_GetTick() - start) > 10U)
        {
            break;
        }
    }
    USART3->ICR = USART_ICR_TCCF;

    uart_tx_task = NULL;
    return true;
}

/**
 * @brief Count one ring wrap and wake the Comms task (ISR).
 * @param[in] huart Completed handle; non-USART3 ignored.
 * @note  The wrap is counted before the hint so the woken task sees it.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
        uart_rx_wraps++;
        comms_post_rx_from_isr();
    }
}

/**
 * @brief Wake the Comms task at the half-ring mark (ISR).
 * @param[in] huart Handle; non-USART3 ignored.
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
        comms_post_rx_from_isr();
    }
}

/**
 * @brief Notify the waiting sender that TX DMA finished (ISR).
 * @param[in] huart Handle; ignored unless USART3 with a registered task.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART3 || uart_tx_task == NULL)
    {
        return;
    }

    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(uart_tx_task, &woken);
    portYIELD_FROM_ISR(woken);
}

/**
 * @brief Wake the Comms task on a DMA transfer error (ISR).
 * @details Recovery is done by @ref app_uart3_rx_restart.
 * @param[in] huart Handle; non-USART3 ignored.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
        comms_post_rx_from_isr();
    }
}

/**
 * @brief USART3 IRQ: clear RX errors and IDLE via @c ICR, delegate TC to the HAL.
 * @details The HAL handler runs only for TC with TCIE set, after the error
 *          flags are cleared, so it ends the TX without taking its error path.
 *          At most one wake-up hint per interrupt. Priority 6 (DRT-003).
 * @warning @c ICR writes must stay plain stores, never read-modify-write.
 */
void USART3_IRQHandler(void)
{
    uint32_t sr   = USART3->ISR;
    bool     wake = false;

    /* Not via the HAL, which aborts circular RX on overrun; not via RDR, which
     * would steal a byte from the DMA (RM0410 30.6.9). */
    if((sr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) != 0U)
    {
        USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
        wake        = true;
    }

    /* Only path that ends a DMA transmit: the HAL clears TCIE, readies the
     * handle and calls HAL_UART_TxCpltCallback(). */
    if(((sr & USART_ISR_TC) != 0U) && ((USART3->CR1 & USART_CR1_TCIE) != 0U))
    {
        HAL_UART_IRQHandler(&huart3);
    }

    /* Re-read: IDLE may have been raised meanwhile. */
    if((USART3->ISR & USART_ISR_IDLE) != 0U)
    {
        USART3->ICR = USART_ICR_IDLECF;
        wake        = true;
    }
    if(wake)
    {
        comms_post_rx_from_isr();
    }
}

/** DMA1 Stream1 (RX) IRQ; the HAL dispatches the half/full/error callbacks. */
void DMA1_Stream1_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart3_rx); }

/** DMA1 Stream3 (TX) IRQ; the sender is woken from the USART3 TC path. */
void DMA1_Stream3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart3_tx); }
