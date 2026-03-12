#include "bme280-driver.h"
#include "bme280-regs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"

typedef struct
{
	bme280_i2c_read i2c_read;
	bme280_i2c_write i2c_write;
} bme280_ctx_t;

static bme280_ctx_t bme280_ctx = {0};

static struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} bme280_calib = {0};

static int32_t t_fine = 0;

static bool bme280_telemetry_running = false;
static uint64_t bme280_last_measure_us = 0;
#define BME280_TELEMETRY_PERIOD_US 500000  // 500 ms

static void bme280_read_calibration_data() {
    uint8_t buffer[24] = {0};
    
    bme280_read_regs(0x88, buffer, 24);
    
    bme280_calib.dig_T1 = (uint16_t)(buffer[1] << 8) | buffer[0];
    bme280_calib.dig_T2 = (int16_t)(buffer[3] << 8) | buffer[2];
    bme280_calib.dig_T3 = (int16_t)(buffer[5] << 8) | buffer[4];
    
    bme280_calib.dig_P1 = (uint16_t)(buffer[7] << 8) | buffer[6];
    bme280_calib.dig_P2 = (int16_t)(buffer[9] << 8) | buffer[8];
    bme280_calib.dig_P3 = (int16_t)(buffer[11] << 8) | buffer[10];
    bme280_calib.dig_P4 = (int16_t)(buffer[13] << 8) | buffer[12];
    bme280_calib.dig_P5 = (int16_t)(buffer[15] << 8) | buffer[14];
    bme280_calib.dig_P6 = (int16_t)(buffer[17] << 8) | buffer[16];
    bme280_calib.dig_P7 = (int16_t)(buffer[19] << 8) | buffer[18];
    bme280_calib.dig_P8 = (int16_t)(buffer[21] << 8) | buffer[20];
    bme280_calib.dig_P9 = (int16_t)(buffer[23] << 8) | buffer[22];
    
    uint8_t hbuf[7] = {0};
    bme280_read_regs(0xE1, hbuf, 7);
    
    bme280_calib.dig_H1 = buffer[24];
    
    bme280_calib.dig_H2 = (int16_t)(hbuf[1] << 8) | hbuf[0];
    bme280_calib.dig_H3 = hbuf[2];
    bme280_calib.dig_H4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
    bme280_calib.dig_H5 = (int16_t)((hbuf[5] << 4) | (hbuf[4] >> 4));
    bme280_calib.dig_H6 = (int8_t)hbuf[6];
}

void bme280_init(bme280_i2c_read i2c_read, bme280_i2c_write i2c_write)
{
    bme280_ctx.i2c_read = i2c_read;
    bme280_ctx.i2c_write = i2c_write;

    uint8_t id_reg_buf[1] = {0};
    bme280_read_regs(BME280_REG_id, id_reg_buf, sizeof(id_reg_buf));

    if (id_reg_buf[0] != 0x60) {
        printf("BME280 ERROR: Invalid device ID (expected 0x60, got 0x%02X)\n", id_reg_buf[0]);
    }
    printf("BME280 detected (ID: 0x60)\n");

    bme280_read_calibration_data();

    uint8_t ctrl_hum_reg_value = 0;
    ctrl_hum_reg_value |= (0b001 << 0);  // osrs_h[2:0] = oversampling 1
    bme280_write_reg(BME280_REG_ctrl_hum, ctrl_hum_reg_value);

    uint8_t config_reg_value = 0;
    config_reg_value |= (0b0 << 0); // spi3w_en[0:0] = false
    config_reg_value |= (0b000 << 2); // filter[4:2] = Filter off
    config_reg_value |= (0b001 << 5); // t_sb[7:5] = 62.5 ms
    bme280_write_reg(BME280_REG_config, config_reg_value);

    uint8_t ctrl_meas_reg_value = 0;
    ctrl_meas_reg_value |= (0b001 << 5); // osrs_t[7:5] = oversampling temperature 1
    ctrl_meas_reg_value |= (0b001 << 2); // osrs_p[4:2] = oversampling pressure 1
    ctrl_meas_reg_value |= (0b11 << 0);  // mode[1:0] = normal mode
    bme280_write_reg(BME280_REG_ctrl_meas, ctrl_meas_reg_value);
}

