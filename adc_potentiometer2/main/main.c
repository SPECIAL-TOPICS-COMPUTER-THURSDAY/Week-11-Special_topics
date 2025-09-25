// การทดลอง ADC แบบปรับปรุง: Oversampling + Moving Average (ESP-IDF v5.x)

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// legacy ADC driver + calibration (ยังใช้ได้ใน v5.x)
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

// กำหนด pin และพารามิเตอร์
#define SENSOR_CHANNEL   ADC1_CHANNEL_6   // GPIO34 (ADC1_CH6)
#define DEFAULT_VREF     1100             // mV (ใช้ค่า eFuse ถ้ามี, ไม่มีก็ fallback ค่านี้)
#define OVERSAMPLES      100              // จำนวนครั้งในการ oversample
#define FILTER_SIZE      10               // ขนาด Moving Average Filter

static const char *TAG = "ADC_ENHANCED";
static esp_adc_cal_characteristics_t *adc_chars;

// ตัวแปรสำหรับ Moving Average Filter
static float filterBuffer[FILTER_SIZE];
static int   filterIndex = 0;
static float filterSum = 0.0f;
static bool  filterInitialized = false;

static bool check_efuse(void)
{
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Two Point: รองรับ");
    } else {
        ESP_LOGI(TAG, "eFuse Two Point: ไม่รองรับ");
    }
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Vref: รองรับ");
    } else {
        ESP_LOGI(TAG, "eFuse Vref: ไม่รองรับ");
    }
    return true;
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "ใช้การปรับเทียบแบบ Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "ใช้การปรับเทียบแบบ eFuse Vref");
    } else {
        ESP_LOGI(TAG, "ใช้การปรับเทียบแบบ Default Vref");
    }
}

// อ่านค่าด้วย Oversampling (เฉลี่ยหลายครั้ง)
static float readADCOversampling(adc1_channel_t channel, int samples)
{
    uint64_t sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += (uint32_t)adc1_get_raw(channel);
        vTaskDelay(pdMS_TO_TICKS(1));  // หน่วงเล็กน้อยลดคอรเลชัน
    }
    return (float)sum / (float)samples;
}

// Moving Average Filter
static float movingAverageFilter(float newValue)
{
    if (!filterInitialized) {
        for (int i = 0; i < FILTER_SIZE; i++) filterBuffer[i] = newValue;
        filterSum = newValue * FILTER_SIZE;
        filterInitialized = true;
        return newValue;
    }

    filterSum -= filterBuffer[filterIndex];
    filterBuffer[filterIndex] = newValue;
    filterSum += newValue;
    filterIndex = (filterIndex + 1) % FILTER_SIZE;

    return filterSum / (float)FILTER_SIZE;
}

void app_main(void)
{
    // ตรวจสอบ eFuse calibration
    check_efuse();

    // ตั้งค่า ADC (ความละเอียด 12-bit)
    adc1_config_width(ADC_WIDTH_BIT_12);

    // ESP-IDF v5.5: ใช้ ADC_ATTEN_DB_12 (DB_11 ถูก deprecate แต่พฤติกรรมเทียบเท่า)
    adc1_config_channel_atten(SENSOR_CHANNEL, ADC_ATTEN_DB_12);

    // สร้างคาแรคเตอร์ของการคาลิเบรต
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type =
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    ESP_LOGI(TAG, "ทดสอบการปรับปรุงความแม่นยำ ADC");
    ESP_LOGI(TAG, "เทคนิค: Oversampling (%d) + Moving Average (N=%d)", OVERSAMPLES, FILTER_SIZE);
    ESP_LOGI(TAG, "Pin: GPIO34 (ADC1_CH6)");
    ESP_LOGI(TAG, "----------------------------------------");

    while (1) {
        // 1) อ่านแบบ raw
        uint32_t rawValue = (uint32_t)adc1_get_raw((adc1_channel_t)SENSOR_CHANNEL);

        // 2) Oversampling
        float oversampledValue = readADCOversampling((adc1_channel_t)SENSOR_CHANNEL, OVERSAMPLES);

        // 3) Moving Average
        float filteredValue = movingAverageFilter(oversampledValue);

        // แปลงเป็นแรงดัน (mV) แล้วเป็นโวลต์
        uint32_t raw_mv        = esp_adc_cal_raw_to_voltage(rawValue, adc_chars);
        uint32_t oversamp_mv   = esp_adc_cal_raw_to_voltage((uint32_t)oversampledValue, adc_chars);
        uint32_t filtered_mv   = esp_adc_cal_raw_to_voltage((uint32_t)filteredValue, adc_chars);

        float rawV       = raw_mv      / 1000.0f;
        float oversampV  = oversamp_mv / 1000.0f;
        float filteredV  = filtered_mv / 1000.0f;

        ESP_LOGI(TAG, "=== เปรียบเทียบการอ่าน ===");
        ESP_LOGI(TAG, "Raw         : %4u (%.3f V)", rawValue,       rawV);
        ESP_LOGI(TAG, "Oversampled : %4.1f (%.3f V)", oversampledValue, oversampV);
        ESP_LOGI(TAG, "Filtered    : %4.1f (%.3f V)", filteredValue,   filteredV);
        ESP_LOGI(TAG, " ");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
