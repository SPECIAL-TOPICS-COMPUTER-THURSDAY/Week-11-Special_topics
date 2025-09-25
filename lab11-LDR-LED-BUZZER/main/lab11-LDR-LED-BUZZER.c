#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LDR_PIN ADC1_CHANNEL_7     // GPIO35
#define LED_BRIGHT_PIN GPIO_NUM_18 // GPIO18 for brightness LED
#define BUZZER_PIN GPIO_NUM_19     // GPIO19 for Buzzer
#define LED_STATUS_PIN GPIO_NUM_21 // GPIO21 for status LED

#define LIGHT_THRESHOLD 1000 // Threshold for low light

static const char *TAG = "LDR_LED_BUZZER_CONTROL";

void app_main(void)
{
    ESP_LOGI(TAG, "เริ่มระบบควบคุมแสง LED และเตือนด้วย Buzzer โดยใช้ LDR");

    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_PIN, ADC_ATTEN_DB_11);

    // Configure LEDC PWM for brightness LED
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ledc_conf = {
        .gpio_num = LED_BRIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_conf);

    // Configure GPIO for Buzzer and Status LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN) | (1ULL << LED_STATUS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "LDR Pin: GPIO35 (ADC1_CH7)");
    ESP_LOGI(TAG, "Brightness LED Pin: GPIO18");
    ESP_LOGI(TAG, "Buzzer Pin: GPIO19");
    ESP_LOGI(TAG, "Status LED Pin: GPIO21");
    ESP_LOGI(TAG, "ช่วง ADC: 0-4095");
    ESP_LOGI(TAG, "PWM Duty: 0-8191 (13-bit)");
    ESP_LOGI(TAG, "เกณฑ์แสงต่ำ: %d", LIGHT_THRESHOLD);
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

        // Check threshold for alarm
        if (adc_value < LIGHT_THRESHOLD)
        {
            // Low light: Turn on Buzzer and Status LED
            gpio_set_level(BUZZER_PIN, 1);
            gpio_set_level(LED_STATUS_PIN, 1);
            ESP_LOGI(TAG, "ค่า ADC: %d | Duty PWM: %d | สถานะ: แสงน้อย - Buzzer และ LED สถานะ เปิด", adc_value, (int)duty);
        }
        else
        {
            // Normal light: Turn off Buzzer and Status LED
            gpio_set_level(BUZZER_PIN, 0);
            gpio_set_level(LED_STATUS_PIN, 0);
            ESP_LOGI(TAG, "ค่า ADC: %d | Duty PWM: %d | สถานะ: แสงปกติ - Buzzer และ LED สถานะ ปิด", adc_value, (int)duty);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Delay 500ms
    }
}
