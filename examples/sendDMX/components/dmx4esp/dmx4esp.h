#ifndef DMX_H
#define DMX_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_mac.h"

typedef struct dmxPinout {
    gpio_num_t tx;
    gpio_num_t rx;
    gpio_num_t dir;
} dmxPinout;

void setupDMX(dmxPinout pinout);
esp_err_t initDMX(bool sendDMX);

void sendDMX(uint8_t DMXStream[]);
void sendAddress(uint16_t address, uint8_t value);

#endif