/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_mac.h"

static const int RX_BUF_SIZE = 512;

//ASYNC DMX Handler
QueueHandle_t dmxQueue;
SemaphoreHandle_t sendDMXSemaphore;

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)
#define rxtxDIR_PIN (GPIO_NUM_23)
#define UART_PORT UART_NUM_2 // we're using UART_NUM_2, UART_NUM_0 is connected to Serial UART Interface

//UART DMX Communication Protocol
#define delayBreakMICROSEC 250 // how long should the Break signal be (>88µs)
#define delayMarkMICROSEC 20 // how long should the Mark signal be (>12µs)

enum DMXStatus {SEND, RECEIVE, BREAK};
enum DMXStatus dmxStatus = SEND;

uint8_t dmxPacket[512];
uint8_t dmxReadOutput[512];
uint16_t lastDmxReadAddress = 0;

/**
* DMX
*/


void sendDMXPipeline(uint8_t *startCode){
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

    xSemaphoreTake(sendDMXSemaphore, portMAX_DELAY);

    //DMX PACKET
    uart_write_bytes(UART_PORT, (const char*) dmxPacket, 512);

    xSemaphoreGive(sendDMXSemaphore);

    uart_wait_tx_done(UART_PORT, 1000);

    vTaskDelay(10 / portTICK_PERIOD_MS); //sleep 10ms
}

void sendDMXtask(void * parameters){
    
    uint8_t startCode = 0x00;

    for(;;){
        sendDMXPipeline(&startCode);
    }
}

void read_uart_stream(uint8_t *receiveBuffer, uart_event_t *uartEvent){
    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t*)&uartEvent->size)); //check for enough space to receive
    uart_read_bytes(UART_PORT, receiveBuffer, uartEvent->size, portMAX_DELAY); //read uart indefinitely

    switch(dmxStatus){
        case BREAK:
            if(receiveBuffer[0] == 0){
                dmxStatus = RECEIVE;
                lastDmxReadAddress = 0; //break -> DMX Stream starts at the beginning
                break;
            }
            break;
        case RECEIVE:
            for(int i; i < &uartEvent->size; i++){
                if(lastDmxReadAddress < 513){
                    dmxReadOutput[lastDmxReadAddress+1] = receiveBuffer[i]; //assign output to dmx data
                }
            }
            break;
    }
}

void receiveDMXtask(void * parameters){
    uint8_t receiveBuffer[RX_BUF_SIZE]; //without malloc() -> static buffer

    uart_event_t uartEvent;

    for(;;){
        memset(receiveBuffer, 0, RX_BUF_SIZE); //clear buffer

        switch(uartEvent.type){
            case UART_BREAK:
                dmxStatus = BREAK;
                break;
    
            case UART_DATA:
                read_uart_stream(receiveBuffer, &uartEvent);
                break;
        }
    }

}

esp_err_t initDMX(bool sendDMX) {
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

    gpio_set_level(rxtxDIR_PIN, sendDMX ? 1 : 0); // PULL OUTPUT DIR HIGH TO SEND
    sendDMXSemaphore = xSemaphoreCreateMutex();


    esp_err_t result = uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 513, 0, NULL, 0);

    // Check if installation was successful
    if (result != ESP_OK) {
        printf("Failed to install UART driver: %d\n", result);
    }else{
        xTaskCreatePinnedToCore(sendDMXtask, "DMX Send Task", 2048, NULL, 1, NULL, 1); //PIN TO CORE 1
    }

    return result;
}

void clearDMXQueue(){
    uart_flush_input(UART_PORT);
}

void sendDMX(uint8_t DMXStream[]){
    xSemaphoreTake(sendDMXSemaphore, portMAX_DELAY);
    memcpy(dmxPacket, DMXStream, 512);
    xSemaphoreGive(sendDMXSemaphore);
}


