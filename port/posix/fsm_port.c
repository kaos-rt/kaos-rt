/*
 * KAOS-RT: реализация порта для POSIX. Не является прошивочным портом.
 *
 * Источник логических тиков: CLOCK_MONOTONIC.
 * Критическая секция: pthread_mutex_t.
 * Платформенные зависимости: POSIX.1-2008 и pthread.
 */

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "fsm_config.h"
#include "fsm_instance.h"
#include "fsm_port.h"

#ifndef FSM_INSTANCE_CLOCK_INITIAL_OFFSET_US
#define FSM_INSTANCE_CLOCK_INITIAL_OFFSET_US  0
#endif

#ifndef FSM_INSTANCE_CLOCK_FREQUENCY_ERROR_PPB
#define FSM_INSTANCE_CLOCK_FREQUENCY_ERROR_PPB  0
#endif

static pthread_mutex_t fsm_posix_irq_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fsm_posix_time_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local uint32_t fsm_posix_irq_depth;
static struct timespec fsm_posix_time_previous;
static struct timespec fsm_posix_profile_base;
static int64_t fsm_posix_local_time_ns;
static int64_t fsm_posix_initial_offset_ns;
static int64_t fsm_posix_frequency_remainder;
static int32_t fsm_posix_frequency_error_ppb;

static struct timespec fsm_posix_clock_now(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        abort();
    }

    return now;
}

static fsm_profile_tick_t fsm_posix_elapsed_ticks(struct timespec base, uint32_t ticks_per_second)
{
    struct timespec now = fsm_posix_clock_now();
    uint64_t seconds;
    uint64_t nanoseconds;

    if ((now.tv_sec < base.tv_sec) ||
        ((now.tv_sec == base.tv_sec) && (now.tv_nsec < base.tv_nsec)))
    {
        abort();
    }

    seconds = (uint64_t)(now.tv_sec - base.tv_sec);

    if (now.tv_nsec < base.tv_nsec)
    {
        seconds -= UINT64_C(1);
        nanoseconds = UINT64_C(1000000000) + (uint64_t)now.tv_nsec - (uint64_t)base.tv_nsec;
    }
    else
    {
        nanoseconds = (uint64_t)now.tv_nsec - (uint64_t)base.tv_nsec;
    }

    return (fsm_profile_tick_t)((uint32_t)seconds * ticks_per_second) +
           (fsm_profile_tick_t)((nanoseconds * (uint64_t)ticks_per_second) /
                                UINT64_C(1000000000));
}

static uint64_t fsm_posix_elapsed_nanoseconds(struct timespec previous, struct timespec now)
{
    uint64_t seconds;
    uint64_t nanoseconds;

    if ((now.tv_sec < previous.tv_sec) ||
        ((now.tv_sec == previous.tv_sec) && (now.tv_nsec < previous.tv_nsec)))
    {
        abort();
    }

    seconds = (uint64_t)(now.tv_sec - previous.tv_sec);

    if (now.tv_nsec < previous.tv_nsec)
    {
        seconds -= UINT64_C(1);
        nanoseconds = UINT64_C(1000000000) + (uint64_t)now.tv_nsec - (uint64_t)previous.tv_nsec;
    }
    else
    {
        nanoseconds = (uint64_t)now.tv_nsec - (uint64_t)previous.tv_nsec;
    }

    return (seconds * UINT64_C(1000000000)) + nanoseconds;
}

static void fsm_posix_advance_local_clock(uint64_t elapsed_ns)
{
    int64_t elapsed_seconds = (int64_t)(elapsed_ns / UINT64_C(1000000000));
    int64_t elapsed_remainder_ns = (int64_t)(elapsed_ns % UINT64_C(1000000000));
    int64_t frequency_error_ppb = (int64_t)fsm_posix_frequency_error_ppb;
    int64_t frequency_fraction;
    int64_t frequency_correction_ns;

    frequency_fraction = (elapsed_remainder_ns * frequency_error_ppb) + fsm_posix_frequency_remainder;
    frequency_correction_ns = (elapsed_seconds * frequency_error_ppb) +
                              (frequency_fraction / INT64_C(1000000000));
    fsm_posix_frequency_remainder = frequency_fraction % INT64_C(1000000000);

    if (elapsed_ns > (uint64_t)INT64_MAX)
    {
        abort();
    }

    fsm_posix_local_time_ns += (int64_t)elapsed_ns + frequency_correction_ns;
}

