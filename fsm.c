/*
 * Copyright 2026 Morozov Oleg and Chirkov Boris
 * SPDX-License-Identifier: Apache-2.0
 *
 * KAOS-RT core implementation.
 *
 * Language baseline: ISO C17.
 */

#include <stddef.h>
#include <stdint.h>

#include "fsm.h"

typedef struct
{
    /* Application state and its coalescing event bitmap. */
    uint32_t state;
    volatile uint32_t events_set;
    fsm_t callback;
} fsm_ev_t;

typedef struct
{
    /* A zero delta denotes a free timer slot; active timers always have delta > 0. */
    fsm_time_t delta;
    fsm_time_t period;
    uint32_t events;
    fsm_id_t fsm;
    uint64_t generation;
} fsm_timer_t;

/* All mutable kernel state remains private to this translation unit. */
static fsm_ev_t fsm_table[FSM_LAST];
static fsm_timer_t fsm_time_table[FSM_TIMER_COUNT];
static fsm_time_t fsm_last_time;
static uint32_t fsm_timer_next_slot;

#if (FSM_PROFILE_ENABLE != 0)
static fsm_profile_fsm_t fsm_profile_fsm[FSM_LAST];
static fsm_profile_system_t fsm_profile_system;
#endif

/* Private validation and default behavior. */
static uint32_t valid_fsm(fsm_id_t fsm)
{
    return (uint32_t)((uint32_t)fsm < (uint32_t)FSM_LAST);
}

static uint32_t valid_event(fsm_event_t event)
{
    return (uint32_t)((uint32_t)event < (uint32_t)FSM_EVENT_COUNT);
}

/* A removed slot remains dispatchable, but deliberately has no side effects. */
static uint32_t null_fsm(uint32_t state, uint32_t events)
{
    (void)events;
    return state;
}

/* Optional application service hook. A strong application definition overrides this no-op. */
FSM_PORT_WEAK void fsm_loop_service(void)
{
}

/* Kernel initialization and time accounting. */
void start_fsm(void)
{
    uint32_t i;

    for (i = UINT32_C(0); i < (uint32_t)FSM_TIMER_COUNT; ++i)
    {
        fsm_time_table[i].delta = UINT32_C(0);
        fsm_time_table[i].period = UINT32_C(0);
        fsm_time_table[i].events = UINT32_C(0);
        fsm_time_table[i].fsm = FSM_INVALID_ID;
        fsm_time_table[i].generation = UINT64_C(0);
    }

    for (i = UINT32_C(0); i < (uint32_t)FSM_LAST; ++i)
    {
        fsm_table[i].callback = null_fsm;
        fsm_table[i].events_set = UINT32_C(0);
        fsm_table[i].state = UINT32_C(0);

#if (FSM_PROFILE_ENABLE != 0)
        fsm_profile_fsm[i] = (fsm_profile_fsm_t){0};
#endif
    }

#if (FSM_PROFILE_ENABLE != 0)
    fsm_profile_system = (fsm_profile_system_t){0};
#endif

    fsm_port_time_init();
    fsm_last_time = fsm_port_time_now();
    fsm_timer_next_slot = UINT32_C(0);

#if (FSM_PROFILE_ENABLE != 0)
    fsm_port_profile_init();
#endif
}

static fsm_time_t elapsed_time(void)
{
    /* Unsigned subtraction preserves elapsed time across one counter wrap. */
    fsm_time_t now = fsm_port_time_now();
    fsm_time_t elapsed = now - fsm_last_time;

    fsm_last_time = now;

    return elapsed;
}

/* FSM lifecycle and diagnostic state access. */
fsm_t set_fsm(fsm_id_t fsm, fsm_t callback)
{
    if ((valid_fsm(fsm) == UINT32_C(0)) || (callback == NULL))
    {
        return NULL;
    }

    fsm_t previous = fsm_table[(uint32_t)fsm].callback;

    /* Timers belong to the old callback and must not reach the replacement. */
    remove_timers(fsm);

    fsm_irq_state_t irq_state = fsm_port_irq_save();
    fsm_table[(uint32_t)fsm].callback = callback;
    fsm_table[(uint32_t)fsm].state = UINT32_C(0);
    fsm_table[(uint32_t)fsm].events_set = UINT32_C(0);
    fsm_port_irq_restore(irq_state);

    return previous;
}

fsm_t remove_fsm(fsm_id_t fsm)
{
    if (valid_fsm(fsm) == UINT32_C(0))
    {
        return NULL;
    }

    fsm_t previous = fsm_table[(uint32_t)fsm].callback;

    remove_timers(fsm);

    fsm_irq_state_t irq_state = fsm_port_irq_save();
    fsm_table[(uint32_t)fsm].callback = null_fsm;
    fsm_table[(uint32_t)fsm].state = UINT32_C(0);
    fsm_table[(uint32_t)fsm].events_set = UINT32_C(0);
    fsm_port_irq_restore(irq_state);

    return previous;
}

