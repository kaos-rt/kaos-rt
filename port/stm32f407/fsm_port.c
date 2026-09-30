/* KAOS-RT: TIM2 time source shared by logical time and platform uptime. */

#include "fsm.h"

#define FSM_PORT_COUNTER_CYCLE_TICKS ((UINT32_MAX / FSM_PORT_USEC_PER_LOGICAL_TICK) + (((UINT32_MAX % FSM_PORT_USEC_PER_LOGICAL_TICK) + UINT32_C(1)) / FSM_PORT_USEC_PER_LOGICAL_TICK))
#define FSM_PORT_COUNTER_CYCLE_REMAINDER (((UINT32_MAX % FSM_PORT_USEC_PER_LOGICAL_TICK) + UINT32_C(1)) % FSM_PORT_USEC_PER_LOGICAL_TICK)

static volatile uint32_t fsm_port_tim2_overflow_count;
static volatile uint32_t fsm_port_logical_epoch;
static volatile uint32_t fsm_port_logical_remainder;

static void fsm_port_advance_logical_epoch(uint32_t * const epoch, uint32_t * const remainder)
{
    /* Preserve the fractional logical tick accumulated over one 32-bit cycle. */
    *epoch += FSM_PORT_COUNTER_CYCLE_TICKS;
    *remainder += FSM_PORT_COUNTER_CYCLE_REMAINDER;

    if (*remainder >= FSM_PORT_USEC_PER_LOGICAL_TICK)
    {
        *remainder -= FSM_PORT_USEC_PER_LOGICAL_TICK;
        *epoch += UINT32_C(1);
    }
}

static uint64_t fsm_port_tim2_now(void)
{
    fsm_irq_state_t irq_state;
    uint32_t overflow_count;
    uint32_t counter;
    uint32_t overflow_pending;

    /* Foreground only: TIM2 IRQ cannot change either half of this snapshot. */
    irq_state = fsm_port_irq_save();
    overflow_count = fsm_port_tim2_overflow_count;
    counter = TIM2->CNT;
    overflow_pending = TIM2->SR & FSM_PORT_TIM_SR_UIF;

    if (overflow_pending != UINT32_C(0))
    {
        overflow_count += UINT32_C(1);
        counter = TIM2->CNT;
    }

    fsm_port_irq_restore(irq_state);

    return ((uint64_t)overflow_count << 32) | (uint64_t)counter;
}

void port_uptime_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->PSC = (FSM_TIMER_CLOCK_HZ / FSM_PORT_TIME_SOURCE_HZ) - UINT32_C(1);
    TIM2->ARR = UINT32_MAX;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CNT = UINT32_C(0);
    TIM2->SR = UINT32_C(0);
    TIM2->DIER |= FSM_PORT_TIM_DIER_UIE;

    fsm_port_tim2_overflow_count = UINT32_C(0);
    fsm_port_logical_epoch = UINT32_C(0);
    fsm_port_logical_remainder = UINT32_C(0);
    FSM_PORT_NVIC_ISER0 = FSM_PORT_TIM2_IRQ_MASK;
    TIM2->CR1 |= TIM_CR1_CEN;
}

uint64_t port_uptime_usec(void)
{
    return fsm_port_tim2_now();
}

fsm_time_t fsm_port_time_now(void)
{
    fsm_irq_state_t irq_state;
    uint32_t epoch;
    uint32_t remainder;
    uint32_t counter;
    uint32_t overflow_pending;

    irq_state = fsm_port_irq_save();
    epoch = fsm_port_logical_epoch;
    remainder = fsm_port_logical_remainder;
    counter = TIM2->CNT;
    overflow_pending = TIM2->SR & FSM_PORT_TIM_SR_UIF;

    if (overflow_pending != UINT32_C(0))
    {
        fsm_port_advance_logical_epoch(&epoch, &remainder);
        counter = TIM2->CNT;
    }

    fsm_port_irq_restore(irq_state);

    epoch += counter / FSM_PORT_USEC_PER_LOGICAL_TICK;
    if ((counter % FSM_PORT_USEC_PER_LOGICAL_TICK) >= (FSM_PORT_USEC_PER_LOGICAL_TICK - remainder))
    {
        epoch += UINT32_C(1);
    }

    return (fsm_time_t){ .ticks = epoch };
}

void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & FSM_PORT_TIM_SR_UIF) != UINT32_C(0))
    {
        uint32_t epoch = fsm_port_logical_epoch;
        uint32_t remainder = fsm_port_logical_remainder;

        TIM2->SR = UINT32_C(0);
        fsm_port_tim2_overflow_count += UINT32_C(1);
        fsm_port_advance_logical_epoch(&epoch, &remainder);
        fsm_port_logical_epoch = epoch;
        fsm_port_logical_remainder = remainder;
    }
}
