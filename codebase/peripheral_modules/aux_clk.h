/*
 *  aux_clk.c : Module to handle auxiliary clock used by different modules
 *  Copyright (C) 2019  Appiko
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef AUX_CLK_H
#define AUX_CLK_H
/**
 * @addtogroup group_peripheral_modules
 * @{
 *
 * @defgroup group_aux_clk Auxiliary clock management
 *
 * @brief Module to manage auxiliary clock used by other modules like pir_sense
 * and tssp_detect. This module will switch between RTC(LFCLK) and TIMER(HFCLK)
 * as needed
 * @{
 */

#include "stdint.h"
#include "nrf_util.h"
#include "nrf.h"

#if SYS_CFG_PRESENT == 1
#include "sys_config.h"
#endif 

#ifndef AUX_CLK_PPI_CHANNELS_USED 
#define AUX_CLK_PPI_CHANNELS_USED 2
#endif

#ifndef AUX_CLK_PPI_CHANNEL_BASE
#define AUX_CLK_PPI_CHANNEL_BASE 5
#endif

#ifndef AUX_CLK_PPI_CHANNEL_0
#define AUX_CLK_PPI_CHANNEL_0 (AUX_CLK_PPI_CHANNEL_BASE + 0)
#endif

#ifndef AUX_CLK_PPI_CHANNEL_1
#define AUX_CLK_PPI_CHANNEL_1 (AUX_CLK_PPI_CHANNEL_BASE + 1)
#endif

#if (AUX_CLK_PPI_CHANNELS_USED > 2)
#ifndef AUX_CLK_PPI_CHANNEL_2
#define AUX_CLK_PPI_CHANNEL_2 (AUX_CLK_PPI_CHANNEL_BASE + 2)
#endif

#if (AUX_CLK_PPI_CHANNELS_USED > 3)
#ifndef AUX_CLK_PPI_CHANNEL_3
#define AUX_CLK_PPI_CHANNEL_3 (AUX_CLK_PPI_CHANNEL_BASE + 3)
#endif

#if (AUX_CLK_PPI_CHANNELS_USED > 4)
#error "Auxiliary clock module cannot handle more than 4 PPI channels"
#endif
#endif
#endif

#ifndef AUX_CLK_LFCLK_RTC_USED
#define AUX_CLK_LFCLK_RTC_USED 0
#endif

#ifndef AUX_CLK_HFCLK_TIMER_USED
#define AUX_CLK_HFCLK_TIMER_USED 2
#endif

#ifndef AUX_CLK_HFCLK_SOLO_MODULE 
#define AUX_CLK_HFCLK_SOLO_MODULE 1
#endif
#define AUX_CLK_NO_IRQ 0xFFFFFFFF

#define AUX_CLK_MAX_CHANNELS 4



/** List of Clock sources */
typedef enum
{
    /** Low Freq Clock : RTC peripheral */
    AUX_CLK_SRC_LFCLK,
    /** High Freq Clock : Timer peripheral */
    AUX_CLK_SRC_HFCLK,
}aux_clk_source_t;

/** List of events generated by this module */
typedef enum
{
    /** Event generated on clock overflow */
    AUX_CLK_EVT_NON = 0x00,
    /** Event generated channel 0 compare match */
    AUX_CLK_EVT_CC0 = 0x01,
    /** Event generated channel 1 compare match */
    AUX_CLK_EVT_CC1 = 0x02,
    /** Event generated channel 2 compare match */
    AUX_CLK_EVT_CC2 = 0x04,
    /** Event generated channel 3 compare match */
    AUX_CLK_EVT_CC3 = 0x08,
}aux_clk_evt_t;

/** List of tasks which can be performed on clocks */
typedef enum
{
    /** Task to start the clock */
    AUX_CLK_TASKS_START,
    /** Task to stop the clock */
    AUX_CLK_TASKS_STOP,
    /** Task to clear the counter value */
    AUX_CLK_TASKS_CLEAR,
}aux_clk_tsk_t;