fsm_status_t get_fsm_state(fsm_id_t fsm, uint32_t *state)
{
    if (valid_fsm(fsm) == UINT32_C(0))
    {
        return FSM_STATUS_INVALID_FSM;
    }

    if (state == NULL)
    {
        return FSM_STATUS_NULL_ARGUMENT;
    }

    *state = fsm_table[(uint32_t)fsm].state;

    return FSM_STATUS_OK;
}

#if (FSM_PROFILE_ENABLE != 0)
fsm_status_t fsm_profile_get_fsm(fsm_id_t fsm, fsm_profile_fsm_t *profile)
{
    fsm_irq_state_t irq_state;

    if (valid_fsm(fsm) == UINT32_C(0))
    {
        return FSM_STATUS_INVALID_FSM;
    }

    if (profile == NULL)
    {
        return FSM_STATUS_NULL_ARGUMENT;
    }

    irq_state = fsm_port_irq_save();
    *profile = fsm_profile_fsm[(uint32_t)fsm];
    fsm_port_irq_restore(irq_state);

    return FSM_STATUS_OK;
}

fsm_status_t fsm_profile_get_system(fsm_profile_system_t *profile)
{
    fsm_irq_state_t irq_state;

    if (profile == NULL)
    {
        return FSM_STATUS_NULL_ARGUMENT;
    }

    irq_state = fsm_port_irq_save();
    *profile = fsm_profile_system;
    fsm_port_irq_restore(irq_state);

    return FSM_STATUS_OK;
}
#endif

/* Event API. append_event() is the only public API permitted from an ISR. */
static void post_events(fsm_id_t fsm, uint32_t events)
{
    /* Keep the bitmap update atomic with dispatcher take-and-clear. */
    fsm_irq_state_t irq_state = fsm_port_irq_save();

#if (FSM_PROFILE_ENABLE != 0)
    if ((fsm_table[(uint32_t)fsm].events_set & events) != UINT32_C(0))
    {
        fsm_profile_system.coalesced_event_posts += UINT32_C(1);
    }

    fsm_profile_system.event_posts += UINT32_C(1);
#endif

    fsm_table[(uint32_t)fsm].events_set |= events;

    fsm_port_irq_restore(irq_state);
}

fsm_status_t append_event(fsm_id_t fsm, fsm_event_t event)
{
    if (valid_fsm(fsm) == UINT32_C(0))
    {
        return FSM_STATUS_INVALID_FSM;
    }

    if (valid_event(event) == UINT32_C(0))
    {
        return FSM_STATUS_INVALID_EVENT;
    }

    post_events(fsm, to_events_set(event));

    return FSM_STATUS_OK;
}

fsm_status_t remove_event(fsm_id_t fsm, fsm_event_t event)
{
    if (valid_fsm(fsm) == UINT32_C(0))
    {
        return FSM_STATUS_INVALID_FSM;
    }

    if (valid_event(event) == UINT32_C(0))
    {
        return FSM_STATUS_INVALID_EVENT;
    }

    {
        uint32_t mask = to_events_set(event);
        fsm_irq_state_t irq_state = fsm_port_irq_save();

        fsm_table[(uint32_t)fsm].events_set &= ~mask;

        fsm_port_irq_restore(irq_state);
    }

    return FSM_STATUS_OK;
}

fsm_status_t remove_events(fsm_id_t fsm)
{
    if (valid_fsm(fsm) == UINT32_C(0))
    {
        return FSM_STATUS_INVALID_FSM;
    }

    {
        fsm_irq_state_t irq_state = fsm_port_irq_save();

        fsm_table[(uint32_t)fsm].events_set = UINT32_C(0);

        fsm_port_irq_restore(irq_state);
    }

    return FSM_STATUS_OK;
}

/* Timer allocation and lifetime. */
static uint64_t next_timer_generation(uint64_t generation)
{
    if (generation >= FSM_TIMER_GENERATION_MAX)
    {
        return UINT64_C(1);
    }

    return generation + UINT64_C(1);
}

