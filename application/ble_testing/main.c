/*
 *  main.c : Application for SenseBe devices.
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

/**
 * @addtogroup group_appln
 * @{
 *
 * @defgroup sensebe_appln The code for the active IR based Sense units.
 * @brief The active IR sense application's main file that makes it operate.
 *
 * @{
 *
 */

#include "stdbool.h"
#include "stdint.h"
#include <stddef.h>
#include <string.h>

#include "evt_sd_handler.h"
#include "nrf.h"
#include "boards.h"

#include "log.h"
#include "hal_wdt.h"
#include "nrf_util.h"
#include "hal_gpio.h"
#include "ms_timer.h"
#include "hal_nop_delay.h"
#include "irq_msg_util.h"
#include "device_tick.h"
#include "button_ui.h"
#include "nrf_nvic.h"
#include "ble.h"
#include "nrf_sdm.h"
#include "app_error.h"
#include "sensebe_ble.h"
#include "led_seq.h"
#include "led_ui.h"

/* ----- Defines ----- */

/**< Name of device, to be included in the advertising data. */

#define APP_DEVICE_NAME_CHAR 'S', 'e', 'n', 's', 'e', 'B', 'e'
const uint8_t app_device_name[] = {APP_DEVICE_NAME_CHAR};

/** Complete 128 bit UUID of the SenseBe service
 * 3c73dc60-07f5-480d-b066-837407fbde0a */
#define APP_UUID_COMPLETE 0x0a, 0xde, 0xfb, 0x07, 0x74, 0x83, 0x66, 0xb0, 0x0d, 0x48, 0xf5, 0x07, 0x60, 0xdc, 0x73, 0x3c

/** The data to be sent in the advertising payload. It is of the format
 *  of a sequence of {Len, type, data} */
#define APP_ADV_DATA                                                                                \
    {                                                                                               \
        0x02, BLE_GAP_AD_TYPE_FLAGS, BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE,                   \
            sizeof(app_device_name) + 1, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, APP_DEVICE_NAME_CHAR, \
            0x11, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE, APP_UUID_COMPLETE                   \
    }

/** The data to be sent in the scan response payload. It is of the format
 *  of a sequence of {Len, type, data} */
#define APP_SCAN_RSP_DATA                                                                   \
    {                                                                                       \
        0x02, BLE_GAP_AD_TYPE_TX_POWER_LEVEL, 0,                                            \
            0x11, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,                                         \
            'S', 'B', '0', '3', '0', '0', '1', '9', '0', '8', '2', '2', 'R', '0', '8', '8', \
            0x04, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0, 0, 0                       \
    }

/** The WDT bites if not fed every 301 sec (5 min)
 * @warning All the tick intervals must be lower than this*/
#define WDT_PERIOD_MS 301000

/** Flag to specify if the Watchdog timer is used or not */
#define ENABLE_WDT 1

/** The fast tick interval in ms in the Sense mode */
#define SENSE_FAST_TICK_INTERVAL_MS 60
/** The slow tick interval in ms in the Sense mode */
#define SENSE_SLOW_TICK_INTERVAL_MS 300000

/** The fast tick interval in ms in the Advertising mode */
#define ADV_FAST_TICK_INTERVAL_MS 60
/** The slow tick interval in ms in the Advertising mode */
#define ADV_SLOW_TICK_INTERVAL_MS 1100

/** The fast tick interval in ms in the Connected mode */
#define CONN_FAST_TICK_INTERVAL_MS 60
/** The slow tick interval in ms in the Connected mode */
#define CONN_SLOW_TICK_INTERVAL_MS 1100

/** The time in ms (min*sec*ms) to timeout of the Connected mode*/
#define CONN_TIMEOUT_MS (10 * 60 * 1000)

/** Defines the states possible in the SensePi device */
typedef enum
{
    SENSING,     //!< Use IR Tx-Rx to sense motion based on the set configuration
    ADVERTISING, //!< BLE advertising to get connected to an app
    CONNECTED    //!< BLE connection established with an app
} sense_states;

/* ----- Global constants in flash ----- */

/* ----- Global variables in RAM ----- */
/** Stores the current state of the device */
sense_states current_state;

/** To keep track of the amount of time in the connected state */
static uint32_t conn_count;

static senseberx_config_t senseberx_ble_default_config = {

    .battery_type = BATTERY_STANDARD,

    .current_date.dd = 0,
    .current_date.mm = 0,
    .current_date.yy = 0,

    .current_time = 0,

    .dev_name = {'S', 'e', 'n', 's', 'e', 'B', 'e'},

    .radio_control.radio_channel = CHANNEL0,
    .radio_control.radio_oper_duration_25ms = 0,
    .radio_control.radio_oper_freq_100us = 0,

    .speed = SPEED_FAST,

    .trigger_oper_cond_sel = {1, 1},

};

