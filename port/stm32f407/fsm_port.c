/* KAOS-RT: 64-bit uptime extension for the STM32F407 TIM2 time source. */

#include "fsm_port.h"

static volatile uint32_t fsm_port_tim2_overflow_count;

void port_uptime_init(void)
{
    fsm_port_tim2_overflow_count = UINT32_C(0);
    TIM2->SR = UINT32_C(0);
    TIM2->DIER |= FSM_PORT_TIM_DIER_UIE;
    FSM_PORT_NVIC_ISER0 = FSM_PORT_TIM2_IRQ_MASK;
}

uint64_t port_uptime_usec(void)
{
    uint32_t overflow_before;
    uint32_t overflow_after;
    uint32_t counter;
    uint32_t overflow_pending;

    overflow_before = fsm_port_tim2_overflow_count;
    counter = TIM2->CNT;
    overflow_after = fsm_port_tim2_overflow_count;

    if (overflow_before != overflow_after)
    {
        overflow_before = overflow_after;
        counter = TIM2->CNT;
    }
    else
    {
        overflow_pending = TIM2->SR & FSM_PORT_TIM_SR_UIF;

        if (overflow_pending != UINT32_C(0))
        {
            overflow_before += UINT32_C(1);
            counter = TIM2->CNT;
        }
    }

    uint64_t ticks = ((uint64_t)overflow_before << 32) | (uint64_t)counter;

    return ticks * (uint64_t)PORT_UPTIME_USEC_PER_TICK;
}

void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & FSM_PORT_TIM_SR_UIF) != UINT32_C(0))
    {
        TIM2->SR = UINT32_C(0);
        fsm_port_tim2_overflow_count += UINT32_C(1);
    }
}
