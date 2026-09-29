/*
 * Copyright 2026 Morozov Oleg and Chirkov Boris
 * SPDX-License-Identifier: Apache-2.0
 *
 * KAOS-RT core API.
 *
 * Language baseline: ISO C17.
 * The core is written with MISRA C and SEI CERT C conformance in mind.
 */

#ifndef FSM_H_
#define FSM_H_

#include <stdint.h>

#include "fsm_config.h"

/* Logical timestamps and durations are intentionally incompatible types. */
typedef struct
{
    uint32_t ticks;
} fsm_time_t;

typedef struct
{
    uint32_t ticks;
} fsm_duration_t;

typedef struct
{
    uint32_t mask;
} fsm_events_t;

typedef struct
{
    uint32_t slot;
    uint32_t generation;
} fsm_timer_id_t;

_Static_assert(sizeof(fsm_time_t) == sizeof(uint32_t), "fsm_time_t must remain 32-bit");
_Static_assert(sizeof(fsm_duration_t) == sizeof(uint32_t), "fsm_duration_t must remain 32-bit");
_Static_assert(sizeof(fsm_events_t) == sizeof(uint32_t), "fsm_events_t must remain 32-bit");
_Static_assert(sizeof(fsm_timer_id_t) == (sizeof(uint32_t) * 2U), "fsm_timer_id_t must remain two 32-bit words");

#include "fsm_instance.h"
#include "fsm_port.h"

#ifndef FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK
#define FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK  0
#endif

_Static_assert((FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK == 0) || (FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK == 1),
               "FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK must be 0 or 1");

typedef uint32_t (*fsm_t)(uint32_t state, fsm_events_t events);

/* Public operation status values: success is always zero. */
typedef enum
{
    FSM_STATUS_OK = 0,
    FSM_STATUS_INVALID_FSM,
    FSM_STATUS_INVALID_EVENT,
    FSM_STATUS_NULL_ARGUMENT
} fsm_status_t;

/* Event bitmap sentinel and the invalid FSM identifier. */
#define FSM_EVENTS_NONE      ((fsm_events_t){ .mask = UINT32_C(0) })
#define FSM_INVALID_ID       FSM_LAST

#if (FSM_PROFILE_ENABLE != 0)
/* Diagnostic-only state; absent from a release build. */
typedef struct
{
    uint32_t calls;
    uint64_t total_ticks;
    fsm_profile_tick_t max_ticks;
} fsm_profile_fsm_t;

typedef struct
{
    uint64_t main_loops;
    fsm_profile_tick_t max_loop_ticks;
    fsm_profile_tick_t max_timer_service_ticks;
    uint32_t active_timers;
    uint32_t peak_active_timers;
    uint32_t timer_expirations;
    uint32_t timer_missed_periods;
    uint32_t timer_allocation_failures;
    uint32_t event_posts;
    uint32_t coalesced_event_posts;
} fsm_profile_system_t;
#endif

/* A timer ID contains its slot and a non-zero reuse generation. */
#define FSM_TIMER_INVALID_ID      ((fsm_timer_id_t){ .slot = UINT32_C(0), .generation = UINT32_C(0) })

typedef enum
{
    FSM_TIMER_REMOVED = 0,
    FSM_TIMER_NOT_FOUND
} fsm_timer_result_t;

/* Compile-time configuration contract. */
_Static_assert(FSM_TICK_FREQ_HZ > UINT32_C(0),
               "FSM_TICK_FREQ_HZ must be greater than zero");

_Static_assert(FSM_LAST > 0,
               "At least one FSM must be declared");

_Static_assert(FSM_TIMER_COUNT > UINT32_C(0),
               "FSM_TIMER_COUNT must be greater than zero");

_Static_assert(FSM_TIMER_COUNT <= UINT32_C(256),
               "FSM_TIMER_COUNT exceeds the supported capacity");

_Static_assert(FSM_EVENT_COUNT > 0,
               "At least one event must be declared");

_Static_assert(FSM_EVENT_COUNT <= UINT32_C(32),
               "FSM_EVENT_COUNT exceeds event bitmap capacity");

_Static_assert((FSM_PROFILE_ENABLE == 0) || (FSM_PROFILE_ENABLE == 1),
               "FSM_PROFILE_ENABLE must be 0 or 1");

_Static_assert((FSM_TEST_ENABLE == 0) || (FSM_TEST_ENABLE == 1),
               "FSM_TEST_ENABLE must be 0 or 1");

/* Event and logical-time helpers. */
static inline fsm_events_t to_events_set(fsm_event_t event)
{
    uint32_t event_index = (uint32_t)event;
    fsm_events_t events = FSM_EVENTS_NONE;

    if (event_index < (uint32_t)FSM_EVENT_COUNT)
    {
        events.mask = UINT32_C(1) << event_index;
    }

    return events;
}

