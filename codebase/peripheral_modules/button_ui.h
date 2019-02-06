/*
 *  button_ui.h
 *
 *  Created on: 25-Apr-2018
 *
 *  Copyright (c) 2018, Appiko
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its contributors
 *  may be used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 *  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CODEBASE_PERIPHERAL_MODULES_BUTTON_UI_H_
#define CODEBASE_PERIPHERAL_MODULES_BUTTON_UI_H_

/**
 * @addtogroup group_peripheral_modules
 * @{
 *
 * @defgroup group_button_ui Button UI event generator
 *
 * @brief Driver for the generating all the button related UI events
 *  for the application.
 *
 * @{
 */


#include "stdint.h"
#include "stdbool.h"
#include "ms_timer.h"

/**
 * Enum defining the type of actions possible with buttons
 */
typedef enum
{
    BUTTON_UI_ACT_CROSS, //!< When a button press crosses a particular time
    BUTTON_UI_ACT_RELEASE//!< When a button is released
}button_ui_action;

/**
 * Enum to define the different kinds of button presses
 */
typedef enum
{
    BUTTON_UI_STEP_QUICK,//!< For a short button press
    BUTTON_UI_STEP_SHORT,//!< For a short button press
    BUTTON_UI_STEP_LONG, //!< For a long button press
    BUTTON_UI_STEP_WAKE, //!< For the moment a button is pressed
}button_ui_steps;

/**
 * An array defining the time duration in ms for the different
 *  kinds of button press defined in @ref button_ui_steps.
 *
 * @note @ref BUTTON_UI_STEP_WAKE does not have a duration
 */
const static uint32_t press_duration[] = {
    MS_TIMER_TICKS_MS(100),
    MS_TIMER_TICKS_MS(5000),
    MS_TIMER_TICKS_MS(15000),
    0xFFFFFFFF  ///This should never be reached
};

/**
 * @brief Initialize the button ui event generator module
 * @param button_pin Button Pin number to monitor
 * @param irq_priority IRQ priority of the GPIOTE irq used
 * @param button_ui_handler Handler to be called for all the button events
 */
void button_ui_init(uint32_t button_pin, uint32_t irq_priority,
        void (*button_ui_handler)(button_ui_steps step, button_ui_action act));
/**
 * @brief The ticks that need to be sent to the button UI module
 *  for its processing
 * @param ui_ticks UI ticks generated by next interval handler
 */
void button_ui_add_tick(uint32_t ui_ticks);

/**
 * @brief To enable/disable the button UI GPIOTE IRQ
 * @param set_wake_on Set to true to enable and false to disable
 */
void button_ui_config_wake(bool set_wake_on);

#endif /* CODEBASE_PERIPHERAL_MODULES_BUTTON_UI_H_ */

/**
 * @}
 * @}
 */
