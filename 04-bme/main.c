#include "stdio-task/stdio-task.h"
#include "protocol-task.h"
#include "pico/stdlib.h"
#include "stdio.h"
#include "stdlib.h"
#include "led-task/led-task.h"
#include "hardware/i2c.h"
#include "bme280-driver.h"

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

void rp2040_i2c_read(uint8_t* buffer, uint16_t length) {
    i2c_read_timeout_us(i2c1, 0x76, buffer, length, false, 100000);
}

void rp2040_i2c_write(uint8_t* data, uint16_t size) {
    i2c_write_timeout_us(i2c1, 0x76, data, size, false, 100000);
}

void read_regs_callback(const char* args) {
    uint32_t addr = 0;
    uint32_t count = 0;
    
    if (sscanf(args, "%x %x", &addr, &count) != 2) {
        printf("Error: invalid arguments. Usage: read_regs <addr> <count>\n");
        return;
    }

    if (addr > 0xFF) {
        printf("Error: address must be <= 0xFF\n");
        return;
    }
    
    if (count > 0xFF) {
        printf("Error: count must be <= 0xFF\n");
        return;
    }
    
    if (addr + count > 0x100) {
        printf("Error: address + count must be <= 0x100\n");
        return;
    }
    
    uint8_t buffer[256] = {0};
    bme280_read_regs((uint8_t)addr, buffer, (uint8_t)count);
    
    for (int i = 0; i < count; i++) {
        printf("bme280 register [0x%X] = 0x%X\n", addr + i, buffer[i]);
    }
}

void write_reg_callback(const char* args) {
    uint32_t addr = 0;
    uint32_t value = 0;
    
    if (sscanf(args, "%x %x", &addr, &value) != 2) {
        printf("Error: invalid arguments. Usage: write_reg <addr> <value>\n");
        return;
    }
    
    if (addr > 0xFF) {
        printf("Error: address must be <= 0xFF\n");
        return;
    }
    
    if (value > 0xFF) {
        printf("Error: value must be <= 0xFF\n");
        return;
    }
    
    bme280_write_reg((uint8_t)addr, (uint8_t)value);
    printf("Written 0x%02X to register 0x%02X\n", value, addr);
}

void temp_raw_callback(const char* args) {
    uint16_t raw = bme280_read_temp_raw();
    printf("temp_raw: %u (0x%04X)\n", raw, raw);
}

void pres_raw_callback(const char* args) {
    uint16_t raw = bme280_read_pres_raw();
    printf("pres_raw: %u (0x%04X)\n", raw, raw);
}

void hum_raw_callback(const char* args) {
    uint16_t raw = bme280_read_hum_raw();
    printf("hum_raw: %u (0x%04X)\n", raw, raw);
}

void temp_callback(const char* args) {
    float temp = bme280_read_temperature();
    printf("%.2f °C\n", temp);
}

void pres_callback(const char* args) {
    float pres = bme280_read_pressure();
    printf("%.2f Pa\n", pres);
}

void hum_callback(const char* args) {
    float hum = bme280_read_humidity();
    printf("%.2f %%\n", hum);
}

void bme_start_callback(const char* args) {
    bme280_telemetry_start();
    printf("BME280 telemetry started\n");
}

void bme_stop_callback(const char* args) {
    bme280_telemetry_stop();
    printf("BME280 telemetry stopped\n");
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
    {"read_regs", read_regs_callback, "read BME280 registers: read_regs <addr> <count>"},
    {"write_reg", write_reg_callback, "write BME280 register: write_reg <addr> <value>"},
    {"temp_raw", temp_raw_callback, "read raw temperature value from BME280"},
    {"pres_raw", pres_raw_callback, "read raw pressure value from BME280"},
    {"hum_raw", hum_raw_callback, "read raw humidity value from BME280"},
    {"temp", temp_callback, "read temperature in °C"},
    {"pres", pres_callback, "read pressure in hPa"},
    {"hum", hum_callback, "read humidity in %"},
    {"bme_start", bme_start_callback, "start BME280 telemetry (continuous output)"},
    {"bme_stop", bme_stop_callback, "stop BME280 telemetry"},
    {NULL, NULL, NULL},
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
    stdio_init_all();
    stdio_task_init();
    led_task_init();
    protocol_task_init(device_api);
    i2c_init(i2c1, 100000);

    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);

    bme280_init(rp2040_i2c_read, rp2040_i2c_write);

    while (true) {
        char* command_string = stdio_task_handle();
        protocol_task_handle(command_string);
        led_task_handle();
        
        bme280_telemetry_handler();
    }
}