static fsm_time_t fsm_posix_local_time_ticks(void)
{
    int64_t seconds = fsm_posix_local_time_ns / INT64_C(1000000000);
    int64_t nanoseconds = fsm_posix_local_time_ns % INT64_C(1000000000);
    uint64_t ticks;

    if (nanoseconds < INT64_C(0))
    {
        seconds -= INT64_C(1);
        nanoseconds += INT64_C(1000000000);
    }

    ticks = ((uint64_t)seconds * (uint64_t)FSM_TICK_FREQ_HZ) +
            (((uint64_t)nanoseconds * (uint64_t)FSM_TICK_FREQ_HZ) / UINT64_C(1000000000));

    return (fsm_time_t)ticks;
}

static uint64_t fsm_posix_uptime_usec(int64_t uptime_ns)
{
    uint64_t seconds;
    uint64_t nanoseconds;

    if (uptime_ns < INT64_C(0))
    {
        abort();
    }

    seconds = (uint64_t)(uptime_ns / INT64_C(1000000000));
    nanoseconds = (uint64_t)(uptime_ns % INT64_C(1000000000));

    return (seconds * UINT64_C(1000000)) + (nanoseconds / UINT64_C(1000));
}

void fsm_port_time_init(void)
{
    if (pthread_mutex_lock(&fsm_posix_time_mutex) != 0)
    {
        abort();
    }

    fsm_posix_time_previous = fsm_posix_clock_now();
    fsm_posix_initial_offset_ns = (int64_t)FSM_INSTANCE_CLOCK_INITIAL_OFFSET_US * INT64_C(1000);
    fsm_posix_local_time_ns = fsm_posix_initial_offset_ns;
    fsm_posix_frequency_remainder = INT64_C(0);
    fsm_posix_frequency_error_ppb = (int32_t)FSM_INSTANCE_CLOCK_FREQUENCY_ERROR_PPB;

    if (pthread_mutex_unlock(&fsm_posix_time_mutex) != 0)
    {
        abort();
    }
}

fsm_time_t fsm_port_time_now(void)
{
    struct timespec now;
    fsm_time_t local_time;

    if (pthread_mutex_lock(&fsm_posix_time_mutex) != 0)
    {
        abort();
    }

    now = fsm_posix_clock_now();
    fsm_posix_advance_local_clock(fsm_posix_elapsed_nanoseconds(fsm_posix_time_previous, now));
    fsm_posix_time_previous = now;
    local_time = fsm_posix_local_time_ticks();

    if (pthread_mutex_unlock(&fsm_posix_time_mutex) != 0)
    {
        abort();
    }

    return local_time;
}

uint64_t port_uptime_usec(void)
{
    struct timespec now;
    uint64_t uptime;

    if (pthread_mutex_lock(&fsm_posix_time_mutex) != 0)
    {
        abort();
    }

    now = fsm_posix_clock_now();
    fsm_posix_advance_local_clock(fsm_posix_elapsed_nanoseconds(fsm_posix_time_previous, now));
    fsm_posix_time_previous = now;
    uptime = fsm_posix_uptime_usec(fsm_posix_local_time_ns - fsm_posix_initial_offset_ns);

    if (pthread_mutex_unlock(&fsm_posix_time_mutex) != 0)
    {
        abort();
    }

    return uptime;
}

void fsm_port_profile_init(void)
{
    fsm_posix_profile_base = fsm_posix_clock_now();
}

fsm_profile_tick_t fsm_port_profile_now(void)
{
    return fsm_posix_elapsed_ticks(fsm_posix_profile_base, UINT32_C(1000000000));
}

fsm_irq_state_t fsm_port_irq_save(void)
{
    fsm_irq_state_t previous_state = UINT32_C(0);

    if (fsm_posix_irq_depth == UINT32_C(0))
    {
        if (pthread_mutex_lock(&fsm_posix_irq_mutex) != 0)
        {
            abort();
        }

        previous_state = UINT32_C(1);
    }

    fsm_posix_irq_depth += UINT32_C(1);

    return previous_state;
}

void fsm_port_irq_restore(fsm_irq_state_t state)
{
    if (fsm_posix_irq_depth == UINT32_C(0))
    {
        abort();
    }

    fsm_posix_irq_depth -= UINT32_C(1);

    if (state != UINT32_C(0))
    {
        if (fsm_posix_irq_depth != UINT32_C(0))
        {
            abort();
        }

        if (pthread_mutex_unlock(&fsm_posix_irq_mutex) != 0)
        {
            abort();
        }
    }

}
