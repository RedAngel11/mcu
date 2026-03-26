#include "stdio-task.h"
#include "protocol-task.h"
#include "pico/stdlib.h"
#include "stdio.h"
#include "stdlib.h"
#include "led-task.h"
#include "hardware/i2c.h"
#include "bme280-driver.h"
#include "hardware/spi.h"
#include "ili9341-driver.h"
#include "ili9341-display.h"
#include "font-jetbrains.h"
#include "bme280-driver.h"

#define DEVICE_NAME "my-pico-device"
#define DEVICE_VRSN "v0.0.1"

#define ILI9341_PIN_MISO 4
#define ILI9341_PIN_CS 10
#define ILI9341_PIN_SCK 6
#define ILI9341_PIN_MOSI 7
#define ILI9341_PIN_DC 8
#define ILI9341_PIN_RESET 9

// === Глобальные данные для GUI ===
#define HISTORY_SIZE 60
static float temp_history[HISTORY_SIZE] = {0};
static uint8_t history_index = 0;
static uint32_t g_measure_period_ms = 1000; // Период по умолчанию 1 сек
static uint32_t g_last_measure_time = 0;
static float g_last_temp = 0.0f;
static float g_last_pres = 0.0f;
static float g_last_hum = 0.0f;
//-----------------------------------

static ili9341_display_t ili9341_display = {0};

void rp2040_spi_write(const uint8_t* data, uint32_t size) {
    spi_write_blocking(spi0, data, size);
}

void rp2040_spi_read(uint8_t* buffer, uint32_t length) {
    spi_read_blocking(spi0, 0, buffer, length);
}

void rp2040_gpio_cs_write(bool level) {
    gpio_put(ILI9341_PIN_CS, level);
}

void rp2040_gpio_dc_write(bool level) {
    gpio_put(ILI9341_PIN_DC, level);
}

void rp2040_gpio_reset_write(bool level) {
    gpio_put(ILI9341_PIN_RESET, level);
}

void rp2040_delay_ms(uint32_t ms) {
    sleep_ms(ms);
}


uint16_t parse_color(const char* color_str) {
    uint32_t c = 0;
    if (sscanf(color_str, "%x", &c) == 1) {
        return RGB888_2_RGB565(c);
    }
    return COLOR_WHITE;
}

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


void disp_screen_callback(const char* args)
{
	uint32_t c = 0;
	int result = sscanf(args, "%x", &c);
	
	uint16_t color = COLOR_BLACK;
	
	if (result == 1)
	{
		color = RGB888_2_RGB565(c);
	}
	
	ili9341_fill_screen(&ili9341_display, color);
}

void disp_px_callback(const char* args) {
    uint16_t x, y;
    uint32_t color_val = 0;
    
    int result = sscanf(args, "%hu %hu %x", &x, &y, &color_val);
    
    if (result >= 2) {
        uint16_t color = (result == 3) ? RGB888_2_RGB565(color_val) : COLOR_WHITE;
        ili9341_draw_pixel(&ili9341_display, x, y, color);
        printf("Pixel drawn at (%d, %d) with color 0x%06X\n", x, y, color_val);
    } else {
        printf("Error: invalid arguments. Usage: disp_px <x> <y> [color]\n");
    }
}

void disp_line_callback(const char* args) {
    uint16_t x0, y0, x1, y1;
    uint32_t color_val = 0;
    
    int result = sscanf(args, "%hu %hu %hu %hu %x", &x0, &y0, &x1, &y1, &color_val);
    
    if (result >= 4) {
        uint16_t color = (result == 5) ? RGB888_2_RGB565(color_val) : COLOR_WHITE;
        ili9341_draw_line(&ili9341_display, x0, y0, x1, y1, color);
        printf("Line drawn from (%d,%d) to (%d,%d) with color 0x%06X\n", 
               x0, y0, x1, y1, color_val);
    } else {
        printf("Error: invalid arguments. Usage: disp_line <x0> <y0> <x1> <y1> [color]\n");
    }
}

void disp_rect_callback(const char* args) {
    uint16_t x, y, w, h;
    uint32_t color_val = 0;
    
    int result = sscanf(args, "%hu %hu %hu %hu %x", &x, &y, &w, &h, &color_val);
    
    if (result >= 4) {
        uint16_t color = (result == 5) ? RGB888_2_RGB565(color_val) : COLOR_WHITE;
        ili9341_draw_rect(&ili9341_display, x, y, w, h, color);
        printf("Rectangle drawn at (%d,%d) size %dx%d with color 0x%06X\n", 
               x, y, w, h, color_val);
    } else {
        printf("Error: invalid arguments. Usage: disp_rect <x> <y> <w> <h> [color]\n");
    }
}

