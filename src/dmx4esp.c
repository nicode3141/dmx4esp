/*
 * SPDX-FileCopyrightText: 2024-2025 Nicolas Pfeifer <info@nicodenetworks.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "dmx4esp.h"
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

//Async DMX Handler for multithreading, I'm using a semaphore in order to prevent race conditions and avoid data corruption during transmission.
static SemaphoreHandle_t sendDMXSemaphore; //semaphore in form of a Mutex
static TaskHandle_t dmxOperationsTaskHandle; //keep track of running tasks

//define pinout
static gpio_num_t TXD_PIN = GPIO_NUM_NC;
static gpio_num_t RXD_PIN = GPIO_NUM_NC;
static gpio_num_t rxtxDIR_PIN = GPIO_NUM_NC;
static const uart_port_t UART_PORT = UART_NUM_2; // we're using UART_NUM_2, UART_NUM_0 is connected to Serial UART Interface

//UART DMX Communication Protocol
#define delayBreakMICROSEC 250 // duration of the Break Signal (>88µs)
#define delayMarkMICROSEC 20 // duration of the Mark After Break Signal (>12µs)

static uint8_t dmxPacket[512]; //send packet
static uint8_t dmxReadOutput[512]; //received packet
static uint16_t lastDmxReadAddress = 0;

/**
* DMX
*/


/**
 * @brief Configures the GPIO pins for DMX communication.
 **
 * @note To use the default pins, don't call this function.
 * @note Library's default pins are: TX->1 RX-3 dir->23
 * @param txPin The GPIO pin number for transmitting DMX data.
 * @param rxPin The GPIO pin number for receiving DMX data.
 * @param rxtxDirectionPin The GPIO pin number for controlling the direction of the DMX communication.
 *
 * @return void
 */
void setupDMX(dmxPinout pinout){
    TXD_PIN = pinout.tx;
    RXD_PIN = pinout.rx;
    rxtxDIR_PIN = pinout.dir;
}

/**
 * @brief Internal pipeline for sending current dmx data from internal uint8_t dmxPacket[512] once.
 *
 * @note This function is only expected to be used internally.
 * @param startCode Pointer to the start code, normally 0x00 for default control.
 *                                           Special cases covered in the README.
 * 
 * 
 * @return void
 */
static void sendDMXPipeline(uint8_t *startCode){
    //UART communication
    uart_wait_tx_done(UART_PORT, 1000); // wait 1000 ticks until empty
    //Reset or Break > 88µs
    uart_set_line_inverse(UART_PORT, UART_SIGNAL_TXD_INV); //create a break signal by inversing TXD signal
    esp_rom_delay_us(delayBreakMICROSEC);
    uart_set_line_inverse(UART_PORT, 0); //stopping break signal by flipping signal back to normal
    //Mark > 12µs
    esp_rom_delay_us(delayMarkMICROSEC); //Mark signal after Break

    //Start Code
    uart_write_bytes(UART_PORT, (const char*) startCode, 1); //mark start code

    xSemaphoreTake(sendDMXSemaphore, portMAX_DELAY);

    //DMX PACKET
    uart_write_bytes(UART_PORT, (const char*) dmxPacket, 512);

    xSemaphoreGive(sendDMXSemaphore);

    uart_wait_tx_done(UART_PORT, 1000);

    vTaskDelay(10 / portTICK_PERIOD_MS); //sleep 10ms
}

/**
 * @brief Internal loop to send dmx continuously.
 *
 * @note This function is only expected to be used internally.
 * 
 * @return void
 */
static void sendDMXtask(void * parameters){
    
    uint8_t startCode = 0x00;

    for(;;){
        sendDMXPipeline(&startCode);
    }
}

/**
 * @brief configures the esp to send / receive dmx data.
 *        This function can be called multiple times.
 **
 * @note  sends / reads a dmxSignal concurrently!
 * @param sendDMX if true, send dmx forever. Otherwise read dmx.
 * @return void
 */
esp_err_t initDMX(bool sendDMX) {
    const uart_config_t uart_config = {
        .baud_rate = 250000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    if (!sendDMX) {
        printf("DMX Read is currently unsuported! \n");
        return ESP_FAIL;
    }

    //Check if pins are defined
    if(TXD_PIN == GPIO_NUM_NC || RXD_PIN == GPIO_NUM_NC || rxtxDIR_PIN == GPIO_NUM_NC){
        printf("No pinout present, please define use setupDMX() first! \n");
        return ESP_FAIL;
    }
    
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    gpio_set_direction(rxtxDIR_PIN, GPIO_MODE_OUTPUT); // CONFIGURE GPIO PIN 26 AS OUTPUT

    gpio_set_level(rxtxDIR_PIN, sendDMX ? 1 : 0); // PULL OUTPUT DIR HIGH TO SEND
    sendDMXSemaphore = xSemaphoreCreateMutex();

    //Check if the semaphore was successfully created.
    if (sendDMXSemaphore == NULL) {
        printf("Failed to create DMX semaphore\n");
        return ESP_FAIL;
    }

    esp_err_t result = uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 513, 0, NULL, 0); //install uart driver without any uart event handler -> not used for sending

    // Check if installation was successful
    if (result != ESP_OK) {
        printf("Failed to install UART driver: %d\n", result);
    } else if(dmxOperationsTaskHandle != NULL){
        vTaskDelete(dmxOperationsTaskHandle); // Delete other running dmx operations
    } else{
        xTaskCreatePinnedToCore(sendDMXtask, "DMX Send Task", 2048, NULL, 1, &dmxOperationsTaskHandle, 1); //PIN TO CORE 1
    }

    return result;
}


/**
 * @brief Clears the uart input buffer.
 * @note  This function is only expected to be used internally.
 * @return void
 */
void clearDMXQueue(){
    uart_flush_input(UART_PORT);
}

/**
 * @brief This function only sets the dmx data to send!
 **       The actual data transfer happens in the init() function.  
 * @note  init() sends the dmxSignal concurrently!
 * @param DMXStream 512 bytes long array containing the dmx data to send
 * @return void
 */
void sendDMX(uint8_t DMXStream[]){
    xSemaphoreTake(sendDMXSemaphore, portMAX_DELAY);
    memcpy(dmxPacket, DMXStream, 512);
    xSemaphoreGive(sendDMXSemaphore);
}

/**
 * @brief Changes the value of any given dmx channel.
 *        This function only sets the data to send!     
 * @note  init() sends the dmxSignal concurrently!
 *        
 * @param address The address of the dmx channel (1 - 512)
 * @param value The dmx value to send (0 - 255)
 * @return void
 */
void sendAddress(uint16_t address, uint8_t value){
    if(address >= 1 && address <= 512){
        xSemaphoreTake(sendDMXSemaphore, portMAX_DELAY);
        dmxPacket[address-1] = value;
        xSemaphoreGive(sendDMXSemaphore);
    } else{
        printf("Address out of scope (1 - 512): %i", address);
    }
}