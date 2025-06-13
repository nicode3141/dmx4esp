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

//best practise: external array to store the current dmx stream to send, this helps to prevent accidental overwrites
uint8_t currentDMX[512] = {0};
uint8_t blackout[512] = {0};

//DMX sequence example: controlling a moving head
void sequence1(){
    sendAddress(1, 255); // PAN -> 255 (max)

    waitMS(2000);

    sendAddress(3, 255); // TILT -> 255 (max)

    waitMS(2000);

    sendAddress(6, 255); // DIM -> 255 (full)
    sendAddress(7, 255); // RED -> 255 (full)

    waitMS(2000);

    //Purple Hue 
    sendAddress(7, 102);
    sendAddress(8, 92);
    sendAddress(9, 231);

    waitMS(5000);

    sendDMX(blackout); // Blackout

    waitMS(2000);
}

/**
 * @brief Retuns a received dmx signal (once).
 * 
 * @note  init() reads the dmxSignal concurrently!
 *    
 * @return dmxOutput - pointer to 512 bytes long array containing the dmx data received.
 */
void app_main(void){
    //configure pinout for rx, tx & direction ports
    dmxPinout dmxPins = {
        .tx = GPIO_NUM_17,
        .rx = GPIO_NUM_18,
        .dir = GPIO_NUM_1
    };

    //apply pinout
    setupDMX(dmxPins);

    //initialize to send DMX
    initDMX(true);
    
    //blackout all channels
    memcpy(&currentDMX, &blackout, sizeof(blackout));
    sendDMX(currentDMX);

    //execute demo sequence
    sequence1();
}