void disp_frect_callback(const char* args) {
    uint16_t x, y, w, h;
    uint32_t color_val = 0;
    
    int result = sscanf(args, "%hu %hu %hu %hu %x", &x, &y, &w, &h, &color_val);
    
    if (result >= 4) {
        uint16_t color = (result == 5) ? RGB888_2_RGB565(color_val) : COLOR_WHITE;
        ili9341_draw_filled_rect(&ili9341_display, x, y, w, h, color);
        printf("Filled rectangle drawn at (%d,%d) size %dx%d with color 0x%06X\n", 
               x, y, w, h, color_val);
    } else {
        printf("Error: invalid arguments. Usage: disp_frect <x> <y> <w> <h> [color]\n");
    }
}

void disp_text_callback(const char* args) {
    char text[64] = {0};
    uint16_t x, y;
    uint32_t fg_color_val = 0;
    uint32_t bg_color_val = 0;
    
    int result = sscanf(args, "%hu %hu %63s %x %x", &x, &y, text, &fg_color_val, &bg_color_val);
    
    if (result >= 3) {
        uint16_t fg_color = (result >= 4) ? RGB888_2_RGB565(fg_color_val) : COLOR_WHITE;
        uint16_t bg_color = (result >= 5) ? RGB888_2_RGB565(bg_color_val) : COLOR_BLACK;
        
        ili9341_draw_text(&ili9341_display, x, y, text, 
                          &jetbrains_font, fg_color, bg_color);
        printf("Text '%s' drawn at (%d,%d) with colors FG:0x%06X BG:0x%06X\n", 
               text, x, y, fg_color_val, bg_color_val);
    } else {
        printf("Error: invalid arguments. Usage: disp_text <x> <y> <text> [fg_color] [bg_color]\n");
    }
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
//------------------------------
// === Отрисовка данных датчиков ===
void gui_draw_sensor_data() {
    char buf[32];
    
    // Температура (красный блок)
    ili9341_draw_filled_rect(&ili9341_display, 10, 10, 100, 60, COLOR_RED);
    sprintf(buf, "%.1f C", g_last_temp);
    ili9341_draw_text(&ili9341_display, 20, 35, buf, &jetbrains_font, COLOR_WHITE, COLOR_RED);
    
    // Давление (зеленый блок)
    ili9341_draw_filled_rect(&ili9341_display, 120, 10, 100, 60, COLOR_GREEN);
    sprintf(buf, "%.0f hPa", g_last_pres);
    ili9341_draw_text(&ili9341_display, 130, 35, buf, &jetbrains_font, COLOR_WHITE, COLOR_GREEN);
    
    // Влажность (синий блок)
    ili9341_draw_filled_rect(&ili9341_display, 230, 10, 80, 60, COLOR_BLUE);
    sprintf(buf, "%.0f %%", g_last_hum);
    ili9341_draw_text(&ili9341_display, 240, 35, buf, &jetbrains_font, COLOR_WHITE, COLOR_BLUE);
}

// === Отрисовка графика температуры ===
void gui_draw_graph() {
    // Очистка области графика
    ili9341_draw_filled_rect(&ili9341_display, 10, 90, 300, 140, COLOR_BLACK);
    ili9341_draw_rect(&ili9341_display, 10, 90, 300, 140, COLOR_WHITE);
    
    // Рисуем линии графика
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        int x1 = 10 + i * 5;
        int y1 = 230 - (int)(temp_history[i] * 3); // Масштаб: 1°C = 3 пикселя
        int x2 = 10 + (i + 1) * 5;
        int y2 = 230 - (int)(temp_history[i + 1] * 3);
        
        ili9341_draw_line(&ili9341_display, x1, y1, x2, y2, COLOR_YELLOW);
    }
}

// === Добавление точки в историю ===
void gui_add_history_point(float temp) {
    temp_history[history_index] = temp;
    history_index = (history_index + 1) % HISTORY_SIZE;
}

