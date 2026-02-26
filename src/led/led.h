#pragma once
#include <stdbool.h>
#include "pico/types.h"

// LED チャンネル
typedef enum {
    LED_CH_CEC_RX = 0,
    LED_CH_CEC_TX = 1,
    LED_CH_RI_TX  = 2,
    LED_CH_COUNT
} led_ch_t;

// 初期化 (GPIO ピン番号を指定。0 なら無効化)
void led_init(uint gpio_cec_rx, uint gpio_cec_tx, uint gpio_ri_tx);

// 指定チャンネルを一瞬フラッシュ (メインループで消灯管理)
void led_flash(led_ch_t ch);

// メインループで毎周期呼ぶ — タイムアウトした LED を消灯
void led_update(void);
