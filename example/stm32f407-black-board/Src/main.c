#include "fsm.h"
#include "fsm_instance.h"
#include "board.h"


static uint32_t controller_fsm(uint32_t state, uint32_t events)
{
    if (have_event(events, Sys_GeneralEvent) != FSM_EVENTS_NONE)
    {
        if (key0_pressed() != UINT32_C(0))
        {
            led0_on();
        }

        if (key1_pressed() != UINT32_C(0))
        {
            led0_off();
        }
    }

    return state;
}

int main(void)
{
    board_gpio_init();

    start_fsm();
    (void)set_fsm(FSM_Controller, controller_fsm);
    (void)add_periodical_timer(FSM_Controller, fsm_time_from_ms(UINT32_C(100)), to_events_set(Sys_GeneralEvent));

    main_fsm();
}

void SystemInit(void)
{
}