/* ----- Function declarations ----- */

/* ----- Function definitions ----- */
/** Function called just before reset due to WDT */
void wdt_prior_reset_callback(void)
{
    log_printf("WDT reset\n");
}
uint8_t app_scan_rsp_data[] = APP_SCAN_RSP_DATA;

void prepare_init_ble_adv()
{
    uint8_t app_adv_data[] = APP_ADV_DATA;

    //Add in the firmware version
    // memcpy(&app_scan_rsp_data[23], fw_ver_get(), sizeof(fw_ver_t));
    app_scan_rsp_data[25] = 8;

    //Add the device ID
     memcpy((char *) &app_scan_rsp_data[5], senseberx_ble_default_config.dev_name, sizeof(dev_id_t));

    senseberx_ble_adv_data_t app_adv_data_struct =
        {
            .adv_data = app_adv_data,
            .scan_rsp_data = app_scan_rsp_data,
            .adv_len = ARRAY_SIZE(app_adv_data),
            .scan_rsp_len = ARRAY_SIZE(app_scan_rsp_data)};

    senseberx_ble_adv_init(&app_adv_data_struct);
}

/**
 * @brief Handler which will address all BLE related events
 *  generated by the SoftDevice for SensePi application related code.
 * @param evt The pointer to the buffer containing all the data
 *  related to the event
 */
static void ble_evt_handler(ble_evt_t *evt)
{
    log_printf("ble evt %x\n", evt->header.evt_id);
    switch (evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
        irq_msg_push(MSG_STATE_CHANGE, (void *)CONNECTED);
        break;
    case BLE_GAP_EVT_DISCONNECTED:
        irq_msg_push(MSG_STATE_CHANGE, (void *)SENSING);
        break;
    case BLE_GAP_EVT_ADV_SET_TERMINATED:
        irq_msg_push(MSG_STATE_CHANGE, (void *)SENSING);
        break;
    case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        log_printf("sup time %d s, max intvl %d ms, min intvl %d ms, slave lat %d\n",
                   evt->evt.gap_evt.params.conn_param_update.conn_params.conn_sup_timeout / 100,
                   (5 * evt->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval) / 4,
                   (5 * evt->evt.gap_evt.params.conn_param_update.conn_params.min_conn_interval) / 4,
                   evt->evt.gap_evt.params.conn_param_update.conn_params.slave_latency);
        break;
    }
}

/**
 * Handler to get the configuration of SensePi from the mobile app
 * @param config Pointer to the configuration received
 */
