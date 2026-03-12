#include "adc-task.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

const uint ADC_GPIO_PIN = 26;
const uint ADC_CHANNEL = 0;
const uint ADC_TEMP_PIN = 4;

const uint64_t ADC_TASK_MEAS_PERIOD_US = 100000;

static adc_task_state_t adc_state = ADC_TASK_STATE_IDLE;
static uint64_t last_measure_ts = 0;

void adc_task_init() {
    adc_init();
    adc_gpio_init(ADC_GPIO_PIN);
    adc_set_temp_sensor_enabled(true);
}

float adc_task_measure_voltage() {
    adc_select_input(ADC_CHANNEL);
    uint16_t voltage_counts = adc_read();
    
    float voltage = voltage_counts * 3.3f / 4096.0f; //3.3В и 4096 шагов
    
    return voltage;
}

float adc_task_measure_temperature() {
    adc_select_input(ADC_TEMP_PIN);

    uint16_t voltage_counts = adc_read();

    float voltage = voltage_counts * 3.3f / 4096.0f;

    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f; //взята из даташита

    return temperature;
}

void adc_task_set_state(adc_task_state_t state) {
    adc_state = state;
    if (state == ADC_TASK_STATE_RUN) {
        last_measure_ts = time_us_64();
    }
}

void adc_task_handle() {
    if (adc_state != ADC_TASK_STATE_RUN) {
        return;
    }
    
    uint64_t now = time_us_64();
    if (now >= last_measure_ts + ADC_TASK_MEAS_PERIOD_US) {
        last_measure_ts = now;
        
        float voltage = adc_task_measure_voltage();
        float temperature = adc_task_measure_temperature();
        
        printf("%f %f\n", voltage, temperature);
    }
}
