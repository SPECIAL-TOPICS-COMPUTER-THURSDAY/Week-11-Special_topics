#include "driver/adc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define LDR_ADC_CHANNEL  ADC1_CHANNEL_6   // GPIO34
#define BUZZER_GPIO      18               // คุมทรานซิสเตอร์
#define ATTEN            ADC_ATTEN_DB_12  // ช่วง ~0–3.3V
#define N_SAMPLE         16               // เฉลี่ยลด noise
#define TH_ON            950              // ค่าที่ให้ "มืด" -> เปิด
#define TH_OFF           1050             // ค่าที่ให้ "สว่าง" -> ปิด (ฮิสเทอรีซิส)
static const char* TAG = "LDR_BUZZ";

void app_main(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ATTEN);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
    int buz_on = 0;

    while (1) {
        // อ่านแบบถัวเฉลี่ย
        int sum = 0;
        for (int i = 0; i < N_SAMPLE; i++) {
            sum += adc1_get_raw(LDR_ADC_CHANNEL);
        }
        int val = sum / N_SAMPLE;

        // ฮิสเทอรีซิสกันสั่น
        if (!buz_on && val < TH_ON)  { buz_on = 1; }
        if (buz_on  && val > TH_OFF) { buz_on = 0; }

        gpio_set_level(BUZZER_GPIO, buz_on);
        ESP_LOGI(TAG, "ADC=%d  buzzer=%s", val, buz_on ? "ON" : "OFF");

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

