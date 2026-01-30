#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/types.h"

// GPIO 初期化 (cec_od_init を内包)。cec_rx_init より先に呼ぶこと。
void cec_tx_init(uint gpio);

// バスアイドル待ち + 送信（ACK厳密判定は簡略）
bool cec_tx_send(const uint8_t *bytes, size_t len);

// 送信のみ (アイドル待ちなし)
bool cec_tx_send_bytes(const uint8_t *bytes, size_t len);
