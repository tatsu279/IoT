#include "led_strip.h"
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define LED_PIN 8
#define NUM_LEDS 1
void app_main(void)
{
    led_strip_handle_t led_strip;
    led_strip_config_t strip_config = {
    .strip_gpio_num = LED_PIN,
    .max_leds = NUM_LEDS,
    };
        led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 64,
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    led_strip_clear(led_strip); // tắt
    while (1) {
    // Đỏ
        led_strip_set_pixel(led_strip, 0, 255, 0, 0);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
        // Tắt
        led_strip_clear(led_strip);
        vTaskDelay(pdMS_TO_TICKS(300));
        // Xanh lá
        led_strip_set_pixel(led_strip, 0, 0, 255, 0);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
        // Xanh dương
        led_strip_set_pixel(led_strip, 0, 0, 0, 255);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}