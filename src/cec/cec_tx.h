#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/types.h"

// NACK 時の最大リトライ回数 (CEC 仕様: 最大5回)
#define CEC_TX_MAX_RETRIES 5

// GPIO 初期化 (cec_od_init を内包)。cec_rx_init より先に呼ぶこと。
void cec_tx_init(uint gpio);

// バスアイドル待ち + 送信 (NACK 時は自動リトライ)
bool cec_tx_send(const uint8_t *bytes, size_t len);

// 送信のみ (アイドル待ちなし、リトライなし)
bool cec_tx_send_bytes(const uint8_t *bytes, size_t len);
