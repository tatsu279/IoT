/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

TaskHandle_t HelloWorldTaskHandle = NULL;
void HelloWorld_Task(void *arg)
{
while (1)
{
printf("Task running: Hello World ..\n");
vTaskDelay(1000 / portTICK_PERIOD_MS);
}
}
void app_main(void)
{
    xTaskCreate(HelloWorld_Task, "HelloWorld", 4096, NULL, 10, &HelloWorldTaskHandle);
    while (1)
    {
        printf("Task running: app main ..\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    //xTaskCreatePinnedToCore(HelloWorld_Task, "HelloWorld", 4096, NULL, 10, &HelloWorldTaskHandle, 1); // Run on Core 1
}