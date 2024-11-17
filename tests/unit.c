#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "unity.h"
#include "dmx4esp.h"

#define UNITY_OUTPUT_COLOR "\x1b[1;31m"

static char output[256] = "Hello, world!";


void testHelloWorld(){
    TEST_ASSERT_EQUAL_STRING("Hello, world!", output);
}

void app_main(void){
    UNITY_BEGIN();
    RUN_TEST(testHelloWorld);
    UNITY_END();
}