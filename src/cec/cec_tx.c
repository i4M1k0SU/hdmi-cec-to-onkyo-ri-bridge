#include "cec_tx.h"
#include "cec_tx.pio.h"
#include "cec_rx.h"
#include "cec_od.h"
#include "cec_timing.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#define CEC_TX_IDLE_US          5000 // バス送信前のアイドル確認時間
#define CEC_ACK_SAMPLE_DELAY_US 1050 // FIFO drain から ACK サンプルまで (µs)

static PIO  g_pio;
static uint g_sm;
static uint g_prog_offset;
static uint g_cec_gpio;

void cec_tx_init(uint gpio) {
    g_cec_gpio = gpio;
    cec_od_init(gpio);

    // PIO / SM / プログラムを確保
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &cec_tx_program, &g_pio, &g_sm, &g_prog_offset,
            gpio, 1, true)) {
        panic("CEC TX: no free PIO SM");
    }

    // SM 構成 (GPIO は SIO に戻して返す)
    cec_tx_program_init(g_pio, g_sm, g_prog_offset, gpio);
}

static void cec_wait_idle(uint32_t idle_us) {
    absolute_time_t t0 = get_absolute_time();
    while (absolute_time_diff_us(t0, get_absolute_time()) < (int64_t)idle_us) {
        if (!cec_od_read()) {
            t0 = get_absolute_time();
        }
        tight_loop_contents();
    }
}

// シンボル1個分 (LOW→HIGH) を FIFO に投入
// X_low = low_us - 3, X_high = high_us - 4 (PIO 命令オーバーヘッド補正)
static inline void cec_tx_push_symbol(uint32_t low_us, uint32_t high_us) {
    uint32_t word = ((low_us - 3u) << 16) | (high_us - 4u);
    pio_sm_put_blocking(g_pio, g_sm, word);
}

bool cec_tx_send_bytes(const uint8_t *bytes, size_t len) {
    if (!bytes || len == 0 || len > CEC_MAX_FRAME_BYTES) {
        return false;
    }

    // バス HIGH 確認 — LOW なら他者が送信中
    if (!cec_od_read()) {
        return false;
    }

    // dst=0xF → broadcast (HIGH=success), else directed (LOW=success)
    bool broadcast = (bytes[0] & 0x0F) == 0x0F;

    // ---- TX 開始 ----

    // RX GPIO IRQ を無効化 (PIO 波形でエッジ割込が誤発火するのを防ぐ)
    gpio_set_irq_enabled(g_cec_gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

    // GPIO を PIO に切り替え
    gpio_set_function(g_cec_gpio, PIO_FUNCSEL_NUM(g_pio, g_cec_gpio));

    // SM リセット + 有効化
    pio_sm_clear_fifos(g_pio, g_sm);
    pio_sm_restart(g_pio, g_sm);
    pio_sm_exec(g_pio, g_sm, pio_encode_jmp(g_prog_offset));
    pio_sm_set_enabled(g_pio, g_sm, true);

    // Start ビット
    cec_tx_push_symbol(CEC_T_START_LOW, CEC_T_START_HIGH);

    bool success = true;

    // データバイト: 8ビット(MSB first) + EOM + ACK (バイトごとに ACK 検出)
    for (size_t i = 0; i < len; i++) {
        bool eom = (i == len - 1);
        uint8_t b = bytes[i];

        // 8 データビット (MSB first)
        for (int bit = 7; bit >= 0; bit--) {
            if ((b >> bit) & 1u) {
                cec_tx_push_symbol(CEC_T_BIT1_LOW, CEC_T_BIT1_HIGH);
            } else {
                cec_tx_push_symbol(CEC_T_BIT0_LOW, CEC_T_BIT0_HIGH);
            }
        }

        // EOM ビット
        if (eom) {
            cec_tx_push_symbol(CEC_T_BIT1_LOW, CEC_T_BIT1_HIGH);
        } else {
            cec_tx_push_symbol(CEC_T_BIT0_LOW, CEC_T_BIT0_HIGH);
        }

        // ACK スロット: 送信側は "1" (解放) を送る
        cec_tx_push_symbol(CEC_T_BIT1_LOW, CEC_T_BIT1_HIGH);

        // ---- ACK サンプリング ----

        // FIFO empty 待ち (PIO が ACK ワードを pull した直後)
        while (!pio_sm_is_tx_fifo_empty(g_pio, g_sm)) {
            tight_loop_contents();
        }

        // ACK サンプルポイントまで待機
        sleep_us(CEC_ACK_SAMPLE_DELAY_US);

        // バス状態サンプリング (PIO が GPIO を所有中でも gpio_get は動作する)
        bool bus_low = !gpio_get(g_cec_gpio);

        // ACK 判定: directed は LOW=ACK, broadcast は HIGH=ACK (極性が逆)
        bool ack_ok = broadcast ? !bus_low : bus_low;

        // PIO が ACK シンボル完了して pull block で停止するまで待機
        while (pio_sm_get_pc(g_pio, g_sm) != g_prog_offset) {
            tight_loop_contents();
        }

        if (!ack_ok) {
            printf("  CEC TX NACK byte %u\n", (unsigned)i);
            success = false;
            break;
        }
    }

    // ---- TX 終了 ----

    pio_sm_set_enabled(g_pio, g_sm, false);

    // GPIO を SIO に戻す
    gpio_set_function(g_cec_gpio, GPIO_FUNC_SIO);

    // RX GPIO IRQ を再有効化
    gpio_set_irq_enabled(g_cec_gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    return success;
}

bool cec_tx_send(const uint8_t *bytes, size_t len) {
    for (int attempt = 0; attempt <= CEC_TX_MAX_RETRIES; attempt++) {
        cec_wait_idle(CEC_TX_IDLE_US);
        if (cec_tx_send_bytes(bytes, len)) {
            return true;
        }
    }
    return false;
}