static void get_sensebe_config_t(senseberx_config_t *config)
{
uint8_t * buff_ = (uint8_t * )config;
for(uint32_t cnt = 0; cnt < sizeof(senseberx_config_t); cnt++)
{
    log_printf("%d ", buff_[cnt]);
}
log_printf("\n");

    // Print config

    log_printf("\n\n\n Battery Type %d", config->battery_type);

    log_printf("\n\n\n Date: %d/%d/%d", config->current_date.dd, config->current_date.mm, config->current_date.yy);
    log_printf("\n\n\n Time: %d s", config->current_time);

    log_printf("\n\n\n Device Name: %s", config->dev_name);
    log_printf("\n\n\n Radio Channel: %d \n Radio Operation Duration: %d s \n Radio Operation Frequency: %d Hz.", config->radio_control.radio_channel, config->radio_control.radio_oper_duration_25ms, config->radio_control.radio_oper_freq_100us);
    log_printf("\n\n\n Speed: %d", config->speed);
    log_printf("\n\n\n Motion: %d\n Timer: %d", config->trigger_oper_cond_sel[0], config->trigger_oper_cond_sel[1]);

    int i;
    for (i = 0; i < MAX_SETTINGS; i++)
    {
        //        Camera Options
        log_printf("\n\n\n Camera Options");
        log_printf("\n Camera Mode %d", config->generic_settings[i].cam_setting.cam_trigger.mode);
        log_printf("\n Mode Mode 0x%x", config->generic_settings[i].cam_setting.cam_trigger.mode_setting);
        log_printf("\n PreFocus On?: %d", config->generic_settings[i].cam_setting.cam_trigger.pre_focus_en);
        log_printf("\n Video with full press?: %d", config->generic_settings[i].cam_setting.cam_trigger.video_w_full_press_en);
        log_printf("\n Pre focus pulse duration: %d", config->generic_settings[i].cam_setting.cam_trigger.prf_pulse_duration_100ms);
        log_printf("\n Radio enabled?: %d", config->generic_settings[i].cam_setting.cam_trigger.radio_trig_en);
        log_printf("\n Trigger pulse duration: %d", config->generic_settings[i].cam_setting.cam_trigger.trig_pulse_duration_100ms);

        //        Operation Time
        log_printf("\n\n\n Operation Time");
        log_printf("\n A Lower: %d\n A Higher: %d", config->generic_settings[i].cam_setting.oper_cond.light_cond.lower_light_threshold, config->generic_settings[i].cam_setting.oper_cond.light_cond.higher_light_threshold);
        log_printf("\n S Time: %d\n E Time: %d", config->generic_settings[i].cam_setting.oper_cond.time_cond.start_time, config->generic_settings[i].cam_setting.oper_cond.time_cond.end_time);

        //        Sensor Setting
        if (config->generic_settings[i].trig_sel == 0)
        {

            log_printf("\n\n\n Motion  Sensor");
            log_printf("\n Rx Enabled?: %d ", config->generic_settings[i].func_setting.detection_func.is_enable);
            log_printf("\n Sensitivity: %d", config->generic_settings[i].func_setting.detection_func.sensitivity);
            log_printf("\n Inter Trigger Timer: %d", config->generic_settings[i].func_setting.detection_func.inter_trig_time);
            log_printf("\n Number of triggers: %d", config->generic_settings[i].func_setting.detection_func.detect_trigger_num);
        }
        else
        {
            log_printf("\n\n\n Timer Setting");
            log_printf("\n Timer duration:%d \n", config->generic_settings[i].func_setting.timer_duration);
        }
    }
    memcpy(&senseberx_ble_default_config, config, sizeof(senseberx_config_t));
    
    uint8_t * buffer_ = (uint8_t * ) &senseberx_ble_default_config;
for(uint32_t cnt = 0; cnt < sizeof(senseberx_config_t); cnt++)
{
    log_printf("%d ", buffer_[cnt]);
}
log_printf("\n");

}

/**
 * @brief The next interval handler is used for providing a periodic tick
 *  to be used by the various modules of the application
 * @param interval The interval from the last call of this function
 */
void next_interval_handler(uint32_t interval)
{
    log_printf("in %d\n", interval);
    button_ui_add_tick(interval);
    switch (current_state)
    {
    case SENSING:
    {
        log_printf("Nxt Evt Hndlr : SENSING\n");
    }
    break;
    case ADVERTISING:
        break;
    case CONNECTED:
    {
        conn_count += interval;
        if (conn_count > MS_TIMER_TICKS_MS(CONN_TIMEOUT_MS))
        {
            senseberx_ble_disconn();
        }
    }
    break;
    }
}

/**
 * @brief The handler that is called whenever the application transitions
 *  to a new state.
 * @param new_state The state to which the application has transitioned to.
 */
void state_change_handler(uint32_t new_state)
{
    log_printf("State change %d\n", new_state);
    if (new_state == current_state)
    {
        log_printf("new state same as current state\n");
        return;
    }
    current_state = (sense_states)new_state;

    switch (current_state)
    {
    case SENSING:
    {
        sd_softdevice_disable();
        log_printf("State Change : SENSING\n");
        device_tick_cfg tick_cfg =
            {
                MS_TIMER_TICKS_MS(SENSE_FAST_TICK_INTERVAL_MS),
                MS_TIMER_TICKS_MS(SENSE_SLOW_TICK_INTERVAL_MS),
                DEVICE_TICK_SAME};
        led_ui_type_stop_all(LED_UI_LOOP_SEQ);

        device_tick_init(&tick_cfg);
    }
    break;
    case ADVERTISING:
    {
        conn_count = 0;

        device_tick_cfg tick_cfg =
            {
                MS_TIMER_TICKS_MS(ADV_FAST_TICK_INTERVAL_MS),
                MS_TIMER_TICKS_MS(ADV_SLOW_TICK_INTERVAL_MS),
                DEVICE_TICK_SAME};
        device_tick_init(&tick_cfg);

        uint8_t is_sd_enabled;
        sd_softdevice_is_enabled(&is_sd_enabled);
        // Would be coming from the SENSING mode
        if (is_sd_enabled == 0)
        {
            senseberx_ble_stack_init();
            senseberx_ble_gap_params_init();
            senseberx_ble_service_init();
            prepare_init_ble_adv();

            sensebe_sysinfo sysinfo;
            memcpy(&sysinfo.id, &app_scan_rsp_data[5], 16);
            sysinfo.battery_status = 0;
            sysinfo.fw_ver.build = 8;
            sysinfo.fw_ver.major = 0;
            sysinfo.fw_ver.minor = 0;
            senseberx_ble_update_sysinfo(&sysinfo);

            senseberx_ble_update_config(&senseberx_ble_default_config);

            ///Get config from sensebe_cam_trigger and send to the BLE module
        }
        senseberx_ble_adv_start();

        led_ui_type_stop_all(LED_UI_LOOP_SEQ);
        led_ui_loop_start(LED_SEQ_ORANGE_WAVE, LED_UI_MID_PRIORITY);
    }
    break;
    case CONNECTED:
    {
        device_tick_cfg tick_cfg =
            {
                MS_TIMER_TICKS_MS(CONN_FAST_TICK_INTERVAL_MS),
                MS_TIMER_TICKS_MS(CONN_SLOW_TICK_INTERVAL_MS),
                DEVICE_TICK_SAME};
        device_tick_init(&tick_cfg);
        led_ui_type_stop_all(LED_UI_LOOP_SEQ);
        led_ui_loop_start(LED_SEQ_GREEN_WAVE, LED_UI_MID_PRIORITY);

        break;
    }
    }
}

