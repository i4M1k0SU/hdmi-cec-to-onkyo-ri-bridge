#include "cec_tx.h"
#include "cec_tx.pio.h"
#include "cec_rx.h"
#include "cec_od.h"
#include "cec_timing.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#define CEC_TX_IDLE_US 5000   // バス送信前のアイドル確認時間

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
        if (!cec_od_read())
            t0 = get_absolute_time();
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
    if (!bytes || len == 0 || len > CEC_MAX_FRAME_BYTES) return false;

    // バス HIGH 確認 — LOW なら他者が送信中
    if (!cec_od_read()) return false;

    // ---- TX 開始 ----

    // RX GPIO IRQ を無効化 (PIO 波形でエッジ割込が誤発火するのを防ぐ)
    gpio_set_irq_enabled(g_cec_gpio,
                         GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

    // GPIO を PIO に切り替え
    gpio_set_function(g_cec_gpio, PIO_FUNCSEL_NUM(g_pio, g_cec_gpio));

    // SM リセット + 有効化
    pio_sm_clear_fifos(g_pio, g_sm);
    pio_sm_restart(g_pio, g_sm);
    pio_sm_exec(g_pio, g_sm, pio_encode_jmp(g_prog_offset));
    pio_sm_set_enabled(g_pio, g_sm, true);

    // Start ビット
    cec_tx_push_symbol(CEC_T_START_LOW, CEC_T_START_HIGH);

    // データバイト: 8ビット(MSB first) + EOM + ACK
    for (size_t i = 0; i < len; i++) {
        bool eom = (i == len - 1);
        uint8_t b = bytes[i];

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
    }

    // FIFO が空になるまで待機
    while (!pio_sm_is_tx_fifo_empty(g_pio, g_sm))
        tight_loop_contents();

    // SM が最後のシンボルを完了して pull block で停止するまで待機
    while (pio_sm_get_pc(g_pio, g_sm) != g_prog_offset)
        tight_loop_contents();

    // ---- TX 終了 ----

    pio_sm_set_enabled(g_pio, g_sm, false);

    // GPIO を SIO に戻す
    gpio_set_function(g_cec_gpio, GPIO_FUNC_SIO);

    // RX GPIO IRQ を再有効化
    gpio_set_irq_enabled(g_cec_gpio,
                         GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    return true;
}

bool cec_tx_send(const uint8_t *bytes, size_t len) {
    cec_wait_idle(CEC_TX_IDLE_US);
    return cec_tx_send_bytes(bytes, len);
}
