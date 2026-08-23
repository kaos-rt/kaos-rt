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
#include "fsm_port.h"

static pthread_mutex_t fsm_posix_irq_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local uint32_t fsm_posix_irq_depth;
static struct timespec fsm_posix_time_base;
static struct timespec fsm_posix_profile_base;

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

void fsm_port_time_init(void)
{
    fsm_posix_time_base = fsm_posix_clock_now();
}

fsm_time_t fsm_port_time_now(void)
{
    return (fsm_time_t)fsm_posix_elapsed_ticks(fsm_posix_time_base, FSM_TICK_FREQ_HZ);
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
