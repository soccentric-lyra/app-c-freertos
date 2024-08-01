/**
 * @file    app_log.c
 * @brief   Non-blocking debug text output on USART6 (PG14, Arduino D1).
 * @details A 1 KB ring is drained by USART6_IRQHandler() at the lowest NVIC
 *          priority; the handler makes no FreeRTOS call (DLOG-002). Writers
 *          mask interrupts only to copy one line in. Full ring: the line is
 *          dropped and counted (DLOG-003); long line: silently truncated.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-06
 * @target  STM32F767ZIT6
 * @srs     DLOG-001..DLOG-004
 * @ingroup app_log
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "stm32f7xx_hal.h"
#include "services/app_log.h"

/**
 * @brief USART6 baud rate in bit/s; 8N1 comes from the reset CR1/CR2 values.
 * @note Shared with BOOT (DLOG-001).
 */
#define LOG_BAUD 115200U
/**
 * @brief Transmit ring size in bytes.
 * @note Must be a power of two; indices wrap with `& (LOG_RING_SIZE - 1U)`.
 */
#define LOG_RING_SIZE 1024U
/** @brief Stack line buffer size in bytes, including the trailing CR LF. */
#define LOG_LINE_MAX 128U
/** @brief Tag between tick and level in every line (DLOG-001). */
#define LOG_TAG "C-FRT"
/**
 * @brief NVIC priority of USART6_IRQn: the lowest urgency.
 * @note Below configMAX_SYSCALL_INTERRUPT_PRIORITY, so kernel critical
 *       sections mask it; the handler calls no FreeRTOS API.
 */
#define LOG_IRQ_PRIORITY 15U

/** @brief USART6 IRQ handler; overrides the weak vector in the start-up file. */
void USART6_IRQHandler(void);

/** @brief Transmit ring; bytes in `[log_tail, log_head)` are queued. */
static char log_ring[LOG_RING_SIZE];
/**
 * @brief Free-running count of bytes queued.
 * @details `log_head - log_tail` is the queued byte count, valid across wrap.
 *          Written only by log_push() with interrupts masked.
 */
static volatile uint32_t log_head;
/**
 * @brief Free-running count of bytes written to TDR.
 * @details Written only by USART6_IRQHandler() and app_log_flush(), both with
 *          no concurrent consumer.
 */
static volatile uint32_t log_tail;
/** @brief Lines dropped because the ring was full (DLOG-003). */
static volatile uint32_t log_dropped;
/** @brief Set once app_log_init() has configured USART6; never cleared. */
static bool log_ready;

/**
 * @brief Configure PG14 as USART6_TX at 115200 baud and enable its IRQ.
 * @details CR1 is cleared before BRR is written (UE must be 0). TXEIE stays
 *          clear until the first line is queued. @ref log_ready is set last so
 *          a concurrent app_log() sees either nothing or a ready port.
 * @pre  Thread context, after HAL_Init() and the clock tree setup.
 * @warning Call once; a second call clears TXEIE and stalls queued bytes.
 */
void app_log_init(void)
{
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();

    GPIO_InitTypeDef tx = {.Pin = GPIO_PIN_14, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_MEDIUM, .Alternate = GPIO_AF8_USART6};
    HAL_GPIO_Init(GPIOG, &tx);

    USART6->CR1 = 0U;
    USART6->BRR = (HAL_RCC_GetPCLK2Freq() + (LOG_BAUD / 2U)) / LOG_BAUD;
    USART6->CR1 = USART_CR1_TE | USART_CR1_UE;

    HAL_NVIC_SetPriority(USART6_IRQn, LOG_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    log_ready = true;
}

/**
 * @brief Copy one line into the ring, or drop and count it (DLOG-003).
 * @details Runs with interrupts masked, which also blocks PendSV, so the copy
 *          is atomic. A line that exactly fills the ring is accepted. On
 *          success TXEIE is set to start the drain.
 *
 * @param[in] text Line bytes, not NUL-terminated.
 * @param[in] len  Length in bytes, 1..@ref LOG_LINE_MAX.
 *
 * @warning NMI and HardFault are not masked by PRIMASK; logging from them can
 *          corrupt a copy in progress.
 */
static void log_push(const char *text, uint32_t len)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if(len > LOG_RING_SIZE - (log_head - log_tail))
    {
        log_dropped++;
    }
    else
    {
        for(uint32_t i = 0U; i < len; i++)
        {
            log_ring[(log_head + i) & (LOG_RING_SIZE - 1U)] = text[i];
        }
        log_head += len;
        USART6->CR1 |= USART_CR1_TXEIE;
    }
    __set_PRIMASK(primask);
}

