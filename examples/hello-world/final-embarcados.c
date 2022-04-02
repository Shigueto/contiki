
#include "contiki.h"
#include "gpio.h"
#include "button-sensor.h"
#include "dev/adc-sensor.h"
#include "platform/srf06-cc26xx/launchpad/cc2650/board.h"
#include "lpm.h"
#include "ti-lib.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"

#include <stdio.h> /* For printf() */

#define EVENT_BUTTON      (44)

#define LED1 BOARD_IOID_DIO25
#define LED2 BOARD_IOID_DIO26
#define LED3 BOARD_IOID_DIO27
#define LED4 BOARD_IOID_DIO28

#define ADC_IN ADC_COMPB_IN_AUXIO7

#define PWM_OUTPUT IOID_21

#define MAX_ADC_OUTPUT_VALUE    15
#define MAX_ADC_INPUT_VALUE     3320288

#define BUTTON  BOARD_IOID_DIO30

PROCESS(init, "init");
PROCESS(read_adc, "Leitura de input de ADC");
PROCESS(read_gpio_button, "Leitura do botão GPIO");

AUTOSTART_PROCESSES(&init, &read_adc, &read_gpio_button);

static struct etimer et_read;
static struct etimer et_press;

void set_leds(uint16_t value) {
    GPIO_writeDio(LED1, (value&0x1)==1);
    GPIO_writeDio(LED2, (value&0x2)==2);
    GPIO_writeDio(LED3, (value&0x4)==4);
    GPIO_writeDio(LED4, (value&0x8)==8);
}

void set_pwm(uint32_t loadvalue, uint16_t value) {
    uint16_t current_duty = value*100/MAX_ADC_OUTPUT_VALUE;
    uint32_t ticks = (current_duty * loadvalue) / 100;
    if (ticks == 0) {
        ticks = 1;
    }
    ti_lib_timer_match_set(GPT0_BASE, TIMER_A, loadvalue - ticks);
}

int16_t convert_data_to_value(uint32_t data) {
    if (data < 5000) {
        data = 0;
    }
    uint16_t result = (MAX_ADC_INPUT_VALUE - data)*MAX_ADC_OUTPUT_VALUE/MAX_ADC_INPUT_VALUE;
    return result == 1293 ? 0 : result;
}

uint8_t pwm_request_max_pm(void) {
    return LPM_MODE_DEEP_SLEEP;
}

void sleep_enter(void) {
    leds_on(LEDS_RED);
}

void sleep_leave(void) {
    leds_off(LEDS_RED);
}

static struct lpm_registered_module pwmdrive_module;
LPM_MODULE(pwmdrive_module, pwm_request_max_pm,
           sleep_enter, sleep_leave, LPM_DOMAIN_PERIPH);

int32_t pwminit(int32_t freq)
{
    uint32_t load = 0;

    ti_lib_ioc_pin_type_gpio_output(PWM_OUTPUT);
    leds_off(LEDS_RED);

    /* Enable GPT0 clocks under active mode */
    ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_TIMER0);
    ti_lib_prcm_load_set();
    while(!ti_lib_prcm_load_get());

    /* Drive the I/O ID with GPT0 / Timer A */
    ti_lib_ioc_port_configure_set(PWM_OUTPUT, IOC_PORT_MCU_PORT_EVENT0,
    IOC_STD_OUTPUT);

    /* GPT0 / Timer A: PWM, Interrupt Enable */
    ti_lib_timer_configure(GPT0_BASE,
    TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM | TIMER_CFG_B_PWM);

    /* Stop the timers */
    ti_lib_timer_disable(GPT0_BASE, TIMER_A);
    ti_lib_timer_disable(GPT0_BASE, TIMER_B);

    if(freq > 0) {
        load = (GET_MCU_CLOCK / freq);
        ti_lib_timer_load_set(GPT0_BASE, TIMER_A, load);
        ti_lib_timer_match_set(GPT0_BASE, TIMER_A, load-1);
        /* Start */
        ti_lib_timer_enable(GPT0_BASE, TIMER_A);
    }

    /* Enable GPT0 clocks under active, sleep, deep sleep */
    ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_TIMER0);
    ti_lib_prcm_peripheral_sleep_enable(PRCM_PERIPH_TIMER0);
    ti_lib_prcm_peripheral_deep_sleep_enable(PRCM_PERIPH_TIMER0);
    ti_lib_prcm_load_set();
    while(!ti_lib_prcm_load_get());
    /* Register with LPM. This will keep the PERIPH PD powered on
    * during deep sleep, allowing the pwm to keep working while the chip is
    * being power-cycled */
    lpm_register_module(&pwmdrive_module);

    return load;
}

PROCESS_THREAD(init, ev, data)
{
    PROCESS_BEGIN();
    GPIO_setOutputEnableDio(BUTTON, GPIO_OUTPUT_DISABLE);
    GPIO_setOutputEnableDio(LED1, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED2, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED3, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED4, GPIO_OUTPUT_ENABLE);
    GPIO_clearMultiDio(LED1|LED2|LED3|LED4);
    PROCESS_END();
}

PROCESS_THREAD(read_gpio_button, ev, data)
{
    PROCESS_BEGIN();
    static uint16_t pressed_counter = 0;

    etimer_set(&et_press, CLOCK_SECOND/100);
    while (1) {
        PROCESS_YIELD();
        if (ev == PROCESS_EVENT_TIMER) {
            if(pressed_counter >= 20){
                pressed_counter=0;
                process_post(&read_adc, EVENT_BUTTON, NULL);
            }

            if (GPIO_readDio(BUTTON) == 0) {
                pressed_counter++;
            }
            etimer_reset(&et_press);
        }
    }
    PROCESS_END();
    return 0;
}

PROCESS_THREAD(read_adc, ev, data)
{
    static struct sensors_sensor *sensor;
    static uint32_t loadvalue;
    static uint16_t value = 0;
    static uint8_t isPwm = 0;

    PROCESS_BEGIN();
    loadvalue = pwminit(5000);

    sensor = (struct sensors_sensor *) sensors_find(ADC_SENSOR);
    etimer_set(&et_read, CLOCK_SECOND/2);

    printf("start reading...\n");
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER) {
            SENSORS_ACTIVATE(*sensor);
            sensor->configure(ADC_SENSOR_SET_CHANNEL, ADC_IN);
            value = convert_data_to_value(sensor->value(ADC_SENSOR_VALUE));
            printf("read value=%d\n", value);
            if (isPwm == 1) {
                set_pwm(loadvalue, value);
            } else {
                set_leds(value);
            }
            SENSORS_DEACTIVATE(*sensor);
            etimer_reset(&et_read);
        } else if (ev == EVENT_BUTTON) {
            isPwm = !isPwm;
            printf("button pressed, isPwm=%d", isPwm);
            if (isPwm == 1) {
                set_leds(0);
                set_pwm(loadvalue, value);
            } else {
                set_leds(value);
                set_pwm(loadvalue, 0);
            }
        }
    }
    PROCESS_END();
}

