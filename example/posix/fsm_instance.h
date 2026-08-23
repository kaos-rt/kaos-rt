#ifndef FSM_INSTANCE_H_
#define FSM_INSTANCE_H_

#include <stdint.h>

typedef enum
{
    FSM_CONTROLLER = 0,
    FSM_ONE_SHOT,
    FSM_LAST
} fsm_id_t;

typedef enum
{
    FSM_CONTROLLER_STEP = 0,
    FSM_CONTROLLER_WAKEUP,
    FSM_ONE_SHOT_EXPIRED,
    FSM_EVENT_COUNT
} fsm_event_t;

#endif /* FSM_INSTANCE_H_ */
