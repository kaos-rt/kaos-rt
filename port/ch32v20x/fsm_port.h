/*
 * KAOS-RT: порт для CH32V20x.
 *
 * Источник логических тиков: 64-битный SysTick QingKe V4.
 * Критическая секция: RISC-V mstatus.
 * Платформенные зависимости: ch32v20x.h и GCC-совместимый inline assembly.
 */

#ifndef FSM_PORT_H_
#define FSM_PORT_H_

#include <stdint.h>

#include "ch32v20x.h"
#include "fsm_config.h"

/*
 * The application board configuration owns the clock tree. Define this macro
 * in its build settings before including fsm.h; do not add a port default.
 */
#ifndef FSM_SYSTICK_CLOCK_HZ
#error "Define FSM_SYSTICK_CLOCK_HZ in the board build configuration as the active SysTick clock in Hz"
#endif

typedef uint32_t fsm_time_t;
typedef uint32_t fsm_irq_state_t;
typedef uint64_t fsm_profile_tick_t;

#define FSM_PORT_WEAK __attribute__((weak))
#define FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK  0

#define FSM_PORT_STK_CTLR_STE    (UINT32_C(1) << 0)
#define FSM_PORT_STK_CTLR_STCLK  (UINT32_C(1) << 2)
#define FSM_PORT_STK_CTLR_STRE   (UINT32_C(1) << 3)
#define FSM_PORT_STK_CTLR_INIT   (UINT32_C(1) << 5)
#define FSM_PORT_IRQ_ENABLE_MASK UINT32_C(0x88)

typedef struct
{
    volatile uint32_t ctlr;
    volatile uint32_t sr;
    volatile uint32_t cntl;
    volatile uint32_t cnth;
    volatile uint32_t cmpl;
    volatile uint32_t cmph;
} fsm_port_systick_t;

#define FSM_PORT_SYSTICK ((fsm_port_systick_t *)UINT32_C(0xE000F000))

_Static_assert(
    (FSM_SYSTICK_CLOCK_HZ % FSM_TICK_FREQ_HZ) == UINT32_C(0),
    "SysTick clock must be an integer multiple of FSM_TICK_FREQ_HZ");

_Static_assert(
    (FSM_SYSTICK_CLOCK_HZ % UINT32_C(1000000)) == UINT32_C(0),
    "SysTick clock must be an integer multiple of 1 MHz");

#define PORT_UPTIME_TICKS_PER_USEC  (FSM_SYSTICK_CLOCK_HZ / UINT32_C(1000000))

static inline uint64_t fsm_port_systick_now(void)
{
    uint32_t high_before = FSM_PORT_SYSTICK->cnth;
    uint32_t low = FSM_PORT_SYSTICK->cntl;
    uint32_t high_after = FSM_PORT_SYSTICK->cnth;

    if (high_before != high_after)
    {
        low = FSM_PORT_SYSTICK->cntl;
    }

    return ((uint64_t)high_after << 32) | (uint64_t)low;
}

static inline void fsm_port_time_init(void)
{
    FSM_PORT_SYSTICK->sr = UINT32_C(0);
    FSM_PORT_SYSTICK->cntl = UINT32_C(0);
    FSM_PORT_SYSTICK->cnth = UINT32_C(0);
    FSM_PORT_SYSTICK->cmpl = UINT32_MAX;
    FSM_PORT_SYSTICK->cmph = UINT32_MAX;
    FSM_PORT_SYSTICK->ctlr = FSM_PORT_STK_CTLR_STE |
                             FSM_PORT_STK_CTLR_STCLK |
                             FSM_PORT_STK_CTLR_STRE |
                             FSM_PORT_STK_CTLR_INIT;
}

static inline fsm_time_t fsm_port_time_now(void)
{
    return (fsm_time_t)(fsm_port_systick_now() / (uint64_t)(FSM_SYSTICK_CLOCK_HZ / FSM_TICK_FREQ_HZ));
}

static inline uint64_t port_uptime_usec(void)
{
    return fsm_port_systick_now() / (uint64_t)PORT_UPTIME_TICKS_PER_USEC;
}

static inline void fsm_port_profile_init(void)
{
}

static inline fsm_profile_tick_t fsm_port_profile_now(void)
{
    return fsm_port_systick_now();
}

static inline fsm_irq_state_t fsm_port_irq_save(void)
{
    fsm_irq_state_t state;

    __asm volatile (
        "csrr %0, mstatus \n"
        "csrc 0x800, %1 \n"
        : "=r" (state)
        : "r" (FSM_PORT_IRQ_ENABLE_MASK)
        : "memory");

    return state;
}

static inline void fsm_port_irq_restore(fsm_irq_state_t state)
{
    __asm volatile (
        "csrw mstatus, %0"
        :
        : "r" (state)
        : "memory");
}

#endif /* FSM_PORT_H_ */
