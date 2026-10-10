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
#include "esp_rom_sys.h"
#include "stdlib.h"
#include "esp_check.h"

static const int RX_BUF_SIZE = 513; // 512 Channels + Startbit

//Async DMX Handler for multithreading, I'm using a semaphore in order to prevent race conditions and avoid data corruption during transmission.
static QueueHandle_t uart_queue; //stores the event queue handle
static SemaphoreHandle_t sendDMXSemaphore; //semaphore in form of a Mutex
static SemaphoreHandle_t readDMXSemaphore;
static TaskHandle_t dmxOperationsTaskHandle; //keep track of running tasks
static portMUX_TYPE dmx_break_lock = portMUX_INITIALIZER_UNLOCKED; //lock core while sending break signal

//define pinout
static gpio_num_t TXD_PIN = GPIO_NUM_NC;
static gpio_num_t RXD_PIN = GPIO_NUM_NC;
static gpio_num_t rxtxDIR_PIN = GPIO_NUM_NC;
static const uart_port_t UART_PORT = UART_NUM_2; // we're using UART_NUM_2, UART_NUM_0 is connected to Serial UART Interface

//UART DMX Communication Protocol
#define delayBreakMICROSEC 250 // duration of the Break Signal (>88µs)
#define delayMarkMICROSEC 20 // duration of the Mark After Break Signal (>12µs)

//enums needed for internal dmx decoding
static volatile dmxStatus = SEND;

static uint8_t dmxPacket[512]; //send packet
static uint8_t dmxRxBuff[513]; //receive task writes bytes
static uint8_t dmxReadOutput[513]; //received packet
static uint16_t lastDmxReadAddress = 0;

static const char* INIT_TAG = "UART_INIT";

/**
* DMX
*/

/**
 * @brief Configures the GPIO pins for DMX communication.
 **
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

    portENTER_CRITICAL(&dmx_break_lock);

    //Reset or Break > 88µs
    uart_set_line_inverse(UART_PORT, UART_SIGNAL_TXD_INV); //create a break signal by inversing TXD signal
    esp_rom_delay_us(delayBreakMICROSEC);
    uart_set_line_inverse(UART_PORT, 0); //stopping break signal by flipping signal back to normal
    //Mark > 12µs
    esp_rom_delay_us(delayMarkMICROSEC); //Mark signal after Break

    portEXIT_CRITICAL(&dmx_break_lock);

    xSemaphoreTake(sendDMXSemaphore, portMAX_DELAY);

     //Start Code
    uart_write_bytes(UART_PORT, (const char*) startCode, 1); //mark start code

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
 * @brief Internal function to decode the received uart stream into dmx data.
 *
 * @note This function is only expected to be used internally.
 * @param receiveBuffer Pointer to the buffer where the received dmx data should be written to.
 * @param uartEvent Pointer to the Event structure used in UART event queue.
 * 
 * @return void
 */
static void read_uart_stream(uint8_t receiveBuffer[], uart_event_t *uartEvent){
    int size = uartEvent->size;
    if (size > RX_BUF_SIZE) size = RX_BUF_SIZE;
    int bytes_read = uart_read_bytes(UART_PORT, receiveBuffer, size, 0); //read uart indefinitely
    
    if(bytes_read <= 0) return;

    int startAdress = 0;

    if(dmxStatus == BREAK){
        if(receiveBuffer[0] != 0x00) return; // detect invalid startbit
        dmxStatus = RECEIVE_DATA;

        dmxRxBuff[0] = receiveBuffer[0];
        lastDmxReadAddress = 1;
        startAdress = 1;
    }

    if(dmxStatus != RECEIVE_DATA) return;

    for(int i = startAdress; i < bytes_read; i++){
        if(lastDmxReadAddress > 512){
            dmxStatus = DONE;
            return;
        }
        dmxRxBuff[lastDmxReadAddress] = receiveBuffer[i];
        lastDmxReadAddress++;
    }

    if(lastDmxReadAddress > 512) dmxStatus = DONE;
}


/**
 * @brief Internal handler for receiving dmx.
 *
 * @note This function is only expected to be used internally.
 * 
 * @return void
 */
static void receiveDMXtask(void * parameters){
    uint8_t receiveBuffer[RX_BUF_SIZE+1]; //without malloc() -> static buffer

    uart_event_t uartEvent;

    for(;;){
        memset(receiveBuffer, 0, RX_BUF_SIZE); //clear buffer
        if(xQueueReceive(uart_queue, (void *)&uartEvent, portMAX_DELAY) == pdTRUE){ //pdTRUE if an item got successfully received from the queue
            
            switch(uartEvent.type){
                case UART_BREAK:
                    if((dmxStatus == DONE) || (dmxStatus == INACTIVE)){
                        uart_flush_input(UART_PORT);
                        xQueueReset(uart_queue);
                    }
                    dmxStatus = BREAK;
                    break;
                case UART_DATA:
                    read_uart_stream(receiveBuffer, &uartEvent);
                    if(dmxStatus == DONE){
                        xSemaphoreTake(readDMXSemaphore, portMAX_DELAY);
                        memcpy(dmxReadOutput, dmxRxBuff, 513);
                        xSemaphoreGive(readDMXSemaphore);
                        dmxStatus = INACTIVE;
                    }
                    break;
                case UART_FRAME_ERR:
                case UART_PARITY_ERR:
                case UART_BUFFER_FULL:
                case UART_FIFO_OVF:
                    uart_flush_input(UART_PORT);
                    xQueueReset(uart_queue);
                    break;
                default:
                    xQueueReset(uart_queue);
                    uart_flush_input(UART_PORT);
                    dmxStatus = INACTIVE;
                    break;
            }
        } else{
            
        }
     }

}