// Обработчик команды
void set_period_callback(const char* args) {
    uint32_t period = 0;
    if (sscanf(args, "%u", &period) == 1 && period > 0) {
        g_measure_period_ms = period;
        printf("Measurement period set to %u ms\n", period);
    } else {
        printf("Error: Usage: set_period <ms>\n");
    }
}
//-----------------------------------
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
        {"disp_screen", disp_screen_callback, "fill screen with color (disp_screen [RRGGBB])"},
    {"disp_px", disp_px_callback, "draw pixel: disp_px <x> <y> [RRGGBB]"},
    {"disp_line", disp_line_callback, "draw line: disp_line <x0> <y0> <x1> <y1> [RRGGBB]"},
    {"disp_rect", disp_rect_callback, "draw rectangle: disp_rect <x> <y> <w> <h> [RRGGBB]"},
    {"disp_frect", disp_frect_callback, "draw filled rectangle: disp_frect <x> <y> <w> <h> [RRGGBB]"},
    {"disp_text", disp_text_callback, "draw text: disp_text <x> <y> <text> [fg_color] [bg_color]"},
    {"set_period", set_period_callback, "set measurement period in ms"},
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

    spi_init(spi0, 62500000);

    gpio_set_function(ILI9341_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(ILI9341_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(ILI9341_PIN_SCK, GPIO_FUNC_SPI);

    gpio_init(ILI9341_PIN_CS);
    gpio_set_dir(ILI9341_PIN_CS, GPIO_OUT);
    
    gpio_init(ILI9341_PIN_DC);
    gpio_set_dir(ILI9341_PIN_DC, GPIO_OUT);
    
    gpio_init(ILI9341_PIN_RESET);
    gpio_set_dir(ILI9341_PIN_RESET, GPIO_OUT);
    
    gpio_put(ILI9341_PIN_CS, 1);
    gpio_put(ILI9341_PIN_DC, 0);
    gpio_put(ILI9341_PIN_RESET, 0);

    ili9341_hal_t ili9341_hal = {0};
    ili9341_hal.spi_write = rp2040_spi_write;
    ili9341_hal.spi_read = rp2040_spi_read;
    ili9341_hal.gpio_cs_write = rp2040_gpio_cs_write;
    ili9341_hal.gpio_dc_write = rp2040_gpio_dc_write;
    ili9341_hal.gpio_reset_write = rp2040_gpio_reset_write;
    ili9341_hal.delay_ms = rp2040_delay_ms;

    ili9341_init(&ili9341_display, &ili9341_hal);

    ili9341_set_rotation(&ili9341_display, ILI9341_ROTATION_90);

    ili9341_fill_screen(&ili9341_display, COLOR_BLACK);
    sleep_ms(300);
    
    /* 2. Coloured rectangles */
    ili9341_draw_filled_rect(&ili9341_display, 10, 10, 100, 60, COLOR_RED);
    ili9341_draw_filled_rect(&ili9341_display, 120, 10, 100, 60, COLOR_GREEN);
    ili9341_draw_filled_rect(&ili9341_display, 230, 10, 80, 60, COLOR_BLUE);


    ili9341_draw_rect(&ili9341_display, 10, 90, 300, 80, COLOR_WHITE);

    ili9341_draw_line(&ili9341_display, 0, 0, 319, 239, COLOR_YELLOW);
    ili9341_draw_line(&ili9341_display, 319, 0, 0, 239, COLOR_CYAN);
    
    ili9341_draw_text(&ili9341_display, 20, 100, "Hello, ILI9341!", &jetbrains_font, COLOR_WHITE, COLOR_BLACK);

    ili9341_draw_text(&ili9341_display, 20, 116, "RP2040 / Pico SDK", &jetbrains_font, COLOR_YELLOW, COLOR_BLACK);
//--------------------------    
    g_last_measure_time = to_ms_since_boot(get_absolute_time());
    gui_draw_sensor_data(); // Нарисовать начальные данные
    gui_draw_graph();       // Нарисовать пустой график

    while (true) {
        // 1. Обработка UART команд
        char* command_string = stdio_task_handle();
        protocol_task_handle(command_string);
        
        // 2. Задача светодиода
        led_task_handle();
        
        // 3. Измерение по таймеру
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - g_last_measure_time >= g_measure_period_ms) {
            // Чтение датчика
            g_last_temp = bme280_read_temperature();
            g_last_pres = bme280_read_pressure();
            g_last_hum = bme280_read_humidity();
            
            // Обновление GUI
            gui_draw_sensor_data();
            gui_add_history_point(g_last_temp);
            gui_draw_graph();
            
            g_last_measure_time = now;
        }
        
        sleep_ms(10); // Небольшая задержка, чтобы не грузить процессор
        }
//----------------------------------
    // while (true) {
    //     char* command_string = stdio_task_handle();
    //     protocol_task_handle(command_string);
    //     led_task_handle();
        
    //     bme280_telemetry_handler();
    // }
}
