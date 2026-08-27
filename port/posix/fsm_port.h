/*
 * KAOS-RT: порт для POSIX. Не является прошивочным портом.
 *
 * Источник логических тиков: CLOCK_MONOTONIC.
 * Критическая секция: pthread_mutex_t.
 * Платформенные зависимости: POSIX.1-2008 и pthread.
 *
 * Профиль PhysicalClock берётся из fsm_instance.h: начальный offset и
 * постоянная ошибка частоты. Положительная ошибка ускоряет, отрицательная
 * замедляет локальный счётчик. По умолчанию оба параметра равны нулю.
 * Это Level 1 модели PhysicalClock.
 * Профилирование остаётся привязано к CLOCK_MONOTONIC.
 */

#ifndef FSM_PORT_H_
#define FSM_PORT_H_

#include <stdint.h>

#define FSM_PORT_EVENT_POLL_REQUIRES_IRQ_LOCK  1

#if defined(__clang__) || defined(__GNUC__)
#define FSM_PORT_WEAK __attribute__((weak))
#else
#define FSM_PORT_WEAK
#endif

typedef uint32_t fsm_time_t;
typedef uint64_t fsm_uptime_t;
typedef uint32_t fsm_irq_state_t;
typedef uint32_t fsm_profile_tick_t;

void fsm_port_time_init(void);
fsm_time_t fsm_port_time_now(void);
fsm_uptime_t fsm_port_uptime(void);
void fsm_port_profile_init(void);
fsm_profile_tick_t fsm_port_profile_now(void);
fsm_irq_state_t fsm_port_irq_save(void);
void fsm_port_irq_restore(fsm_irq_state_t state);

#endif /* FSM_PORT_H_ */