static esp_err_t resetDMX(void){
    // Delete other running dmx operations
    if(dmxOperationsTaskHandle != NULL){
        vTaskDelete(dmxOperationsTaskHandle);
        dmxOperationsTaskHandle == NULL;
    }

    // delete exsisting driver if any
    if(uart_is_driver_installed(UART_PORT)){
        uart_set_line_inverse(UART_PORT, 0); // remove possible break sig
        esp_err_t result = uart_driver_delete(UART_PORT);
        if(result != ESP_OK){
            printf("uart_driver_delete failed: %s", esp_err_to_name(result));
            return result;
        }
    }

    uart_queue = NULL;
    return ESP_OK;
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

    //Check if pins are defined
    if(TXD_PIN == GPIO_NUM_NC || RXD_PIN == GPIO_NUM_NC || rxtxDIR_PIN == GPIO_NUM_NC){
        printf("No pinout present, please define use setupDMX() first! \n");
        return ESP_FAIL;
    }
    
    ESP_RETURN_ON_ERROR(uart_param_config(UART_PORT, &uart_config), INIT_TAG, "UART param config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), INIT_TAG, "UART set pin failed");

    ESP_RETURN_ON_ERROR(gpio_set_direction(rxtxDIR_PIN, GPIO_MODE_OUTPUT), INIT_TAG, "failed to set rxtxDIR_PIN as output");
    gpio_set_level(rxtxDIR_PIN, sendDMX ? 1 : 0);

    if(sendDMXSemaphore == NULL){
        sendDMXSemaphore = xSemaphoreCreateMutex();
    }

    if(readDMXSemaphore == NULL){
        readDMXSemaphore = xSemaphoreCreateMutex();
    }

    // Check if the semaphore was successfully created.
    if (sendDMXSemaphore == NULL || readDMXSemaphore == NULL) {
        printf("Failed to create DMX semaphore\n");
        return ESP_FAIL;
    }

    esp_err_t result = resetDMX();
    if(result != ESP_OK) return result;

    // install uart event queue driver
    esp_err_t result = uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 513, 20, &uart_queue, 0);

    // Check if uart_queue isn't a null pointer
    if(uart_queue == NULL){
        printf("Failed to set an event queue!\n");
    }

    // Check if installation was successful
    if (result != ESP_OK) {
        printf("Failed to install UART driver: %d\n", result);
    } else{
        if(sendDMX){
            xTaskCreatePinnedToCore(sendDMXtask, "DMX Send Task", 2048, NULL, 1, &dmxOperationsTaskHandle, 1); //PIN TO CORE 1
        } else{
            xTaskCreatePinnedToCore(receiveDMXtask, "DMX Receive Task", 4096, NULL, 1, &dmxOperationsTaskHandle, 1); //PIN TO CORE 1
        }
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
 *        Channel 1 is at array index 0! Max index is 511!
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

/**
 * @brief Writes currently received dmx data to a provided array
 * 
 * @param output the desired array to write dmx data to
 */
void readDMX(uint8_t output[513]){
    if(readDMXSemaphore == NULL){ memset(output, 0, 513); return; }
    xSemaphoreTake(readDMXSemaphore, portMAX_DELAY);
    memcpy(output, dmxReadOutput, 513);
    xSemaphoreGive(readDMXSemaphore);
}

/**
 * @brief Retuns a received dmx channel (once).
 * 
 * @param address The address of the dmx channel to read from (1 - 512)
 *    
 * @return dmxOutput - data of the dmx channel (0 - 255) 
 */
uint8_t readAddress(uint16_t address){
    if(address < 1 || address > 512){
        printf("Address out of scope (1 - 512): %i", address);
        return 0;
    }
    if(readDMXSemaphore == NULL) return 0;

    xSemaphoreTake(readDMXSemaphore, portMAX_DELAY);
    uint8_t output = dmxReadOutput[address];
    xSemaphoreGive(readDMXSemaphore);
    return output;
}

/**
 * @brief Retuns a range of the original dmx data.
 * 
 * @note please make sure that the startAddress and footprint don't exceed the maximum of channels! (512)
 * @note  init() reads the dmxSignal concurrently!
 * @param startAddress The first address to read from (1 - 512)
 * @param footprint number of channels needed to read from (1 - 512)
 *    
 * @return dmxOutput - data of the dmx channels. IMPORTANT! free memory after use!
 */
uint8_t* readFixture(uint16_t startAddress, uint16_t footprint){
    if(footprint < 1 || footprint > 512){
        printf("Footprint out of scope (1 - 512): %i", footprint);
        return NULL;
    }
    if(startAddress < 1 || startAddress + footprint > 513){
        printf("startAddress out of scope (1 - 512) / footprint exeeds scope: %i, footprint: %i, lastAddress: %i", startAddress, footprint, startAddress + footprint -1);
        return NULL;
    }

    uint8_t* fixtureData = (uint8_t*) malloc(footprint); //dynamic allocation to the heap. CALLER HAS TO FREE MEMORY AFTER USE!
    if(fixtureData == NULL){
        printf("Memory allocation failed");
        return NULL;
    }

    xSemaphoreTake(readDMXSemaphore, portMAX_DELAY);
    memcpy(fixtureData, &dmxReadOutput[startAddress], footprint); //copy a part of the original dmx output
    xSemaphoreGive(readDMXSemaphore);

    return fixtureData;
}