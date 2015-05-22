#include "event_button.h"

#include "states/armingstates.h"

#include "core/uart.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "db/lexicon.h"

#include "service/humanize.h"


s32 process_mcu_button_msg(u8 *pkt)
{
    OUT_DEBUG_2("process_mcu_button_msg()\r\n");

    UnitIdent unit = getUnitIdent(pkt);
    PinTypeCode io_pin = (PinTypeCode)pkt[MCUPKT_BEGIN_DATABLOCK];
    char *lex = (char *)&pkt[MCUPKT_BEGIN_DATABLOCK + 1];

    // -- find Button object in the table
    Button *pButton = getButtonByParent(unit.uin, unit.type, unit.suin);
    if (!pButton)
        return ERR_DB_RECORD_NOT_FOUND;


    // -- check if the Button is enabled
    if (!pButton->enabled) {
        OUT_DEBUG_3("Button %d is disabled", pButton->humanized_id);
        return RETURN_NO_ERRORS;
    }


    ButtonEventType evt = (ButtonEventType)0;
    if (Ar_Lexicon_isEqual(lex, lexicon.button.out.EvtPress))
        evt = BUTTON_PRESSED;
    else if (Ar_Lexicon_isEqual(lex, lexicon.button.out.EvtRelease))
        evt = BUTTON_RELEASED;
    else
        return ERR_BAD_LEXICON;


    if (io_pin != Pin_Output)
        return ERR_BAD_PINTYPE_FOR_COMMAND;

    OUT_DEBUG_8("Unit %s #%d, button #%d, event: %d\r\n",
                 getUnitTypeByCode(unit.type), unit.uin, unit.suin, evt);

    //

    //  standard response to pressing
    //
    if(BUTTON_PRESSED==evt)
    {
        StopSystemErrorBufferCicl();
        //
        BehaviorPreset behavior = {0};
        behavior.pulse_len = 2;
        behavior.pause_len = 1;
        behavior.pulses_in_batch = 1;

        Led led;            // system (trouble) LED
        led.ptype = OBJ_ETR;
        led.puin = unit.uin;
        led.suin = 0x04;


        EtrBuzzer buzzer;
        buzzer.puin =unit.uin;
        s32 ret = 0;

        ret = setEtrBuzzerUnitState(&buzzer, UnitStateMultivibrator, &behavior);

        ret = setLedUnitState(&led, UnitStateMultivibrator, &behavior);
        //
    }

    // -- create Event
    ButtonEvent be;
    be.button_event_type = evt;
    Ql_GetLocalTime(&be.time);
    be.p_button = pButton;
    be.p_current_arming_state = NULL;

    // -- invoke Handler
    s32 ret = 0;

    if (be.p_current_arming_state) {
        ret = be.p_current_arming_state->handler_button(&be);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("%s::handler_zone() = %d error.\r\n", be.p_current_arming_state->state_name);
            return ret;
        }
    } else {
        ret = common_handler_button(&be);
        if (ret < RETURN_NO_ERRORS) {
            OUT_DEBUG_1("common_handler_button() = %d error.\r\n", ret);
            return ret;
        }
    }

    return RETURN_NO_ERRORS;
}

s32 common_handler_button(ButtonEvent *event)
{
    OUT_DEBUG_2("common_handler_button()\r\n");

    Button *pButton = event->p_button;
    ParentDevice *pParent = pButton->parent_device;

    switch (event->p_button->suin) {
    case 4: // MOVE LED INDICATOR DOWN
    {
        u8 current_decade = pParent->d.etr.led_indicator.current_led_decade;
        if (current_decade > 0 &&
                BUTTON_PRESSED == event->button_event_type)
            setETRLedIndicatorDecade(pParent, --current_decade);
        break;
    }

    case 3: // MOVE LED INDICATOR UP
    {
        u8 current_decade = pParent->d.etr.led_indicator.current_led_decade;
        if (current_decade < 10 &&
                BUTTON_PRESSED == event->button_event_type)
            setETRLedIndicatorDecade(pParent, ++current_decade);
        break;
    }

    case 2: // CALL
    {

        break;
    }

    case 1: // Trouble
    {
        break;
    }

    default:
        break;
    }

    /* handle user defined reactions */
    s32 ret = processReactions(pButton->events[event->button_event_type], NULL);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("processReactions() = %d error.\r\n", ret);
        return ret;
    }

    return RETURN_NO_ERRORS;
}