/** @brief Bounded output sink for the formatter; lives on app_log()'s stack. */
typedef struct
{
    char  *buf; /**< Destination buffer; never NUL-terminated. */
    size_t len; /**< Characters written; next write index. */
    size_t cap; /**< Capacity in bytes; excess characters are discarded. */
} log_out_t;

/**
 * @brief Append one character if it fits; otherwise discard it.
 * @param[in,out] out Sink to append to.
 * @param[in]     c   Character to append.
 */
static void out_char(log_out_t *out, char c)
{
    if(out->len < out->cap)
    {
        out->buf[out->len] = c;
        out->len++;
    }
}

/**
 * @brief Append an unsigned magnitude in the given radix, padded to a width.
 *
 * @param[in,out] out      Sink.
 * @param[in]     value    Magnitude to print.
 * @param[in]     base     Radix, 2..16 (callers use 10 or 16).
 * @param[in]     negative Prefix a '-'.
 * @param[in]     width    Minimum field width; 0 for none.
 * @param[in]     pad      Fill character, ' ' or '0'.
 *
 * @note Padding precedes the sign, so `%06d` prints -42 as `000-42`, unlike
 *       printf. No current caller zero-pads a negative number.
 */
static void out_number(log_out_t *out, uint32_t value, uint32_t base, bool negative, uint32_t width, char pad)
{
    char     digits[11];
    uint32_t n = 0U;
    do
    {
        uint32_t d = value % base;
        digits[n]  = (char)(d < 10U ? '0' + d : 'a' + d - 10U);
        n++;
        value /= base;
    } while(value != 0U);
    if(negative)
    {
        digits[n] = '-';
        n++;
    }
    for(uint32_t i = n; i < width; i++)
    {
        out_char(out, pad);
    }
    while(n > 0U)
    {
        n--;
        out_char(out, digits[n]);
    }
}

/**
 * @brief Expand the supported printf subset into the sink.
 * @details Accepts `%[0][width][.prec][l]` then one of `c s d i u x %`.
 *          Precision applies to `%s`; width and pad to numbers. An unsupported
 *          conversion is echoed and ends formatting, so no later argument is
 *          read.
 *
 * @param[in,out] out  Sink.
 * @param[in]     fmt  NUL-terminated format; must not be NULL.
 * @param[in]     args Arguments; the caller owns va_start/va_end.
 */
static void log_format(log_out_t *out, const char *fmt, va_list args)
{
    for(; *fmt != '\0'; fmt++)
    {
        if(*fmt != '%')
        {
            out_char(out, *fmt);
            continue;
        }
        fmt++;
        char pad = ' ';
        if(*fmt == '0')
        {
            pad = '0';
            fmt++;
        }
        uint32_t width = 0U;
        while(*fmt >= '0' && *fmt <= '9')
        {
            width = (width * 10U) + (uint32_t)(*fmt - '0');
            fmt++;
        }
        uint32_t precision = UINT32_MAX;
        if(*fmt == '.')
        {
            precision = 0U;
            fmt++;
            while(*fmt >= '0' && *fmt <= '9')
            {
                precision = (precision * 10U) + (uint32_t)(*fmt - '0');
                fmt++;
            }
        }
        if(*fmt == 'l')
        {
            fmt++; /* long and int are both 32 bits here */
        }
        switch(*fmt)
        {
        case 'c': out_char(out, (char)va_arg(args, int)); break;
        case 's':
        {
            const char *text = va_arg(args, const char *);
            for(uint32_t i = 0U; text != NULL && text[i] != '\0' && i < precision; i++)
            {
                out_char(out, text[i]);
            }
            break;
        }
        case 'd':
        case 'i':
        {
            int32_t value = va_arg(args, int32_t);
            out_number(out, value < 0 ? (uint32_t)(-(value + 1)) + 1U : (uint32_t)value, 10U, value < 0, width, pad);
            break;
        }
        case 'u': out_number(out, va_arg(args, uint32_t), 10U, false, width, pad); break;
        case 'x': out_number(out, va_arg(args, uint32_t), 16U, false, width, pad); break;
        case '%': out_char(out, '%'); break;
        default: /* unsupported: echo it, then stop before later args */
            out_char(out, '%');
            if(*fmt == '\0')
            {
                return;
            }
            out_char(out, *fmt);
            return;
        }
    }
}

