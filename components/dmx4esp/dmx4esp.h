#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

esp_err_t initDMX(void);
void sendDMX(uint8_t DMXStream[]);