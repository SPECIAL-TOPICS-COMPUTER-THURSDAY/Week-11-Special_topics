#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#define POTENTIOMETER_CHANNEL ADC1_CHANNEL_6  // GPIO34
#define DEFAULT_VREF 1100
#define NO_OF_SAMPLES 64

static const char *TAG = "ADC_POT";
static esp_adc_cal_characteristics_t *adc_chars;

static bool check_efuse(void){
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) ESP_LOGI(TAG,"eFuse Two Point: รองรับ");
    else ESP_LOGI(TAG,"eFuse Two Point: ไม่รองรับ");
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) ESP_LOGI(TAG,"eFuse Vref: รองรับ");
    else ESP_LOGI(TAG,"eFuse Vref: ไม่รองรับ");
    return true;
}

static void print_char_val_type(esp_adc_cal_value_t t){
    if (t==ESP_ADC_CAL_VAL_EFUSE_TP) ESP_LOGI(TAG,"ใช้การปรับเทียบแบบ Two Point");
    else if (t==ESP_ADC_CAL_VAL_EFUSE_VREF) ESP_LOGI(TAG,"ใช้การปรับเทียบแบบ eFuse Vref");
    else ESP_LOGI(TAG,"ใช้การปรับเทียบแบบ Default Vref");
}

void app_main(void){
    check_efuse();

    adc1_config_width(ADC_WIDTH_BIT_12);
    // ESP-IDF v5.5: ใช้ DB_12 (DB_11 ถูก deprecate แต่ค่าพฤติกรรมเท่ากัน)
    adc1_config_channel_atten(POTENTIOMETER_CHANNEL, ADC_ATTEN_DB_12);

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t vt = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12,
                                                      ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(vt);

    ESP_LOGI(TAG,"เริ่มอ่านค่า Potentiometer ที่ GPIO34 (ADC1_CH6)");

    while (1) {
        uint32_t adc_reading = 0;
        for (int i=0;i<NO_OF_SAMPLES;i++){
            adc_reading += adc1_get_raw((adc1_channel_t)POTENTIOMETER_CHANNEL);
        }
        adc_reading /= NO_OF_SAMPLES;

        uint32_t mv = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        float v = mv/1000.0f;
        float pct = (adc_reading/4095.0f)*100.0f;

        ESP_LOGI(TAG,"ADC: %d  |  V: %.2f V  |  %.1f %%", adc_reading, v, pct);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