/**
 * @brief Handler for all button related events.
 * @param step The step reached by the button press
 * @param act The action of the button press i.e. if a step
 *  is crossed or the button is released
 */
void button_handler(button_ui_steps step, button_ui_action act)
{
    log_printf("Act (0 = CROSS, 1= RELEASE) : %d\nStep : %d\n", act, step);
    if (act == BUTTON_UI_ACT_CROSS)
    {
        switch (step)
        {
        case BUTTON_UI_STEP_WAKE:
            log_printf("fast\n");
            button_ui_config_wake(false);
            device_tick_cfg tick_cfg =
                {
                    MS_TIMER_TICKS_MS(SENSE_FAST_TICK_INTERVAL_MS),
                    MS_TIMER_TICKS_MS(SENSE_SLOW_TICK_INTERVAL_MS),
                    DEVICE_TICK_FAST};
            device_tick_init(&tick_cfg);
            break;
        case BUTTON_UI_STEP_QUICK:
            if (current_state == SENSING)
            {
                irq_msg_push(MSG_STATE_CHANGE, (void *)ADVERTISING);
            }

            break;
        case BUTTON_UI_STEP_SHORT:
            break;
        case BUTTON_UI_STEP_LONG:
        {
            NRF_POWER->GPREGRET = 0xB1;
            log_printf("Trying to do system reset..!!");
            uint8_t is_sd_enabled;
            sd_softdevice_is_enabled(&is_sd_enabled);
            if (is_sd_enabled == 0)
            {
                sd_nvic_SystemReset();
            }
            else
            {
                NVIC_SystemReset();
            }
        }
        break;
        }
    }
    else //BUTTON_UI_ACT_RELEASE
    {
        device_tick_switch_mode(DEVICE_TICK_SLOW);
        log_printf("slow\n");
        button_ui_config_wake(true);
        switch (step)
        {
        case BUTTON_UI_STEP_WAKE:
            break;
        case BUTTON_UI_STEP_QUICK:
            break;
        case BUTTON_UI_STEP_SHORT:
            break;
        case BUTTON_UI_STEP_LONG:
            break;
        }
    }
}

/**
 * @brief Initialize and blink the LEDs momentarily. To
 *  be used at the start of the program.
 */
void leds_init(void)
{
    hal_gpio_cfg_output(LED_1, LEDS_ACTIVE_STATE);
    hal_gpio_cfg_output(LED_2, !LEDS_ACTIVE_STATE);
    hal_nop_delay_ms(600);
    hal_gpio_pin_write(LED_1, !LEDS_ACTIVE_STATE);
    hal_gpio_pin_write(LED_2, LEDS_ACTIVE_STATE);
    hal_nop_delay_ms(600);
    hal_gpio_pin_write(LED_1, !LEDS_ACTIVE_STATE);
    hal_gpio_pin_write(LED_2, !LEDS_ACTIVE_STATE);
}

/**
 * @brief Prints the reason for the last reset, enables the internal
 *  DC-DC converter if the board supports it and puts the nRF SoC to
 *  the low power mode.
 */
