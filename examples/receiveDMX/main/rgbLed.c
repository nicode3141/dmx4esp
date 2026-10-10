#include "dmx4esp.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_mac.h"
#include "led_strip.h"

#define ARRAY_LENGTH_MACRO(a) (sizeof(a) / sizeof(a[0]))

#define FIXTURE_FOOTPRINT 3
#define FIXTURE_ADDRESS 1
#define REFRESH_RATE 1

#define LED_GPIO 48

static led_strip_handle_t strip;


uint8_t receivedSignal[FIXTURE_FOOTPRINT];

// config for esp32-s3
dmxPinout dmxPins = {
    .tx = GPIO_NUM_17,
    .rx = GPIO_NUM_18,
    .dir = GPIO_NUM_1
};



void setup(){
    led_strip_config_t cfg = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&cfg, &rmt, &strip));
    led_strip_clear(strip);

    setupDMX(dmxPins);
    initDMX(false);
}

// rgb is displayed via pwm duty cycles with ledc
void displayRGB(uint8_t dmxSignal[]){
    led_strip_set_pixel(strip, 0, dmxSignal[0], dmxSignal[1], dmxSignal[2]);
    led_strip_refresh(strip);
}

void app_main(void){    
    setup();

    //update state
    for(;;){
        readFixture(receivedSignal, FIXTURE_ADDRESS, FIXTURE_FOOTPRINT);
        displayRGB(receivedSignal);
        
        //wait a custom interval before reading
        vTaskDelay(REFRESH_RATE);
        
    }
}