static inline fsm_events_t fsm_events_union(fsm_events_t left, fsm_events_t right)
{
    return (fsm_events_t){ .mask = left.mask | right.mask };
}

static inline uint32_t fsm_events_is_empty(fsm_events_t events)
{
    return (uint32_t)(events.mask == UINT32_C(0));
}

static inline uint32_t fsm_events_are_valid(fsm_events_t events)
{
    uint32_t valid_mask = UINT32_MAX >> (UINT32_C(32) - (uint32_t)FSM_EVENT_COUNT);

    return (uint32_t)((events.mask & ~valid_mask) == UINT32_C(0));
}

static inline uint32_t have_event(fsm_events_t events, fsm_event_t event)
{
    return events.mask & to_events_set(event).mask;
}

static inline fsm_duration_t fsm_duration_from_units(uint32_t value, uint32_t units_per_second)
{
    /*
    * uint64_t is used only for intermediate arithmetic.
    * Runtime duration representation remains uint32_t logical ticks.
    */
    uint64_t product;
    uint64_t ticks;

    if ((value == UINT32_C(0)) || (units_per_second == UINT32_C(0)))
    {
        return (fsm_duration_t){ .ticks = UINT32_C(0) };
    }

    product = (uint64_t)value * (uint64_t)FSM_TICK_FREQ_HZ;
    ticks = product / (uint64_t)units_per_second;

    if ((product % (uint64_t)units_per_second) != UINT64_C(0))
    {
        ticks += UINT64_C(1);
    }

    if (ticks > (uint64_t)UINT32_MAX)
    {
        return (fsm_duration_t){ .ticks = UINT32_C(0) };
    }

    return (fsm_duration_t){ .ticks = (uint32_t)ticks };
}

static inline fsm_duration_t fsm_duration_from_us(uint32_t value)
{
    return fsm_duration_from_units(value, UINT32_C(1000000));
}

static inline fsm_duration_t fsm_duration_from_ms(uint32_t value)
{
    return fsm_duration_from_units(value, UINT32_C(1000));
}

static inline fsm_duration_t fsm_duration_from_s(uint32_t value)
{
    return fsm_duration_from_units(value, UINT32_C(1));
}

static inline fsm_duration_t fsm_time_elapsed(fsm_time_t since, fsm_time_t now)
{
    /* Unsigned subtraction preserves elapsed time across one counter wrap. */
    return (fsm_duration_t){ .ticks = now.ticks - since.ticks };
}

/* Timer-handle validity is the only public handle inspection operation. */
static inline uint32_t fsm_timer_is_valid(fsm_timer_id_t id)
{
    return (uint32_t)((id.slot < (uint32_t)FSM_TIMER_COUNT) && (id.generation != UINT32_C(0)));
}

/* Dispatcher lifecycle. */
void start_fsm(void);
void main_fsm(void);

/* Optional bounded hook, called once at the beginning of every dispatcher pass. */
void fsm_loop_service(void);

/* FSM lifecycle and state diagnosis. */
fsm_t set_fsm(fsm_id_t fsm, fsm_t callback);
fsm_t remove_fsm(fsm_id_t fsm);
fsm_status_t get_fsm_state(fsm_id_t fsm, uint32_t *state);

/* Event bitmap API; only append_event() is ISR-safe. */
fsm_status_t append_event(fsm_id_t fsm, fsm_event_t event);
fsm_status_t remove_event(fsm_id_t fsm, fsm_event_t event);
fsm_status_t remove_events(fsm_id_t fsm);

/* Timer API. The target FSM precedes the duration and event mask. */
fsm_timer_id_t add_timer(fsm_id_t fsm, fsm_duration_t delay, fsm_events_t events);
fsm_timer_id_t add_periodical_timer(fsm_id_t fsm, fsm_duration_t period, fsm_events_t events);
fsm_timer_result_t remove_timer(fsm_timer_id_t timer_id);
void remove_timers(fsm_id_t fsm);

#if (FSM_PROFILE_ENABLE != 0)
/* Snapshot diagnostics; never use these values as release timing acceptance. */
fsm_status_t fsm_profile_get_fsm(fsm_id_t fsm, fsm_profile_fsm_t *profile);
fsm_status_t fsm_profile_get_system(fsm_profile_system_t *profile);
#endif

#if (FSM_TEST_ENABLE != 0)
/* Test build only: execute one complete main-loop pass and return. */
void fsm_test_run_once(void);

/* Test build only: prepare a free timer slot for generation-wrap testing. */
void fsm_test_force_timer_generation(uint32_t slot, uint32_t generation);
#endif

#endif /* FSM_H_ */
