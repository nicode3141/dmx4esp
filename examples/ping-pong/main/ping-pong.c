/**
 * Simple request response loop between two devices
 */
#include "dmx4esp.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_mac.h"

#define ARRAY_LENGTH_MACRO(a) (sizeof(a) / sizeof(a[0]))

#define LED_PIN GPIO_NUM_10
#define DEBUG_PIN GPIO_NUM_2
#define REFRESH_RATE_MS 10

#define START_CODE_AT_0 1

uint8_t sentDMX[512] = {0};
uint8_t receivedDMX[513] = {0};


// config for esp32-s3
dmxPinout dmxPins = {
    .tx = GPIO_NUM_17,
    .rx = GPIO_NUM_18,
    .dir = GPIO_NUM_1          
};

void setup(){
    setupDMX(dmxPins);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DEBUG_PIN, GPIO_MODE_OUTPUT);
    initDMX(false); // readDMX
}

void LOG(uint8_t *data){
    static uint8_t last[513];
    for (int i = START_CODE_AT_0; i < 513; i++) {
        if (data[i] != last[i]) {
            last[i] = data[i];
            ESP_LOGI("DMX", "Channel %d: %u", i + 1 - START_CODE_AT_0, data[i]);
        }
    }
}

void app_main(void){
    setup();

    for(;;){
        initDMX(true); // send DMX
        memset(sentDMX, 255, sizeof(sendDMX));
        sendDMX(sentDMX);

        vTaskDelay(pdMS_TO_TICKS(1000));

        initDMX(false); // receive DMX
        bool exit = false;

        long time = xTaskGetTickCount();

        while(!exit){
            readDMX(receivedDMX);
            uint8_t signal = receivedDMX[1];
            gpio_set_level(LED_PIN, signal > 0 ? 1 : 0);
            vTaskDelay(pdMS_TO_TICKS(10));

            if(xTaskGetTickCount() - time > pdMS_TO_TICKS(1000)){
                exit = true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    


   //array to store the dmx values
    uint8_t receivedSignal[513];
    uint8_t fixtureData[3];

    //update state
    for(;;){
        test_read(receivedSignal);
        //wait a custom interval before reading
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


