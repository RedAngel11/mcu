#pragma once
#include <stdint.h>

typedef enum {
    ADC_TASK_STATE_IDLE = 0,
    ADC_TASK_STATE_RUN = 1,
} adc_task_state_t;

void adc_task_init();
float adc_task_measure_voltage();
float adc_task_measure_temperature();

void adc_task_handle();
void adc_task_set_state(adc_task_state_t state);