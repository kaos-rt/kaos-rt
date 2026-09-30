/*
 * KAOS-RT: порт для STM32F407.
 *
 * Общий источник логических тиков и uptime: 32-битный TIM2 с частотой 1 МГц.
 * Критическая секция: Cortex-M PRIMASK.
 * Платформенные зависимости: stm32f407_min.h и GCC-совместимый inline assembly.
 */

#ifndef FSM_PORT_H_
#define FSM_PORT_H_

#include <stdint.h>

#include "stm32f407_min.h"
#include "fsm_config.h"

/*
 * The board configuration owns the TIM2 clock tree; do not add a port default.
 */
#ifndef FSM_TIMER_CLOCK_HZ
#error "Define FSM_TIMER_CLOCK_HZ as the actual APB1 timer clock for TIM2"
#endif

/* Core-visible port types. */
typedef uint32_t fsm_irq_state_t;
typedef uint32_t fsm_profile_tick_t;

/* Cortex-M debug and DWT registers used only by the optional profiler. */
#define FSM_PORT_DEMCR            (*(volatile uint32_t *)UINT32_C(0xE000EDFC))
#define FSM_PORT_DWT_CTRL         (*(volatile uint32_t *)UINT32_C(0xE0001000))
#define FSM_PORT_DWT_CYCCNT       (*(volatile uint32_t *)UINT32_C(0xE0001004))
#define FSM_PORT_DEMCR_TRCENA     (UINT32_C(1) << 24)
#define FSM_PORT_DWT_CYCCNTENA    UINT32_C(1)
#define FSM_PORT_TIM_DIER_UIE     UINT32_C(1)
#define FSM_PORT_TIM_SR_UIF       UINT32_C(1)
#define FSM_PORT_NVIC_ISER0       (*(volatile uint32_t *)UINT32_C(0xE000E100))
#define FSM_PORT_TIM2_IRQ_MASK    (UINT32_C(1) << 28)
#define FSM_PORT_TIME_SOURCE_HZ   UINT32_C(1000000)
#define FSM_PORT_USEC_PER_LOGICAL_TICK (FSM_PORT_TIME_SOURCE_HZ / FSM_TICK_FREQ_HZ)

/* GNU-compatible compiler extension; record as a MISRA language deviation. */
#define FSM_PORT_WEAK __attribute__((weak))

/* Cortex-M IRQ is the only concurrent writer; a relaxed empty poll is safe here. */
#define FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK  0

/* TIM2 uses a 16-bit prescaler and remains a free-running 32-bit counter. */
_Static_assert(
    (FSM_TIMER_CLOCK_HZ % FSM_PORT_TIME_SOURCE_HZ) == UINT32_C(0),
    "TIM2 clock must be an integer multiple of 1 MHz");

_Static_assert(
    (FSM_TIMER_CLOCK_HZ / FSM_PORT_TIME_SOURCE_HZ) >= UINT32_C(1),
    "TIM2 clock is lower than 1 MHz");

_Static_assert(
    (FSM_TIMER_CLOCK_HZ / FSM_PORT_TIME_SOURCE_HZ) <= UINT32_C(65536),
    "TIM2 prescaler does not fit into 16 bits");

_Static_assert(
    (FSM_PORT_TIME_SOURCE_HZ % FSM_TICK_FREQ_HZ) == UINT32_C(0),
    "1 MHz time source must be an integer multiple of FSM_TICK_FREQ_HZ");

_Static_assert(
    FSM_TICK_FREQ_HZ <= FSM_PORT_TIME_SOURCE_HZ,
    "FSM_TICK_FREQ_HZ must not exceed 1 MHz");

void port_uptime_init(void);
uint64_t port_uptime_usec(void);

static inline void fsm_port_time_init(void)
{
    port_uptime_init();
}

fsm_time_t fsm_port_time_now(void);

/* DWT CYCCNT measures core-clock cycles and is used only by diagnostics. */
static inline void fsm_port_profile_init(void)
{
    FSM_PORT_DEMCR |= FSM_PORT_DEMCR_TRCENA;
    FSM_PORT_DWT_CYCCNT = UINT32_C(0);
    FSM_PORT_DWT_CTRL |= FSM_PORT_DWT_CYCCNTENA;
}

static inline fsm_profile_tick_t fsm_port_profile_now(void)
{
    return FSM_PORT_DWT_CYCCNT;
}

static inline fsm_irq_state_t fsm_port_irq_save(void)
{
    fsm_irq_state_t state;

    __asm volatile (
        "mrs   %0, primask \n"
        "cpsid i          \n"
        : "=r" (state)
        :
        : "memory"
    );

    return state;
}

/* Restore exactly the PRIMASK state observed by fsm_port_irq_save(). */
static inline void fsm_port_irq_restore(fsm_irq_state_t state)
{
    __asm volatile (
        "msr primask, %0"
        :
        : "r" (state)
        : "memory"
    );
}

#endif /* FSM_PORT_H_ */