static fsm_timer_id_t add_timer_internal(fsm_id_t fsm, fsm_time_t delay, fsm_time_t period, uint32_t events)
{
    fsm_time_t elapsed_since_timer_service;
    uint32_t i;
    uint32_t probe;

    if ((valid_fsm(fsm) == UINT32_C(0)) || (delay == UINT32_C(0)) || (events == FSM_EVENTS_NONE))
    {
        return FSM_TIMER_INVALID_ID;
    }

    /* A timer starts at this call, not at the preceding timer-service phase. */
    elapsed_since_timer_service = fsm_port_time_now() - fsm_last_time;

    if (delay > (UINT32_MAX - elapsed_since_timer_service))
    {
        return FSM_TIMER_INVALID_ID;
    }

    for (probe = UINT32_C(0); probe < (uint32_t)FSM_TIMER_COUNT; ++probe)
    {
        /* Round-robin allocation prevents a permanently preferred free slot. */
        i = fsm_timer_next_slot + probe;

        if (i >= (uint32_t)FSM_TIMER_COUNT)
        {
            i -= (uint32_t)FSM_TIMER_COUNT;
        }

        if (fsm_time_table[i].delta == UINT32_C(0))
        {
            uint64_t generation = next_timer_generation(fsm_time_table[i].generation);

            fsm_time_table[i].delta = delay + elapsed_since_timer_service;
            fsm_time_table[i].period = period;
            fsm_time_table[i].events = events;
            fsm_time_table[i].fsm = fsm;
            fsm_time_table[i].generation = generation;

#if (FSM_PROFILE_ENABLE != 0)
            fsm_profile_system.active_timers += UINT32_C(1);

            if (fsm_profile_system.active_timers > fsm_profile_system.peak_active_timers)
            {
                fsm_profile_system.peak_active_timers = fsm_profile_system.active_timers;
            }
#endif

            fsm_timer_next_slot = i + UINT32_C(1);

            if (fsm_timer_next_slot >= (uint32_t)FSM_TIMER_COUNT)
            {
                fsm_timer_next_slot = UINT32_C(0);
            }

            return fsm_timer_make_id(i, fsm_time_table[i].generation);
        }
    }

#if (FSM_PROFILE_ENABLE != 0)
    fsm_profile_system.timer_allocation_failures += UINT32_C(1);
#endif

    return FSM_TIMER_INVALID_ID;
}

fsm_timer_id_t add_timer(fsm_id_t fsm, fsm_time_t delay, uint32_t events)
{
    return add_timer_internal(fsm, delay, UINT32_C(0), events);
}

fsm_timer_id_t add_periodical_timer(fsm_id_t fsm, fsm_time_t period, uint32_t events)
{
    return add_timer_internal(fsm, period, period, events);
}

static inline void clear_timer_slot(uint32_t slot)
{
    fsm_time_table[slot].delta = UINT32_C(0);
    fsm_time_table[slot].period = UINT32_C(0);
    fsm_time_table[slot].events = UINT32_C(0);
    fsm_time_table[slot].fsm = FSM_INVALID_ID;

#if (FSM_PROFILE_ENABLE != 0)
    fsm_profile_system.active_timers -= UINT32_C(1);
#endif
}

fsm_timer_result_t remove_timer(fsm_timer_id_t timer_id)
{
    uint32_t slot;
    uint64_t generation;

    if (fsm_timer_is_valid(timer_id) == UINT32_C(0))
    {
        return FSM_TIMER_NOT_FOUND;
    }

    slot = fsm_timer_get_slot(timer_id);
    generation = fsm_timer_get_generation(timer_id);

    if ((fsm_time_table[slot].delta != UINT32_C(0)) && (fsm_time_table[slot].generation == generation))
    {
        clear_timer_slot(slot);

        return FSM_TIMER_REMOVED;
    }

    return FSM_TIMER_NOT_FOUND;
}

void remove_timers(fsm_id_t fsm)
{
    uint32_t i;

    if (valid_fsm(fsm) == UINT32_C(0))
    {
        return;
    }

    for (i = UINT32_C(0); i < (uint32_t)FSM_TIMER_COUNT; ++i)
    {
        if ((fsm_time_table[i].delta != UINT32_C(0)) && (fsm_time_table[i].fsm == fsm))
        {
            clear_timer_slot(i);
        }
    }
}

static inline uint32_t take_fsm_events(uint32_t fsm)
{
    fsm_irq_state_t irq_state = fsm_port_irq_save();
    uint32_t events = fsm_table[fsm].events_set;

    fsm_table[fsm].events_set = UINT32_C(0);
    fsm_port_irq_restore(irq_state);

    return events;
}

