#include "stdio-task/stdio-task.h"
#include "protocol-task/protocol-task.h"
#include "pico/stdlib.h"
#include "stdio.h"
#include "stdlib.h"
#include "led-task/led-task.h"
#include "adc-task/adc-task.h"

#define DEVICE_NAME "my-pico-device"
#define DEVICE_VRSN "v0.0.1"

void version_callback(const char* args)
{
	printf("device name: '%s', firmware version: %s\n", DEVICE_NAME, DEVICE_VRSN);
}

void led_on_callback(const char* args) {
    led_task_state_set(LED_STATE_ON);
    printf("LED turned ON\n");
}

void led_off_callback(const char* args) {
    led_task_state_set(LED_STATE_OFF);
    printf("LED turned OFF\n");
}

void led_blink_callback(const char* args) {
    led_task_state_set(LED_STATE_BLINK);
    printf("LED blinking started\n");
}

void led_blink_set_period_ms_callback(const char* args) {
    uint period_ms = 0;
    sscanf(args, "%u", &period_ms);
    
    if (period_ms == 0) {
        printf("Error: period cannot be zero\n");
        return;
    }
    
    led_task_set_blink_period_ms(period_ms);
    printf("LED blink period set to %u ms\n", period_ms);
}

void help_callback(const char* args);

void mem_callback(const char* args) {
    uint32_t addr = 0;
    
    if (sscanf(args, "%x", &addr) != 1) {
        printf("Error: invalid address format. Usage: mem <addr>\n");
        return;
    }
    
    volatile uint32_t* ptr = (volatile uint32_t*)addr;
    uint32_t value = *ptr;
    
    printf("Memory at 0x%08X: 0x%08X (dec: %u)\n", addr, value, value);
}

void wmem_callback(const char* args) {
    uint32_t addr = 0;
    uint32_t value = 0;
    
    if (sscanf(args, "%x %x", &addr, &value) != 2) {
        printf("Error: invalid arguments. Usage: wmem <addr> <value>\n");
        return;
    }
    
    volatile uint32_t* ptr = (volatile uint32_t*)addr;
    *ptr = value;
    
    printf("Written 0x%08X to address 0x%08X\n", value, addr);
}

void get_adc_callback(const char* args) {
    float voltage = adc_task_measure_voltage();
    printf("%f\n", voltage);
}

void get_temp_callback(const char* args) {
    float temperature = adc_task_measure_temperature();
    printf("%f\n", temperature  );
}

void tm_start_callback(const char* args) {
    adc_task_set_state(ADC_TASK_STATE_RUN);
    printf("Telemetry started\n");
}

void tm_stop_callback(const char* args) {
    adc_task_set_state(ADC_TASK_STATE_IDLE);
    printf("Telemetry stopped\n");
}

api_t device_api[] = {
    {"help", help_callback, "show this help message"},
    {"version", version_callback, "get device name and firmware version"},
    {"on", led_on_callback, "turn LED on"},
    {"off", led_off_callback, "turn LED off"},
    {"blink", led_blink_callback, "make LED blink"},
    {"led_blink_set_period_ms", led_blink_set_period_ms_callback, "set blink period in milliseconds"},
    {"mem", mem_callback, "read memory word at address (mem <addr>)"},
    {"wmem", wmem_callback, "write memory word at address (wmem <addr> <value>)"},
    {"get_adc", get_adc_callback, "measure voltage on GPIO 26"},
    {"get_temp", get_temp_callback, "measure internal temperature"},
    {"tm_start", tm_start_callback, "start telemetry (continuous measurements)"},
    {"tm_stop", tm_stop_callback, "stop telemetry"},
    {NULL, NULL, NULL}
};

void help_callback(const char* args) {
    printf("\nAvailable commands:\n");
    
    for (int i = 0; device_api[i].command_name != NULL; i++) {
        printf("  %-20s - %s\n", device_api[i].command_name, device_api[i].command_help);
    }
    printf("\n");
}


int main()
{
    adc_task_init();
    stdio_init_all();
    stdio_task_init();
    led_task_init();
    protocol_task_init(device_api);

    while (true) {
        char* command_string = stdio_task_handle();
        protocol_task_handle(command_string);
        led_task_handle();
        adc_task_handle();
    }
}
