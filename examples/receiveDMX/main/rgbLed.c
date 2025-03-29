#include "dmx4esp.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_mac.h"

#define ARRAY_LENGTH_MACRO(a) (sizeof(a) / sizeof(a[0]))

#define FIXTURE_FOOTPRINT 3
#define FIXTURE_ADDRESS 1
#define REFRESH_RATE 10

#define LED_PIN_R 1
#define LED_PIN_G 2
#define LED_PIN_B 3

uint8_t receivedSignal[FIXTURE_FOOTPRINT];

void app_main(void){
    setup();

    for(;;){
        uint8_t* readBuffer = readFixture(FIXTURE_ADDRESS, FIXTURE_FOOTPRINT);

        memcpy(receivedSignal, readBuffer, FIXTURE_FOOTPRINT);
        free(readBuffer);

        displayRGB(receivedSignal);
        vTaskDelay(REFRESH_RATE);
    }
}

// rgb is displayed via pwm duty cycles with ledc
void displayRGB(uint8_t dmxSignal[]){
    //Red
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dmxSignal[0]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    //Green
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, dmxSignal[1]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    //Blue
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, dmxSignal[2]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

void setupLed(int channel, int gpioPin){
    ledc_timer_config_t ledcTimer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT, //0-256
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledcTimer);

    ledc_channel_config_t ledcChannel = {
        .gpio_num = gpioPin,
        .channel = channel,
        .duty = 0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .intr_type = LEDC_INTR_DISABLE,
        .hpoint = 0,
    };
    ledc_channel_config(&ledcChannel);
}

void setup(){
    setupLed(LEDC_CHANNEL_0, LED_PIN_R);
    setupLed(LEDC_CHANNEL_1, LED_PIN_G);
    setupLed(LEDC_CHANNEL_2, LED_PIN_B);

    initDMX(false);
}