void boot_pwr_config(void)
{
    log_printf("Reset because of ");
    if (NRF_POWER->RESETREAS == 0)
    {
        log_printf("power on or brownout, ");
    }
    if (NRF_POWER->RESETREAS & POWER_RESETREAS_DIF_Msk)
    {
        log_printf("entering into debug interface from Sys OFF, ");
    }
    if (NRF_POWER->RESETREAS & POWER_RESETREAS_DOG_Msk)
    {
        log_printf("watchdog bite, ");
    }
    if (NRF_POWER->RESETREAS & POWER_RESETREAS_LOCKUP_Msk)
    {
        log_printf("CPU lockup, ");
    }
    if (NRF_POWER->RESETREAS & POWER_RESETREAS_OFF_Msk)
    {
        log_printf("wake up from SYS OFF by GPIO, ");
    }
    if (NRF_POWER->RESETREAS & POWER_RESETREAS_RESETPIN_Msk)
    {
        log_printf("pin reset, ");
    }
    if (NRF_POWER->RESETREAS & POWER_RESETREAS_SREQ_Msk)
    {
        log_printf("software reset, ");
    }
    log_printf("\n");

    //Clear the reset reason
    NRF_POWER->RESETREAS = (POWER_RESETREAS_DIF_Msk |
                            POWER_RESETREAS_DOG_Msk |
                            POWER_RESETREAS_LOCKUP_Msk |
                            POWER_RESETREAS_OFF_Msk |
                            POWER_RESETREAS_RESETPIN_Msk |
                            POWER_RESETREAS_SREQ_Msk);

    //Enable the DCDC converter if the board supports it
#if DC_DC_CIRCUITRY == true //Defined in the board header file
    NRF_POWER->DCDCEN = POWER_DCDCEN_DCDCEN_Enabled << POWER_DCDCEN_DCDCEN_Pos;
#endif
    NRF_POWER->TASKS_LOWPWR = 1;
}

/**
 * Different calls to sleep depending on the status of Softdevice
 */
void slumber(void)
{
    uint8_t is_sd_enabled;
    sd_softdevice_is_enabled(&is_sd_enabled);
    // Would in the SENSING mode
    if (is_sd_enabled == 0)
    {
        __WFI();
    }
    else
    {
        sd_app_evt_wait();
    }
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{

    leds_init();

    /* Mandatory welcome message */
    log_init();
    log_printf("\n\nHello SenseBe World!\n");

    log_printf("Structure size = %d \n", sizeof(senseberx_config_t));
    boot_pwr_config();

    lfclk_init(LFCLK_SRC_Xtal);
    ms_timer_init(APP_IRQ_PRIORITY_LOW);

#if ENABLE_WDT == 1
    hal_wdt_init(WDT_PERIOD_MS, wdt_prior_reset_callback);
    hal_wdt_start();
#endif

    int i;
    settings_t setting = {

        //        Camera Settings
        .cam_setting.cam_trigger.mode = NO_SHOT,
        .cam_setting.cam_trigger.mode_setting.long_press_duration_100ms = 1,
        .cam_setting.cam_trigger.pre_focus_en = 1,
        .cam_setting.cam_trigger.prf_pulse_duration_100ms = 1,
        .cam_setting.cam_trigger.radio_trig_en = 1,
        .cam_setting.cam_trigger.trig_pulse_duration_100ms = 1,
        .cam_setting.cam_trigger.video_w_full_press_en = 1,

        .cam_setting.oper_cond.time_cond.start_time = 0xFFFFFFFF,
        .cam_setting.oper_cond.time_cond.end_time = 0xFFFFFFFF,

        //        Sensor Setting
        .func_setting.timer_duration = 15,
        .trig_sel = TIMER_ONLY,

    };

    for (i = 0; i < MAX_SETTINGS; i++)
    {
        memcpy((uint8_t *)(&senseberx_ble_default_config.generic_settings[i]), &setting, sizeof(settings_t));
    }
    
//    get_sensebe_config_t(&senseberx_ble_default_config);

    button_ui_init(BUTTON_PIN, APP_IRQ_PRIORITY_LOW,
                   button_handler);

    {
        irq_msg_callbacks cb =
            {next_interval_handler, state_change_handler};
        irq_msg_init(&cb);
    }

    current_state = ADVERTISING; //So that a state change happens
    irq_msg_push(MSG_STATE_CHANGE, (void *)SENSING);
    senseberx_ble_init(ble_evt_handler, get_sensebe_config_t);

    while (true)
    {
#if ENABLE_WDT == 1
        //        Since the application demands that CPU wakes up
        hal_wdt_feed();
#endif
        device_tick_process();
        irq_msg_process();
        slumber();
    }
}

/** @} */
/** @} */
