#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#include "unity.h"
#include "dmx4esp.h"

#define UNITY_OUTPUT_COLOR "\x1b[1;31m"

static char output[256] = "Hello, world!";

void test_initDMX(){
    TEST_ASSERT(initDMX() == 0);
}

void test_sendDMX(){
    uint8_t dmxData[512] = {0};
    dmxData[0] = 255;

    //mock func

    sendDMX(dmxData);
}

void testHelloWorld(){
    TEST_ASSERT_EQUAL_STRING("Hello, world!", output);
}

/*void app_main(void){
    UNITY_BEGIN();
    RUN_TEST(testHelloWorld);
    RUN_TEST(test_initDMX);
    RUN_TEST(test_sendDMX);
    UNITY_END();
}*/

void app_main(void){
    initDMX();
    test_sendDMX();
}