/**
 * @brief Variadic wrapper around log_format().
 * @param[in,out] out Sink.
 * @param[in]     fmt NUL-terminated format; must not be NULL.
 * @param[in]     ... Arguments for @p fmt.
 * @note No format(printf) attribute: only a subset is supported.
 */
static void log_printf(log_out_t *out, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_format(out, fmt, args);
    va_end(args);
}

/**
 * @brief Format one debug line and queue it on USART6.
 * @details Prefix and message are capped at 126 chars so CR LF always fit.
 *
 * @param[in] level Label character, 'E', 'W', 'I' or 'D' by convention.
 * @param[in] fmt   printf-style format; must not be NULL.
 * @param[in] ...   Arguments for @p fmt.
 *
 * @note Task, ISR and pre-scheduler safe. Uses @ref LOG_LINE_MAX bytes of the
 *       caller's stack.
 */
void app_log(char level, const char *fmt, ...)
{
    if(!log_ready)
    {
        return;
    }
    char line[LOG_LINE_MAX];
    /* Leave room for CR LF at the end of a truncated line. */
    log_out_t out = {line, 0U, sizeof(line) - 2U};
    log_printf(&out, "[%7lu] " LOG_TAG " %c ", HAL_GetTick(), level);
    va_list args;
    va_start(args, fmt);
    log_format(&out, fmt, args);
    va_end(args);
    out.cap = sizeof(line);
    out_char(&out, '\r');
    out_char(&out, '\n');
    log_push(line, (uint32_t)out.len);
}

/**
 * @brief Busy-wait until the ring is empty and TC is set, or the budget ends.
 * @details Feeds TDR by polling with interrupts masked, then waits for TC so
 *          the last stop bit is out before a reset.
 *
 * @param[in] timeout_ms Approximate wait limit in ms; 0 only clears TXEIE.
 *
 * @post TXEIE is clear and PRIMASK is restored.
 * @warning No interrupt, RTOS tick or watchdog feed runs during the wait.
 */
void app_log_flush(uint32_t timeout_ms)
{
    if(!log_ready)
    {
        return;
    }
    /* Drain by polling with interrupts off: this also runs from the fault
     * path, where neither the TXE interrupt nor the HAL tick advances.
     * The bound is a loop count for the same reason (about 4 cycles each
     * at 216 MHz). */
    uint32_t loops   = timeout_ms > (UINT32_MAX / 50000U) ? UINT32_MAX : timeout_ms * 50000U;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    while((log_head != log_tail || (USART6->ISR & USART_ISR_TC) == 0U) && loops-- > 0U)
    {
        if(log_head != log_tail && (USART6->ISR & USART_ISR_TXE) != 0U)
        {
            USART6->TDR = (uint8_t)log_ring[log_tail & (LOG_RING_SIZE - 1U)];
            log_tail++;
        }
    }
    USART6->CR1 &= ~USART_CR1_TXEIE;
    __set_PRIMASK(primask);
}

/**
 * @brief Return the number of lines dropped because the ring was full.
 * @return Lines dropped since reset (DLOG-003).
 */
uint32_t app_log_dropped(void) { return log_dropped; }

/**
 * @brief Send one queued byte, or clear TXEIE when the ring is empty.
 * @details ISR context, lowest priority. Writing TDR clears TXE. Needs no
 *          lock: log_push() updates @ref log_head with interrupts masked.
 */
void USART6_IRQHandler(void)
{
    if((USART6->ISR & USART_ISR_TXE) == 0U)
    {
        return;
    }
    if(log_tail != log_head)
    {
        USART6->TDR = (uint8_t)log_ring[log_tail & (LOG_RING_SIZE - 1U)];
        log_tail++;
    }
    else
    {
        USART6->CR1 &= ~USART_CR1_TXEIE;
    }
}
