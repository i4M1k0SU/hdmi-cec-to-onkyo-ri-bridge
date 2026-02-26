#include "led.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#define LED_FLASH_MS 80  // フラッシュ点灯時間 (ms)

static uint g_gpio[LED_CH_COUNT];
static bool g_enabled = false;
static absolute_time_t g_off_time[LED_CH_COUNT];
static bool g_lit[LED_CH_COUNT];

// アクティブ LOW: LOW で点灯、HIGH で消灯
static inline void led_on(uint gpio) {
    gpio_put(gpio, 0);
}

static inline void led_off(uint gpio) {
    gpio_put(gpio, 1);
}

void led_init(uint gpio_cec_rx, uint gpio_cec_tx, uint gpio_ri_tx) {
    g_gpio[LED_CH_CEC_RX] = gpio_cec_rx;
    g_gpio[LED_CH_CEC_TX] = gpio_cec_tx;
    g_gpio[LED_CH_RI_TX]  = gpio_ri_tx;

    // いずれかが 0 なら無効
    if (gpio_cec_rx == 0 || gpio_cec_tx == 0 || gpio_ri_tx == 0) {
        g_enabled = false;
        return;
    }

    g_enabled = true;

    for (int i = 0; i < LED_CH_COUNT; i++) {
        gpio_init(g_gpio[i]);
        gpio_set_dir(g_gpio[i], true);
        led_off(g_gpio[i]);
        g_lit[i] = false;
        g_off_time[i] = nil_time;
    }
}

void led_flash(led_ch_t ch) {
    if (!g_enabled || ch >= LED_CH_COUNT) {
        return;
    }

    led_on(g_gpio[ch]);
    g_lit[ch] = true;
    g_off_time[ch] = make_timeout_time_ms(LED_FLASH_MS);
}

void led_update(void) {
    if (!g_enabled) {
        return;
    }

    absolute_time_t now = get_absolute_time();
    for (int i = 0; i < LED_CH_COUNT; i++) {
        if (g_lit[i] && absolute_time_diff_us(now, g_off_time[i]) <= 0) {
            led_off(g_gpio[i]);
            g_lit[i] = false;
        }
    }
}
