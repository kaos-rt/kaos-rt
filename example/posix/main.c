#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "fsm.h"

static atomic_uint_fast32_t controller_steps;
static atomic_uint_fast32_t controller_wakeups;
static atomic_uint_fast32_t one_shot_completed;

/* KAOS-SRS-002: callback выполняет только короткий неблокирующий шаг. */
static uint32_t controller_fsm(uint32_t state, uint32_t events)
{
    if (have_event(events, FSM_CONTROLLER_STEP) != FSM_EVENTS_NONE)
    {
        (void)atomic_fetch_add_explicit(&controller_steps, UINT32_C(1), memory_order_relaxed);
    }

    if (have_event(events, FSM_CONTROLLER_WAKEUP) != FSM_EVENTS_NONE)
    {
        (void)atomic_fetch_add_explicit(&controller_wakeups, UINT32_C(1), memory_order_relaxed);
    }

    return state;
}

/* KAOS-SRS-004: автомат может удалить собственный слот после завершения работы. */
static uint32_t one_shot_fsm(uint32_t state, uint32_t events)
{
    if (have_event(events, FSM_ONE_SHOT_EXPIRED) != FSM_EVENTS_NONE)
    {
        atomic_store_explicit(&one_shot_completed, UINT32_C(1), memory_order_relaxed);
        (void)remove_fsm(FSM_ONE_SHOT);
    }

    return state;
}

/* KAOS-SRS-003: внешний источник событий вызывает только append_event(). */
static void *event_source(void *argument)
{
    const struct timespec pause = { .tv_sec = 1, .tv_nsec = 0L };

    (void)argument;
    (void)nanosleep(&pause, NULL);
    (void)append_event(FSM_CONTROLLER, FSM_CONTROLLER_WAKEUP);

    return NULL;
}

/* Печать и ожидание выполняются вне callback и не входят в работу ядра. */
static void *reporter(void *argument)
{
    const struct timespec pause = { .tv_sec = 1, .tv_nsec = 0L };

    (void)argument;

    for (;;)
    {
        (void)nanosleep(&pause, NULL);
        (void)printf("steps=%" PRIuFAST32 " wakeups=%" PRIuFAST32 " one_shot=%" PRIuFAST32 "\n",
            atomic_load_explicit(&controller_steps, memory_order_relaxed),
            atomic_load_explicit(&controller_wakeups, memory_order_relaxed),
            atomic_load_explicit(&one_shot_completed, memory_order_relaxed));
        (void)fflush(stdout);
    }
}

int main(void)
{
    pthread_t event_thread;
    pthread_t reporter_thread;
    fsm_timer_id_t periodic_timer;
    fsm_timer_id_t one_shot_timer;

    /* KAOS-SRS-001: ядро запускается до регистрации автоматов и таймеров. */
    start_fsm();

    if ((set_fsm(FSM_CONTROLLER, controller_fsm) == NULL) || (set_fsm(FSM_ONE_SHOT, one_shot_fsm) == NULL))
    {
        return 1;
    }

    /* KAOS-SRS-003: несколько одинаковых событий объединяются в одну маску. */
    (void)append_event(FSM_CONTROLLER, FSM_CONTROLLER_WAKEUP);
    (void)append_event(FSM_CONTROLLER, FSM_CONTROLLER_WAKEUP);
    (void)append_event(FSM_CONTROLLER, FSM_CONTROLLER_WAKEUP);

    /* KAOS-SRS-005: используются периодический и одноразовый таймеры. */
    periodic_timer = add_periodical_timer(FSM_CONTROLLER, fsm_time_from_ms(UINT32_C(100)), to_events_set(FSM_CONTROLLER_STEP));
    one_shot_timer = add_timer(FSM_ONE_SHOT, fsm_time_from_ms(UINT32_C(250)), to_events_set(FSM_ONE_SHOT_EXPIRED));

    if ((periodic_timer == FSM_TIMER_INVALID_ID) || (one_shot_timer == FSM_TIMER_INVALID_ID))
    {
        return 2;
    }

    if ((pthread_create(&event_thread, NULL, event_source, NULL) != 0)
        || (pthread_detach(event_thread) != 0)
        || (pthread_create(&reporter_thread, NULL, reporter, NULL) != 0)
        || (pthread_detach(reporter_thread) != 0))
    {
        return 3;
    }

    main_fsm();
}
