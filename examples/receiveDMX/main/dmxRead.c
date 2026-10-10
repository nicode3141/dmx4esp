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

void test_read(uint8_t receivedSignal[513]){
    //get the current dmx frame as an array
    readDMX(receivedSignal);
        
    //copy the received data into a static array
    LOG(receivedSignal);
    uint8_t signal = receivedSignal[260];
    gpio_set_level(LED_PIN, signal > 0 ? 1 : 0);
    gpio_set_level(DEBUG_PIN, receivedSignal[258] == 255 ? 1 : 0);
}

void test_read_address(void){
    uint8_t value = readAddress(260);
    gpio_set_level(DEBUG_PIN, value == 255 ? 1 : 0);
}

void test_read_fixture(uint8_t fixureData[3]){
    readFixture(fixureData, 258, 3);
    uint8_t value = fixureData[2];
    gpio_set_level(LED_PIN, value == 255 ? 1 : 0);
}

void app_main(void){
    setup();

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


