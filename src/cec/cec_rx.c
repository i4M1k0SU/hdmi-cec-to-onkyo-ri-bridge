#include "cec_rx.h"
#include "cec_od.h"
#include "cec_timing.h"
#include <string.h>
#include "pico/time.h"
#include "hardware/sync.h"

// RX 受信判定窓 (µs) — TX タイミングに対して実測マージンを含む
#define CEC_RX_BIT1_LOW_MIN     450
#define CEC_RX_BIT1_LOW_MAX     950
#define CEC_RX_BIT0_LOW_MIN    1200
#define CEC_RX_BIT0_LOW_MAX    1900
#define CEC_RX_START_LOW_MIN   3400
#define CEC_RX_START_LOW_MAX   4200

typedef enum { SYM_0=0, SYM_1=1, SYM_START=2, SYM_INVALID=3 } sym_t;

static inline sym_t classify_low(uint32_t us) {
    if (us >= CEC_RX_START_LOW_MIN && us <= CEC_RX_START_LOW_MAX) return SYM_START;
    if (us >= CEC_RX_BIT0_LOW_MIN  && us <= CEC_RX_BIT0_LOW_MAX)  return SYM_0;
    if (us >= CEC_RX_BIT1_LOW_MIN  && us <= CEC_RX_BIT1_LOW_MAX)  return SYM_1;
    return SYM_INVALID;
}

// ---- ACK制御 ----
static bool     g_ack_enabled = false;
static uint8_t  g_logical_addr = 0x05;
static volatile bool g_ack_holding = false;
static alarm_id_t g_ack_alarm = -1;

static volatile uint32_t g_ack_hold_us = 700;

// ACK解放後の自己エッジを読み飛ばすフラグ
static volatile bool g_skip_next_rise = false;

static int64_t ack_release_cb(alarm_id_t id, void* user_data) {
    (void)id; (void)user_data;
    cec_od_release();
    g_ack_holding = false;
    g_ack_alarm = -1;
    return 0;
}

static inline void ack_hold_start(void) {
    if (g_ack_holding) return;

    g_ack_holding = true;
    cec_od_drive_low();

    if (g_ack_alarm >= 0) {
        cancel_alarm(g_ack_alarm);
        g_ack_alarm = -1;
    }

    g_ack_alarm = add_alarm_in_us((int64_t)g_ack_hold_us, ack_release_cb, NULL, true);

    // ACK解放時の立ち上がりエッジをデータビットと誤認しないよう、
    // 次の立ち上がりエッジを1回読み飛ばす
    g_skip_next_rise = true;
}

// ---- フレーム格納（ISR→main受け渡し）----
static volatile bool g_frame_ready = false;
static volatile uint8_t g_frame_len = 0;
static volatile uint8_t g_frame_bytes[CEC_MAX_FRAME_BYTES];

// ---- ISR内部の逐次復号状態 ----
static volatile uint64_t g_last_edge_us = 0;
static volatile bool     g_last_level = true;

static uint8_t s_cur = 0;
static int     s_bitpos = 0;       // 0..7 data, 8=EOM, 9=ACK slot
static bool    s_eom = false;

static uint8_t s_buf[CEC_MAX_FRAME_BYTES];
static uint8_t s_len = 0;
static bool    s_in_frame = false;
static bool    s_first_byte = true;
static bool    s_addressed_to_us = false;  // 現フレームが自分宛てか
static uint8_t s_header = 0;

static inline bool should_ack_header(uint8_t header_byte) {
    uint8_t dst = header_byte & 0x0F;
    return g_ack_enabled && (dst == (g_logical_addr & 0x0F));
}

static void cec_irq(uint gpio, uint32_t events) {
    if (gpio != cec_od_gpio()) return;

    uint64_t now = time_us_64();
    bool level = cec_od_read();

    // 立ち上がりで直前LOW幅を確定
    if ((events & GPIO_IRQ_EDGE_RISE) && (g_last_level == false)) {

        // 自分の ACK 解放による立ち上がりエッジは読み飛ばす
        if (g_skip_next_rise) {
            g_skip_next_rise = false;
            g_last_level = level;
            g_last_edge_us = now;
            return;
        }

        uint32_t low_us = (uint32_t)(now - g_last_edge_us);
        sym_t sym = classify_low(low_us);

        if (sym == SYM_START) {
            s_in_frame = true;
            s_len = 0;
            s_cur = 0;
            s_bitpos = 0;
            s_eom = false;
            s_first_byte = true;
            s_addressed_to_us = false;
            s_header = 0;
            g_skip_next_rise = false;
        } else if (s_in_frame && (sym == SYM_0 || sym == SYM_1)) {
            if (s_bitpos < 8) {
                s_cur <<= 1;
                if (sym == SYM_1) s_cur |= 1;
                s_bitpos++;
            } else if (s_bitpos == 8) {
                s_eom = (sym == SYM_1);
                s_bitpos++;
            } else {
                // ACKスロット
                bool do_ack = false;
                if (s_first_byte) {
                    s_addressed_to_us = should_ack_header(s_cur);
                    do_ack = s_addressed_to_us;
                } else {
                    // 自分宛てフレームの2バイト目以降のみACK
                    do_ack = g_ack_enabled && s_addressed_to_us;
                }

                if (do_ack) {
                    ack_hold_start();
                }

                if (s_len < sizeof(s_buf)) s_buf[s_len++] = s_cur;

                if (s_first_byte) {
                    s_header = s_cur;
                    s_first_byte = false;
                }

                s_cur = 0;
                s_bitpos = 0;

                if (s_eom) {
                    if (!g_frame_ready) {
                        g_frame_len = s_len;
                        for (uint8_t i = 0; i < s_len; i++) g_frame_bytes[i] = s_buf[i];
                        g_frame_ready = true;
                    }
                    s_in_frame = false;
                }
            }
        } else {
            // invalid
        }
    }

    g_last_level = level;
    g_last_edge_us = now;
}

void cec_rx_init(uint cec_gpio) {
    // NOTE: cec_od_init() は cec_tx_init() で行う。先に呼ぶこと。
    g_last_level = cec_od_read();
    g_last_edge_us = time_us_64();

    gpio_set_irq_enabled_with_callback(
        cec_gpio,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &cec_irq
    );
}

void cec_rx_set_logical_addr(uint8_t logical_addr) {
    g_logical_addr = (uint8_t)(logical_addr & 0x0F);
}

void cec_rx_enable_ack(bool enable) {
    g_ack_enabled = enable;
}

bool cec_rx_poll_frame(cec_frame_t* out) {
    if (!g_frame_ready) return false;

    uint32_t save = save_and_disable_interrupts();
    if (out) {
        out->len = g_frame_len;
        memcpy(out->bytes, (const void *)g_frame_bytes, g_frame_len);
    }
    g_frame_ready = false;
    restore_interrupts(save);
    return true;
}
