// Auto LED Brightness with LDR (ESP32 + ESP-IDF)
// - LDR divider -> ADC1_CH7 (GPIO35)
// - LED PWM     -> GPIO25 (LEDC)

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"         // legacy adc
#include "esp_adc_cal.h"        // calibration
#include "esp_log.h"
#include "esp_rom_sys.h"        // <<<< เพิ่มบรรทัดนี้

#include "driver/ledc.h"        // PWM

// ====== PINs & Params ======
#define LDR_CHANNEL        ADC1_CHANNEL_7   // GPIO35
#define LED_GPIO           25
#define DEFAULT_VREF       1100
#define OVERSAMPLES        64
#define MA_WINDOW          8
#define LEDC_FREQ_HZ       5000
#define LEDC_RES           LEDC_TIMER_12_BIT
#define USE_GAMMA          1
#define GAMMA              2.2f

static const char *TAG = "LED_LDR_CTRL";
static esp_adc_cal_characteristics_t *adc_chars;

// ---- Moving Average buffer ----
static float ma_buf[MA_WINDOW];
static int   ma_idx = 0;
static int   ma_filled = 0;
static float ma_sum = 0.0f;

static bool check_efuse(void)
{
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
        ESP_LOGI(TAG, "eFuse Two Point: OK");
    else
        ESP_LOGI(TAG, "eFuse Two Point: Not programmed");

    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
        ESP_LOGI(TAG, "eFuse Vref: OK");
    else
        ESP_LOGI(TAG, "eFuse Vref: Not programmed");
    return true;
}

static void ledc_setup_pwm(void)
{
    ledc_timer_config_t tcfg = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_RES,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&tcfg);

    ledc_channel_config_t ccfg = {
        .gpio_num       = LED_GPIO,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ccfg);
}

static inline uint32_t ledc_max_duty(void) { return (1u << 12) - 1u; }

static void led_set_brightness(float level_0to1)
{
    if (level_0to1 < 0) level_0to1 = 0;
    if (level_0to1 > 1) level_0to1 = 1;
#if USE_GAMMA
    level_0to1 = powf(level_0to1, GAMMA);
#endif
    uint32_t duty = (uint32_t)(level_0to1 * ledc_max_duty() + 0.5f);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static uint32_t adc_read_oversample(adc1_channel_t ch, int n)
{
    uint64_t sum = 0;
    for (int i = 0; i < n; i++) {
        sum += (uint32_t)adc1_get_raw(ch);
        // small delay helps decorrelate samples
        esp_rom_delay_us(200);  // <<<< แทน ets_delay_us(200);
    }
    return (uint32_t)(sum / (uint64_t)n);
}

static float ma_filter(float x)
{
    if (ma_filled < MA_WINDOW) {
        ma_buf[ma_idx++] = x;
        ma_sum += x;
        ma_filled++;
        if (ma_idx >= MA_WINDOW) ma_idx = 0;
        return ma_sum / (float)ma_filled;
    } else {
        ma_sum -= ma_buf[ma_idx];
        ma_buf[ma_idx] = x;
        ma_sum += x;
        ma_idx = (ma_idx + 1) % MA_WINDOW;
        return ma_sum / (float)MA_WINDOW;
    }
}

void app_main(void)
{
    check_efuse();

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_CHANNEL, ADC_ATTEN_DB_12);

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

    ledc_setup_pwm();
    ESP_LOGI(TAG, "Auto LED w/ LDR: ADC1_CH7(GPIO35) -> LED GPIO%d PWM", LED_GPIO);

    while (1) {
        uint32_t adc_raw = adc_read_oversample((adc1_channel_t)LDR_CHANNEL, OVERSAMPLES);
        float    smoothed = ma_filter((float)adc_raw);

        float light_norm = smoothed / 4095.0f;   // สว่าง -> ค่าใหญ่
        float led_level  = 1.0f - light_norm;    // มืด -> LED สว่าง

        led_set_brightness(led_level);

        uint32_t mv = esp_adc_cal_raw_to_voltage((uint32_t)smoothed, adc_chars);
        ESP_LOGI(TAG, "ADC raw(avg): %.0f  V: %.3f  light_norm: %.2f  LED level: %.2f  duty: %u/4095",
                 smoothed, mv/1000.0f, light_norm, led_level, (uint32_t)(led_level * ledc_max_duty()));

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
