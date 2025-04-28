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
#include "math.h"

#define ARRAY_LENGTH_MACRO(a) (sizeof(a) / sizeof(a[0]))
#define waitMS(a) vTaskDelay(a / portTICK_PERIOD_MS);

void blackout(){
    uint8_t blackout[512] = {0};
    sendDMX(blackout);
}

void fade_channels(){
    float phase = 0.0f;
    float spacing = 0.1f;

    while(true){
        for(int i = 1; i <= 512; i++){
            float value = (sinf(phase + i * spacing) + 1.0f) * 127.5f;
            sendAddress(i, (uint8_t)value);
        }
        phase += 0.1f;
        waitMS(30);
    }
}

void chase_channels(){
    int counter = 0;
    
    counter++;
    for(int i = 1; i <= 512; i++){
        sendAddress(i == 1 ? 512 : i-1, 0);
        sendAddress(i, 255);
        waitMS(20);
    }
    waitMS(100);
    blackout();
    fade_channels();
}

void app_main(void){
    uint8_t blackout[512] = {0};

    initDMX(true);

    sendAddress(1, 255); // PAN

    waitMS(2000);

    sendAddress(3, 255); // TILT

    waitMS(2000);

    sendAddress(6, 255); // DIM
    sendAddress(7, 255); // RED

    waitMS(2000);

    sendDMX(blackout); // Blackout

    waitMS(2000);

    chase_channels();
}