/** Structure to store settings of PPI channels */
typedef struct
{
    /** Event to trigger the PPI. Event can be either from @ref aux_clk_evt_t
     * or default nRF peripheral event */
    uint32_t event;
    /** Primary task which is to be performed once given event occurs
     * Task can be from @ref aux_clk_tsk_t or default nRF peripheral tasks */
    uint32_t task1;
    /** Secondary task which is to be performed once given event occurs
     * Task can be from @ref aux_clk_tsk_t or default nRF peripheral tasks */
    uint32_t task2;
}aux_clk_ppi_t;

/** Structure to store data required to set auxiliary clock module */
typedef struct
{
    /** Source of auxiliary clock */
    aux_clk_source_t source;
    /** IRQ priority for this module
     * @note If no Interrupt is required send APP_IRQ_PRIORITY_THREAD
     */
    app_irq_priority_t irq_priority;
    /** Interrupt handler callback function pointer */
    void (* callback_handler) (uint8_t events);
    /** Array to store comapre and capture values for clock channels in milliseconds */
    uint32_t arr_cc_ms[AUX_CLK_MAX_CHANNELS];
    /** Enable events from @ref aux_clk_evt_t 'or'ed together
     * @note Events required for PPI also has to be 'or'ed here 
     */
    uint8_t events_en;
    /** Array of PPI settings for different PPI channels */
    aux_clk_ppi_t arr_ppi_cnf[AUX_CLK_PPI_CHANNELS_USED];
}aux_clk_setup_t;

/**
 * @brief Function to setup the Auxiliary clock module
 * @param aux_clk Structure pointer to data type @ref aux_clk_setup_t used to 
 * setup the module
 * 
 * @note This Funciton will not start auxiliary clock. Call @ref aux_clk_start ();
 */
void aux_clk_set (aux_clk_setup_t * aux_clk);

/**
 * @brief Function to select the clock source for auxiliary clock module
 * @param source Clock source used. @ref aux_clk_source_t
 */
void aux_clk_select_src (aux_clk_source_t source);

/**
 * @brief Function to update the given CC value for clock channel 
 * @param cc_id Clock channel id
 * @param new_val_ms New value in milliseconds
 */
void aux_clk_update_cc (uint32_t cc_id,uint32_t new_val_ms);

/**
 * @brief Function to Update PPI settings for given ppi channel
 * @param ppi_channel PPI channel of which settings are to be updated @ref AUX_CLK_PPI_CHANNEL_x

 * @param new_ppi New PPI settings
 */
void aux_clk_update_ppi (uint32_t ppi_channel, aux_clk_ppi_t * new_ppi);

/**
 * @brief Function to get ms since Auxiliary clock has started or cleared
 * @return MS count
 */
uint32_t aux_clk_get_ms (void);

/**
 * @breif Function to start clock
 */
void aux_clk_start ();

/**
 * @brief Function to stop clock
 */
void aux_clk_stop ();

/**
 * @brief Function to clear clock counter value
 */
void aux_clk_clear ();

/**
 * @brief Function to enable one or more events from @ref aux_clk_evt_t
 * @param events One or more events 'or'ed together
 */
void aux_clk_en_evt (uint8_t events);

/**
 * @brief Function to disable one or more events from @ref aux_clk_evt_t
 * @param events One or more events 'or'ed together
 */
void aux_clk_dis_evt (uint8_t events);

/**
 * @brief Function to change IRQ Priority if needed.
 * @param new_priority New IRQ priority.
 */
void aux_clk_update_irq_priority (app_irq_priority_t new_priority);

/**
 * @brief Function to clear Auxiliary clock PPI channel
 * @param aux_ppi_channel Auxiliary PPI channel which is to be cleared @ref AUX_CLK_PPI_CHANNEL
 */
void aux_clk_dis_ppi_ch (uint32_t aux_ppi_channel);

/**
 * @brief Function to set Auxiliary clock PPI channel
 * @param aux_ppi_channel Auxiliary PPI channel which is to be set @ref AUX_CLK_PPI_CHANNEL
 */
void aux_clk_en_ppi_ch (uint32_t aux_ppi_channel);

/**
 * @}
 * @}
 */
#endif /* AUX_CLK_H */
