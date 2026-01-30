#include "cec_tx.h"
#include "cec_rx.h"
#include "cec_od.h"
#include "cec_timing.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"

#define CEC_TX_IDLE_US 5000   // バス送信前のアイドル確認時間

void cec_tx_init(uint gpio) {
    cec_od_init(gpio);
}

static void cec_wait_idle(uint32_t idle_us) {
    absolute_time_t t0 = get_absolute_time();
    while (absolute_time_diff_us(t0, get_absolute_time()) < (int64_t)idle_us) {
        if (!cec_od_read())
            t0 = get_absolute_time();
        tight_loop_contents();
    }
}

static inline void cec_send_symbol(bool one) {
    cec_od_drive_low();
    sleep_us(one ? CEC_T_BIT1_LOW : CEC_T_BIT0_LOW);
    cec_od_release();
    sleep_us(one ? CEC_T_BIT1_HIGH : CEC_T_BIT0_HIGH);
}

static inline void cec_send_start(void) {
    cec_od_drive_low();
    sleep_us(CEC_T_START_LOW);
    cec_od_release();
    sleep_us(CEC_T_START_HIGH);
}

static inline void cec_send_byte(uint8_t b, bool eom) {
    // 8 data bits (MSB first)
    for (int i = 7; i >= 0; i--) {
        cec_send_symbol(((b >> i) & 1u) != 0);
    }
    // EOM
    cec_send_symbol(eom);

    // ACK slot: 送信側は "1" を送る(=解放) のが基本。
    // 受信側がACKならここでLOWを引く。
    // ここではACKチェックを簡略化して「解放」だけする。
    cec_send_symbol(true);
}

bool cec_tx_send_bytes(const uint8_t* bytes, size_t len) {
    if (!bytes || len == 0 || len > CEC_MAX_FRAME_BYTES) return false;

    // 送信中にRX ISRが割り込むとタイミングが崩れるので、割り込みを一時止める
    uint32_t irq = save_and_disable_interrupts();

    // バスアイドル(High)確認。Lowなら誰か送信中なのでやめる
    // ※ここは本来「一定期間High」等が必要。簡略化。
    if (!cec_od_read()) {
        restore_interrupts(irq);
        return false;
    }

    cec_send_start();
    for (size_t i = 0; i < len; i++) {
        bool eom = (i == (len - 1));
        cec_send_byte(bytes[i], eom);
    }

    restore_interrupts(irq);
    return true;
}

bool cec_tx_send(const uint8_t *bytes, size_t len) {
    cec_wait_idle(CEC_TX_IDLE_US);
    return cec_tx_send_bytes(bytes, len);
}
