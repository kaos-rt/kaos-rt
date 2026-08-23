#ifndef FSM_CONFIG_H_
#define FSM_CONFIG_H_

#include <stdint.h>

/* Logical timer resolution. A hardware port may require exact clock division. */
#ifndef FSM_TICK_FREQ_HZ
#define FSM_TICK_FREQ_HZ        UINT32_C(100000)
#endif

/* Fixed timer-pool capacity; the public API supports values from 1 through 256. */
#ifndef FSM_TIMER_COUNT
#define FSM_TIMER_COUNT         UINT32_C(10)
#endif

/* Set to 1 only in diagnostic or test builds. */
#ifndef FSM_PROFILE_ENABLE
#define FSM_PROFILE_ENABLE       0
#endif

/* Set only by test builds; it must remain zero in every firmware build. */
#ifndef FSM_TEST_ENABLE
#define FSM_TEST_ENABLE          0
#endif

#endif
