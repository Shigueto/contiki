
#include "contiki.h"
#include "gpio.h"
#include "button-sensor.h"
#include "dev/adc-sensor.h"
#include "platform/srf06-cc26xx/launchpad/cc2650/board.h"

#include <stdio.h> /* For printf() */

#define LED1 BOARD_IOID_DIO25
#define LED2 BOARD_IOID_DIO26
#define LED3 BOARD_IOID_DIO27
#define LED4 BOARD_IOID_DIO28

#define ADC_IN ADC_COMPB_IN_AUXIO7

#define MAX_ADC_OUTPUT_VALUE    15
#define MAX_ADC_INPUT_VALUE     3320288

PROCESS(init, "init");
PROCESS(read_adc, "Leitura de input de ADC");

AUTOSTART_PROCESSES(&init, &read_adc);


PROCESS_THREAD(init, ev, data)
{
    PROCESS_BEGIN();
    GPIO_setOutputEnableDio(LED1, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED2, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED3, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(LED4, GPIO_OUTPUT_ENABLE);
    GPIO_clearMultiDio(LED1|LED2|LED3|LED4);
    PROCESS_END();
}

void set_leds(uint16_t value) {
    GPIO_writeDio(LED1, (value&0x1)==1);
    GPIO_writeDio(LED2, (value&0x2)==2);
    GPIO_writeDio(LED3, (value&0x4)==4);
    GPIO_writeDio(LED4, (value&0x8)==8);
}

static struct etimer et_read;

uint16_t convert_data_to_value(uint32_t data) {
    if (data < 5000) {
        data = 0;
    }
    uint16_t result = (MAX_ADC_INPUT_VALUE - data)*MAX_ADC_OUTPUT_VALUE/MAX_ADC_INPUT_VALUE;
    return result == 1293 ? 0 : result;
}

PROCESS_THREAD(read_adc, ev, data)
{
    static struct sensors_sensor *sensor;

    PROCESS_BEGIN();
    sensor = (struct sensors_sensor *) sensors_find(ADC_SENSOR);
    etimer_set(&et_read, CLOCK_SECOND/2);

    printf("start reading...\n");
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER) {
            SENSORS_ACTIVATE(*sensor);
            sensor->configure(ADC_SENSOR_SET_CHANNEL, ADC_IN);
            uint16_t value = convert_data_to_value(sensor->value(ADC_SENSOR_VALUE));
            printf("read value=%d\n", value);
            set_leds(value);
            SENSORS_DEACTIVATE(*sensor);
            etimer_reset(&et_read);
        }
    }
    PROCESS_END();
}
