#include "fsm.h"
#include "fsm_instance.h"
#include "board.h"


static uint32_t controller_fsm(uint32_t state, fsm_events_t events)
{
    if (have_event(events, Sys_GeneralEvent) != UINT32_C(0))
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
    fsm_timer_id_t controller_timer;

    board_gpio_init();

    start_fsm();
    (void)set_fsm(FSM_Controller, controller_fsm);
    controller_timer = add_periodical_timer(FSM_Controller, fsm_duration_from_ms(UINT32_C(100)), to_events_set(Sys_GeneralEvent));

    if (fsm_timer_is_valid(controller_timer) == UINT32_C(0))
    {
        return 1;
    }

    main_fsm();
}

void SystemInit(void)
{
}
