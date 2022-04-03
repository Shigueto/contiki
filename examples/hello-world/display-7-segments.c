
#include "contiki.h"
#include "gpio.h"
#include "dev/adc-sensor.h"
#include "platform/srf06-cc26xx/launchpad/cc2650/board.h"
#include "ti-lib.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"

#include <stdio.h> /* For printf() */

#define LED_A_MASK  GPIO_DIO_25_MASK
#define LED_B_MASK  GPIO_DIO_26_MASK
#define LED_C_MASK  GPIO_DIO_27_MASK
#define LED_D_MASK  GPIO_DIO_28_MASK
#define LED_E_MASK  GPIO_DIO_29_MASK
#define LED_F_MASK  GPIO_DIO_30_MASK
#define LED_G_MASK  GPIO_DIO_21_MASK
#define LED_H_MASK  GPIO_DIO_22_MASK

#define LEDS_MASK LED_A_MASK|LED_B_MASK|LED_C_MASK|LED_D_MASK|LED_E_MASK|LED_F_MASK|LED_G_MASK|LED_H_MASK

#define LED_A BOARD_IOID_DIO25
#define LED_B BOARD_IOID_DIO26
#define LED_C BOARD_IOID_DIO27
#define LED_D BOARD_IOID_DIO28
#define LED_E BOARD_IOID_DIO29
#define LED_F BOARD_IOID_DIO30
#define LED_G BOARD_IOID_DIO21
#define LED_H BOARD_IOID_DIO22

#define ADC_IN ADC_COMPB_IN_AUXIO7

#define MAX_ADC_OUTPUT_VALUE    15
#define MAX_ADC_INPUT_VALUE     3320288

PROCESS(init, "init");
PROCESS(read_adc, "Leitura de input de ADC");

AUTOSTART_PROCESSES(&init, &read_adc);

static struct etimer et_adc_read;

static uint32_t values[16] = {
                             LED_G_MASK, // 0
                             LED_A_MASK|LED_D_MASK|LED_E_MASK|LED_F_MASK|LED_G_MASK, // 1
                             LED_C_MASK|LED_F_MASK, // 2
                             LED_E_MASK|LED_F_MASK, // 3
                             LED_A_MASK|LED_D_MASK|LED_E_MASK, // 4
                             LED_B_MASK|LED_E_MASK, // 5
                             LED_B_MASK, // 6
                             LED_D_MASK|LED_E_MASK|LED_F_MASK|LED_G_MASK, // 7
                             0, // 8
                             LED_E_MASK, // 9
                             LED_D_MASK|LED_H_MASK, // A
                             0|LED_H_MASK, // B
                             LED_B_MASK|LED_C_MASK|LED_G_MASK|LED_H_MASK, // C
                             LED_G_MASK|LED_H_MASK, // D
                             LED_B_MASK|LED_C_MASK|LED_H_MASK, // E
                             LED_B_MASK|LED_C_MASK|LED_D_MASK|LED_H_MASK // F
};

void set_leds(uint16_t value) {
    GPIO_writeMultiDio(LEDS_MASK, values[value]);
}

int16_t convert_data_to_value(uint32_t data) {
    if (data < 5000) {
        data = 0;
    }
    uint16_t result = (MAX_ADC_INPUT_VALUE - data)*MAX_ADC_OUTPUT_VALUE/MAX_ADC_INPUT_VALUE;
    return result == 1293 ? 0 : result;
}

PROCESS_THREAD(init, ev, data)
{
    PROCESS_BEGIN();
    GPIO_setOutputEnableDio(LED_A, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED_B, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED_C, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED_D, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED_E, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED_F, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED_G, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED_H, GPIO_OUTPUT_ENABLE);
    GPIO_setMultiDio(LEDS_MASK);
    PROCESS_END();
}

PROCESS_THREAD(read_adc, ev, data)
{
    static struct sensors_sensor *sensor;
    static uint16_t value = 0;

    PROCESS_BEGIN();

    sensor = (struct sensors_sensor *) sensors_find(ADC_SENSOR);
    etimer_set(&et_adc_read, CLOCK_SECOND/2);

    printf("start reading...\n");
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER) {
            SENSORS_ACTIVATE(*sensor);
            sensor->configure(ADC_SENSOR_SET_CHANNEL, ADC_IN);
            value = convert_data_to_value(sensor->value(ADC_SENSOR_VALUE));
            printf("read value=%d\n", value);
            set_leds(value);
            SENSORS_DEACTIVATE(*sensor);
            etimer_reset(&et_adc_read);
        }
    }
    PROCESS_END();
}

