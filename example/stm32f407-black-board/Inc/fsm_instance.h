#ifndef FSM_INSTANCE_H_
#define FSM_INSTANCE_H_

#include <stdint.h>

/* Application-owned static slots; lower values run earlier in each dispatcher pass. */
typedef enum
{
    FSM_Controller,
    FSM_LAST
} fsm_id_t;

/* Event indices map directly to the per-FSM 32-bit event bitmap. */
typedef enum
{
    Sys_GeneralEvent = 0,
    Sys_Fault,
    Sys_Timeout,
    FSM_EVENT_COUNT
} fsm_event_t;

#endif /* FSM_INSTANCE_H_ */