/* Dispatcher. One pass services the hook, timers, then FSMs in ascending ID order. */
void main_fsm(void)
{
    for (;;)
    {
        uint32_t i;

#if (FSM_PROFILE_ENABLE != 0)
        fsm_profile_tick_t loop_start = fsm_port_profile_now();
#endif

        fsm_loop_service();

#if (FSM_PROFILE_ENABLE != 0)
        fsm_profile_tick_t timer_service_start = fsm_port_profile_now();
#endif

        fsm_time_t elapsed = elapsed_time();

        if (elapsed != UINT32_C(0))
        {
            for (i = UINT32_C(0); i < (uint32_t)FSM_TIMER_COUNT; ++i)
            {
                if (fsm_time_table[i].delta != UINT32_C(0))
                {
                    if (elapsed >= fsm_time_table[i].delta)
                    {
                        fsm_id_t fsm = fsm_time_table[i].fsm;
                        uint32_t events = fsm_time_table[i].events;

                        if (fsm_time_table[i].period != UINT32_C(0))
                        {
                            /* Preserve periodic phase; missed expiries intentionally coalesce. */
                            fsm_time_t overshoot = elapsed - fsm_time_table[i].delta;
                            fsm_time_t phase = overshoot % fsm_time_table[i].period;

#if (FSM_PROFILE_ENABLE != 0)
                            fsm_profile_system.timer_missed_periods += overshoot / fsm_time_table[i].period;
#endif

                            if (phase == UINT32_C(0))
                            {
                                fsm_time_table[i].delta = fsm_time_table[i].period;
                            }
                            else
                            {
                                fsm_time_table[i].delta = fsm_time_table[i].period - phase;
                            }
                        }
                        else
                        {
                            clear_timer_slot(i);
                        }

                        post_events(fsm, events);

#if (FSM_PROFILE_ENABLE != 0)
                        fsm_profile_system.timer_expirations += UINT32_C(1);
#endif
                    }
                    else
                    {
                        fsm_time_table[i].delta -= elapsed;
                    }
                }
            }
        }

#if (FSM_PROFILE_ENABLE != 0)
        {
            fsm_profile_tick_t timer_service_ticks = fsm_port_profile_now() - timer_service_start;

            if (timer_service_ticks > fsm_profile_system.max_timer_service_ticks)
            {
                fsm_profile_system.max_timer_service_ticks = timer_service_ticks;
            }
        }
#endif

        for (i = UINT32_C(0); i < (uint32_t)FSM_LAST; ++i)
        {
#if (FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK != 0)
            uint32_t events = take_fsm_events(i);
#else
            uint32_t events = fsm_table[i].events_set;
            if (events != UINT32_C(0))
            {
                /* The second check closes the IRQ race after the speculative outer read. */
                events = take_fsm_events(i);
            }
#endif

            if (events != UINT32_C(0))
            {
                uint32_t state;

#if (FSM_PROFILE_ENABLE != 0)
                fsm_profile_tick_t callback_start = fsm_port_profile_now();
#endif
                state = fsm_table[i].callback(fsm_table[i].state, events);

                /* A callback may remove itself; final cleanup follows its return. */
                if (fsm_table[i].callback == null_fsm)
                {
                    fsm_irq_state_t irq_state = fsm_port_irq_save();

                    fsm_table[i].state = UINT32_C(0);
                    fsm_table[i].events_set = UINT32_C(0);

                    fsm_port_irq_restore(irq_state);
                }
                else
                {
                    fsm_table[i].state = state;
                }

#if (FSM_PROFILE_ENABLE != 0)
                {
                    fsm_profile_tick_t callback_ticks = fsm_port_profile_now() - callback_start;

                    fsm_profile_fsm[i].calls += UINT32_C(1);
                    fsm_profile_fsm[i].total_ticks += (uint64_t)callback_ticks;

                    if (callback_ticks > fsm_profile_fsm[i].max_ticks)
                    {
                        fsm_profile_fsm[i].max_ticks = callback_ticks;
                    }
                }
#endif
            }
        }

#if (FSM_PROFILE_ENABLE != 0)
        {
            fsm_profile_tick_t loop_ticks = fsm_port_profile_now() - loop_start;

            fsm_profile_system.main_loops += UINT64_C(1);

            if (loop_ticks > fsm_profile_system.max_loop_ticks)
            {
                fsm_profile_system.max_loop_ticks = loop_ticks;
            }
        }
#endif

#if (FSM_TEST_ENABLE != 0)
        break;
#endif
    }
}

#if (FSM_TEST_ENABLE != 0)
/* Test-only access remains absent from every firmware build. */
void fsm_test_run_once(void)
{
    main_fsm();
}

void fsm_test_force_timer_generation(uint32_t slot, uint64_t generation)
{
    if ((slot < (uint32_t)FSM_TIMER_COUNT) &&
        (fsm_time_table[slot].delta == UINT32_C(0)))
    {
        fsm_time_table[slot].generation = generation;
    }
}
#endif
