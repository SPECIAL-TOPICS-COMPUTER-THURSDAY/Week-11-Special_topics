#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define LDR_PIN ADC1_CHANNEL_7 // GPIO35
#define LED_PIN GPIO_NUM_18    // GPIO18 for LED

static const char *TAG = "LDR_LED_CONTROL";

void app_main(void)
{
    ESP_LOGI(TAG, "เริ่มระบบควบคุมแสง LED โดยใช้ LDR");

    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_PIN, ADC_ATTEN_DB_11);

    // Configure LEDC PWM for LED
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ledc_conf = {
        .gpio_num = LED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_conf);

    ESP_LOGI(TAG, "LDR Pin: GPIO35 (ADC1_CH7)");
    ESP_LOGI(TAG, "LED Pin: GPIO18");
    ESP_LOGI(TAG, "ช่วง ADC: 0-4095");
    ESP_LOGI(TAG, "PWM Duty: 0-8191 (13-bit)");
    ESP_LOGI(TAG, "------------------------");

    while (1)
    {
        int adc_value = adc1_get_raw(LDR_PIN);
        // Map ADC value (0-4095) to PWM duty (0-8191)
        // When ADC low (low light), duty low (dim LED)
        // When ADC high (bright light), duty high (bright LED)
        uint32_t duty = (adc_value * 8191) / 4095;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ESP_LOGI(TAG, "ค่า ADC: %d | Duty PWM: %d", adc_value, (int)duty);

        vTaskDelay(pdMS_TO_TICKS(500)); // Delay 500ms
    }
}
