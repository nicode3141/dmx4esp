/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

static const int RX_BUF_SIZE = 512;

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)
#define rxtxDIR_PIN (GPIO_NUM_21)
#define UART_PORT UART_NUM_2 // we're using UART_NUM_2, UART_NUM_0 is connected to Serial UART Interface

//UART DMX Communication Protocol
#define delayBreakMICROSEC 250 // how long should the Break signal be (>88µs)
#define delayMarkMICROSEC 20 // how long should the Mark signal be (>12µs)

uint8_t dmxPacket[512];


void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 250000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    gpio_set_direction(rxtxDIR_PIN, GPIO_MODE_OUTPUT); // CONFIGURE GPIO PIN 26 AS OUTPUT

   
    


    esp_err_t result = uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 513, 0, NULL, 0);

    // Check if installation was successful
    if (result != ESP_OK) {
        printf("Failed to install UART driver: %d\n", result);
    }
}

void sendDMX(uint8_t DMXStream[]){
    gpio_set_level(rxtxDIR_PIN, 1); // PULL OUTPUT DIR HIGH TO SEND

    uint8_t startCode = 0x00;

    //for(;;){

        //UART communication
        uart_wait_tx_done(UART_PORT, 1000); // wait 1000 ticks until empty
        //Reset or Break > 88µs
        uart_set_line_inverse(UART_PORT, UART_SIGNAL_TXD_INV); //create a break signal by inversing TXD signal
        esp_rom_delay_us(delayBreakMICROSEC);
        uart_set_line_inverse(UART_PORT, 0); //stopping break signal by flipping signal back to normal
        //Mark > 12µs
        esp_rom_delay_us(delayMarkMICROSEC); //Mark signal after Break

        //Start Code
        uart_write_bytes(UART_PORT, (const char*) &startCode, 1); //mark start code
        //DMX PACKET
        uart_write_bytes(UART_PORT, (const char*) DMXStream, 512);

        uart_wait_tx_done(UART_PORT, 1000);
    //}
}



void app_main(void)
{
    init();
    dmxPacket[6] = 255;
    dmxPacket[7] = 255;
    dmxPacket[8] = 255;
    
    for(;;) {
        //fade values between 0 and 255

        for(int i = 0; i <= 255; i++){
            
            dmxPacket[0] = i;
            dmxPacket[5] = i / 2;
            sendDMX(dmxPacket);
            
            vTaskDelay(23 / portTICK_PERIOD_MS);
        }

        for(int i = 255; i >= 0; i--){

            dmxPacket[0] = i;
            dmxPacket[5] = i / 2;
            sendDMX(dmxPacket);
            
            vTaskDelay(23 / portTICK_PERIOD_MS);
        }
        
       
        
    }
    
}