void bme280_read_regs(uint8_t start_reg_address, uint8_t* buffer, uint8_t length)
{
    uint8_t data[1] = {start_reg_address};

    bme280_ctx.i2c_write(data, sizeof(data));

    bme280_ctx.i2c_read(buffer, length);
}

void bme280_write_reg(uint8_t reg_address, uint8_t value)
{
    uint8_t data[2] = {reg_address, value};
    bme280_ctx.i2c_write(data, sizeof(data));
}

uint32_t bme280_read_temp_raw() {
    uint8_t read[3] = {0};
    bme280_read_regs(BME280_REG_temp_msb, read, 3);
    uint32_t value = ((uint32_t)read[0] << 12) | ((uint32_t)read[1] << 4) | (read[2] >> 4);
    return value;
}

uint32_t bme280_read_pres_raw() {
    uint8_t read[3] = {0};
    bme280_read_regs(BME280_REG_press_msb, read, 3);
    uint32_t value = ((uint32_t)read[0] << 12) | ((uint32_t)read[1] << 4) | (read[2] >> 4);
    return value;
}

uint16_t bme280_read_hum_raw() {
    uint8_t read[2] = {0};
    bme280_read_regs(BME280_REG_hum_msb, read, 2);
    uint16_t value = ((uint16_t)read[0] << 8) | read[1];
    return value;
}

float bme280_read_temperature(void) {
    uint32_t adc_T = bme280_read_temp_raw();
    
    int32_t var1, var2, T;
    
    var1 = ((((adc_T >> 3) - ((int32_t)bme280_calib.dig_T1 << 1))) * 
            ((int32_t)bme280_calib.dig_T2)) >> 11;
    
    var2 = (((((adc_T >> 4) - ((int32_t)bme280_calib.dig_T1)) * 
              ((adc_T >> 4) - ((int32_t)bme280_calib.dig_T1))) >> 12) * 
            ((int32_t)bme280_calib.dig_T3)) >> 14;
    
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    
    return (float)T / 100.0f;
}

float bme280_read_pressure(void) {
    uint32_t adc_P = bme280_read_pres_raw();
    
    int64_t var1, var2, p;
    
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bme280_calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bme280_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)bme280_calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bme280_calib.dig_P3) >> 8) + 
           ((var1 * (int64_t)bme280_calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bme280_calib.dig_P1) >> 33;
    
    if (var1 == 0) return 0;
    
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bme280_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bme280_calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bme280_calib.dig_P7) << 4);
    
    return (float)p / 256.0f;
}

float bme280_read_humidity(void) {
    uint16_t adc_H = bme280_read_hum_raw();
    
    int32_t v_x1_u32r;
    
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)bme280_calib.dig_H4) << 20) - 
                    (((int32_t)bme280_calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * 
                 (((((((v_x1_u32r * ((int32_t)bme280_calib.dig_H6)) >> 10) * 
                    (((v_x1_u32r * ((int32_t)bme280_calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + 
                    ((int32_t)2097152)) * ((int32_t)bme280_calib.dig_H2) + 8192) >> 14));
    
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * 
                               ((int32_t)bme280_calib.dig_H1)) >> 4));
    
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    
    return (float)(v_x1_u32r >> 12) / 1024.0f;
}

void bme280_telemetry_start() {
    bme280_telemetry_running = true;
    bme280_last_measure_us = time_us_64();
}

void bme280_telemetry_stop() {
    bme280_telemetry_running = false;
}

bool bme280_telemetry_is_running() {
    return bme280_telemetry_running;
}

void bme280_telemetry_handler() {
    if (!bme280_telemetry_running) {
        return;
    }
    
    uint64_t now = time_us_64();
    if (now >= bme280_last_measure_us + BME280_TELEMETRY_PERIOD_US) {
        bme280_last_measure_us = now;
        
        float temp = bme280_read_temperature();
        float pres = bme280_read_pressure();
        float hum = bme280_read_humidity();
        
        printf("%.2f %.2f %.2f\n", temp, pres, hum);
    }
}