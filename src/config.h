#pragma once

// ============================================================
//  ユーザー設定 — 環境に合わせて変更してください
// ============================================================

// ---- GPIO ピン割り当て ----

#define CEC_GPIO   1   // HDMI CEC ライン (オープンドレイン)
#define RI_GPIO    0   // ONKYO RI ライン (3.5mm Tip)

// ---- インジケータ LED ----
// 0 を設定すると LED 機能を無効化 (通常の Pico / Pico 2 向け)
// XIAO RP2040 の場合: GP17=Red, GP16=Green, GP25=Blue
// WS2812 のようなアドレサブル LED は非対応

#define LED_CEC_RX_GPIO 17  // CEC 受信インジケータ
#define LED_CEC_TX_GPIO 16  // CEC 送信インジケータ
#define LED_RI_TX_GPIO  25  // RI 送信インジケータ
