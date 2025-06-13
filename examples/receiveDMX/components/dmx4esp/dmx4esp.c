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
 static QueueHandle_t uart_queue; //stores the event queue handle
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

//enums needed for internal dmx decoding
DMXStatus dmxStatus = SEND;

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

/** ----------------------------------------------------------------
 *  ------  The DMX READ feature is CURRENTLY NOT SUPPORTED! -------
 *  
 *  The implementation below is unreliable due to timing problems
 *      with the esp32 development boards
 *      and lack of development time.
 *  ----------------------------------------------------------------
 */

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
    /*esp_err_t enoughSpace = uart_get_buffered_data_len(UART_PORT, (size_t*)&uartEvent->size); //check for enough space to receive

    if(enoughSpace != ESP_OK){
        setDebugLED(255, 0, 0);
    }*/
    int size = uartEvent->size;
    if (size > RX_BUF_SIZE) size = RX_BUF_SIZE;
    int bytes_read = uart_read_bytes(UART_PORT, receiveBuffer, size, portMAX_DELAY); //read uart indefinitely
    
    if(bytes_read == -1){
        printf("error whilst reading from UART Buffer! \n");
    }

switch(dmxStatus){
         case BREAK:
             if(receiveBuffer[0] == 0){ // startBit -> 0x00
                //setDebugLED(20, 0, 20);
                //setDebugLED(0, 20, 20);
                
                 dmxStatus = RECEIVE_DATA;
                 lastDmxReadAddress = 1; //break -> DMX Stream starts at the beginning
                 break;
             }
             break;
         case RECEIVE_DATA:
             for(int i = 0; i < uartEvent->size; i++){

                 if(lastDmxReadAddress >= 1 && lastDmxReadAddress <= 512){
                    
                    dmxReadOutput[lastDmxReadAddress] = receiveBuffer[i]; //assign output to dmx data
                    

                     
                     lastDmxReadAddress++;

                     if(lastDmxReadAddress > 512){
                        dmxStatus = DONE;
                        break;
                     }
                 } else{
                    dmxStatus = DONE;
                 }
             }
        default:
             break;
     }
}


/**
 * @brief Internal handler for receiving dmx.
 *
 * @note This function is only expected to be used internally.
 * 
 * @return void
 */
static void receiveDMXtask(void * parameters){
    uint8_t receiveBuffer[RX_BUF_SIZE]; //without malloc() -> static buffer

    uart_event_t uartEvent;

    for(;;){
        memset(receiveBuffer, 0, RX_BUF_SIZE); //clear buffer
        if(xQueueReceive(uart_queue, (void *)&uartEvent, portMAX_DELAY) == pdTRUE){ //pdTRUE if an item got successfully received from the queue
            
            switch(uartEvent.type){
                case UART_BREAK:
                    if((dmxStatus == DONE)){
                        uart_flush_input(UART_PORT);
                        xQueueReset(uart_queue);
                        dmxStatus = BREAK;
                    } else if(dmxStatus == INACTIVE){
                        uart_flush_input(UART_PORT);
                        xQueueReset(uart_queue);
                        dmxStatus = BREAK;
                    } 
                    dmxStatus = BREAK;
                    break;
                case UART_DATA:
                    read_uart_stream(receiveBuffer, &uartEvent);
                    break;
                case UART_FRAME_ERR:
                case UART_PARITY_ERR:
                case UART_BUFFER_FULL:
                case UART_FIFO_OVF:
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

/**
 *  @NOTE: The code below is now safe to use.
 */

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

    esp_err_t result = uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 513, 20, &uart_queue, 0);


    // Check if uart_queue isn't a null pointer
    if(uart_queue == NULL){
        printf("Failed to set an event queue!\n");
    }

    // Check if installation was successful
    if (result != ESP_OK) {
        printf("Failed to install UART driver: %d\n", result);
    } else if(dmxOperationsTaskHandle != NULL){
        vTaskDelete(dmxOperationsTaskHandle); // Delete other running dmx operations
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
 * @brief Retuns a received dmx signal (once).
 * 
 * @note  init() reads the dmxSignal concurrently!
 *    
 * @return dmxOutput - pointer to 512 bytes long array containing the dmx data received.
 */
uint8_t* readDMX(){
   return dmxReadOutput;
}

/**
 * @brief Retuns a received dmx channel (once).
 * 
 * @note  init() reads the dmxSignal concurrently!
 * @param address The address of the dmx channel to read from (1 - 512)
 *    
 * @return dmxOutput - data of the dmx channel (0 - 255) 
 */
uint8_t readAddress(uint16_t address){
    if(address >= 1 && address <= 512){
        return dmxReadOutput[address];
    } else{
        printf("Address out of scope (1 - 512): %i", address);
        return 0;
    }
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

    memcpy(fixtureData, &dmxReadOutput[startAddress], footprint); //copy a part of the original dmx output

    return fixtureData;
}