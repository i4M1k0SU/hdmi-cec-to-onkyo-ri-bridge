#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/watchdog.h"
#include "cec/cec_rx.h"
#include "cec/cec_tx.h"
#include "cec/cec_opcode.h"
#include "ri/ri_tx.h"
#include "ri/ri_code.h"

#define CEC_GPIO 6
#define RI_GPIO  8

// HDMI0 = 0.0.0.0
#define CEC_PHYS_ADDR_HI 0x00
#define CEC_PHYS_ADDR_LO 0x00

#define CEC_LA  CEC_ADDR_AUDIO_SYSTEM
#define CEC_BR  CEC_ADDR_BROADCAST

#define RI_DEBOUNCE_US        2000000
#define RI_INPUT_SEL_DELAY_MS 200
#define WATCHDOG_TIMEOUT_MS   5000

// ---- デバイス状態 ----

typedef struct {
    bool            power_on;
    bool            system_audio_mode;
    uint8_t         volume;      // 0-100 (仮想値, ONKYO実機とは非同期)
    bool            mute;
    absolute_time_t last_on;     // Power ON デバウンス用
    absolute_time_t last_off;    // Power OFF デバウンス用
} device_state_t;

static device_state_t g_state = {
    .power_on          = false,
    .system_audio_mode = false,
    .volume            = 30,
    .mute              = false,
};

// ---- ユーティリティ ----

static inline uint8_t hdr(uint8_t src, uint8_t dst) {
    return (uint8_t)((src << 4) | (dst & 0x0F));
}

// ---- CEC 送信ヘルパー ----

// Report Physical Address (broadcast)
static bool tx_report_physical_addr(void) {
    uint8_t m[] = { hdr(CEC_LA, CEC_BR), CEC_OP_REPORT_PHYSICAL_ADDRESS,
                    CEC_PHYS_ADDR_HI, CEC_PHYS_ADDR_LO, 0x05 };
    return cec_tx_send(m, sizeof m);
}

// Device Vendor ID (broadcast) — vendor = 0x000000 (unknown)
static bool tx_device_vendor_id(void) {
    uint8_t m[] = { hdr(CEC_LA, CEC_BR), CEC_OP_DEVICE_VENDOR_ID,
                    0x00, 0x00, 0x00 };
    return cec_tx_send(m, sizeof m);
}

// Set OSD Name
static bool tx_set_osd_name(uint8_t dst, const char *name) {
    uint8_t m[16];
    uint8_t n = 0;
    m[n++] = hdr(CEC_LA, dst);
    m[n++] = CEC_OP_SET_OSD_NAME;
    while (*name && n < sizeof m) {
        m[n++] = (uint8_t)*name++;
    }
    return cec_tx_send(m, n);
}

// CEC Version — 1.4 = 0x05
static bool tx_cec_version(uint8_t dst) {
    uint8_t m[] = { hdr(CEC_LA, dst), CEC_OP_CEC_VERSION, 0x05 };
    return cec_tx_send(m, sizeof m);
}

// Report Power Status — 0x00=ON, 0x01=Standby
static bool tx_report_power_status(uint8_t dst) {
    uint8_t m[] = { hdr(CEC_LA, dst), CEC_OP_REPORT_POWER_STATUS,
                    (uint8_t)(g_state.power_on ? 0x00 : 0x01) };
    return cec_tx_send(m, sizeof m);
}

// Feature Abort
static bool tx_feature_abort(uint8_t dst, uint8_t opcode, uint8_t reason) {
    uint8_t m[] = { hdr(CEC_LA, dst), CEC_OP_FEATURE_ABORT, opcode, reason };
    return cec_tx_send(m, sizeof m);
}

// Set System Audio Mode (broadcast)
static bool tx_set_system_audio_mode(bool on) {
    uint8_t m[] = { hdr(CEC_LA, CEC_BR), CEC_OP_SET_SYSTEM_AUDIO_MODE,
                    on ? 0x01 : 0x00 };
    return cec_tx_send(m, sizeof m);
}

// System Audio Mode Status (directed)
static bool tx_system_audio_mode_status(uint8_t dst, bool on) {
    uint8_t m[] = { hdr(CEC_LA, dst), CEC_OP_SYSTEM_AUDIO_MODE_STATUS,
                    on ? 0x01 : 0x00 };
    return cec_tx_send(m, sizeof m);
}

// Report Audio Status (directed)
static bool tx_report_audio_status(uint8_t dst) {
    uint8_t m[] = { hdr(CEC_LA, dst), CEC_OP_REPORT_AUDIO_STATUS,
                    (uint8_t)((g_state.mute ? 0x80 : 0x00) | (g_state.volume & 0x7F)) };
    return cec_tx_send(m, sizeof m);
}

// ---- RI アクションヘルパー ----

static void ri_power_off(device_state_t *s) {
    if (is_nil_time(s->last_off)
        || absolute_time_diff_us(s->last_off, get_absolute_time()) > RI_DEBOUNCE_US) {
        printf("=> RI Power OFF (0x%03X)\n", (unsigned)RI_POWER_OFF);
        ri_tx_send(RI_POWER_OFF);
        s->last_off          = get_absolute_time();
        s->power_on          = false;
        s->system_audio_mode = false;
    } else {
        printf("=> RI Power OFF suppressed (debounce)\n");
    }
}

static void ri_power_on(device_state_t *s, const char *tag) {
    if (is_nil_time(s->last_on)
        || absolute_time_diff_us(s->last_on, get_absolute_time()) > RI_DEBOUNCE_US) {
        printf("=> RI Power ON (0x%03X)%s\n", (unsigned)RI_POWER_ON, tag ? tag : "");
        ri_tx_send(RI_POWER_ON);
        sleep_ms(RI_INPUT_SEL_DELAY_MS);
        printf("=> RI Input Sel (0x%03X)\n", (unsigned)RI_INPUT_SEL);
        ri_tx_send(RI_INPUT_SEL);
        s->last_on  = get_absolute_time();
        s->power_on = true;
    }
}

static void ri_set_mute(device_state_t *s, bool mute) {
    s->mute = mute;
    if (mute) {
        printf("=> RI Mute (0x%03X)\n", (unsigned)RI_MUTE);
        ri_tx_send(RI_MUTE);
    } else {
        printf("=> RI Unmute (0x%03X)\n", (unsigned)RI_UNMUTE);
        ri_tx_send(RI_UNMUTE);
    }
}

// ---- CEC フレーム処理 ----

static void handle_cec_frame(const cec_frame_t *f, device_state_t *s) {
    // ログ出力
    printf("CEC RX len=%u:", f->len);
    for (uint8_t i = 0; i < f->len; i++) {
        printf(" %02X", f->bytes[i]);
    }
    printf("\n");

    if (f->len < 2) {
        printf("  polling\n\n");
        return;
    }

    uint8_t header = f->bytes[0];
    uint8_t opcode = f->bytes[1];
    uint8_t src = (header >> 4) & 0x0F;
    uint8_t dst = header & 0x0F;

    printf("  opcode=0x%02X (%s) src=%u dst=%u\n", opcode, cec_opcode_name(opcode), src, dst);

    // ======== Broadcast メッセージ ========
    if (dst == CEC_BR) {
        if (opcode == CEC_OP_STANDBY) {
            ri_power_off(s);
        }
        printf("\n");
        return;
    }

    // ======== 自分宛て以外は無視 ========
    if (dst != CEC_LA) {
        printf("  (not for us)\n\n");
        return;
    }

    // ======== 自分宛て メッセージ ========
    bool ok;
    bool handled = true;

    switch (opcode) {

    case CEC_OP_FEATURE_ABORT: // Feature Abort (受信)
        if (f->len >= 4) {
            printf("  Remote Feature Abort: opcode=0x%02X reason=0x%02X\n", f->bytes[2], f->bytes[3]);
        }
        break;

    // ---- 基本情報応答 (全CEC機器共通) ----

    case CEC_OP_GIVE_PHYSICAL_ADDRESS:
        ok = tx_report_physical_addr();
        printf("  TX Report Physical Address: %s\n", ok ? "OK" : "FAIL");
        break;

    case CEC_OP_GIVE_OSD_NAME:
        ok = tx_set_osd_name(src, "OnkyoRI-Bridge");
        printf("  TX Set OSD Name: %s\n", ok ? "OK" : "FAIL");
        break;

    case CEC_OP_GET_CEC_VERSION:
        ok = tx_cec_version(src);
        printf("  TX CEC Version (1.4): %s\n", ok ? "OK" : "FAIL");
        break;

    case CEC_OP_GIVE_DEVICE_POWER_STATUS:
        ok = tx_report_power_status(src);
        printf("  TX Report Power Status (%s): %s\n",
               s->power_on ? "ON" : "Standby", ok ? "OK" : "FAIL");
        break;

    case CEC_OP_GIVE_DEVICE_VENDOR_ID:
        ok = tx_device_vendor_id();
        printf("  TX Device Vendor ID: %s\n", ok ? "OK" : "FAIL");
        break;

    // ---- System Audio Control ----

    case CEC_OP_SYSTEM_AUDIO_MODE_REQUEST: {
        // オペランドあり → ON, なし → OFF
        bool on = (f->len >= 4);
        s->system_audio_mode = on;
        ok = tx_set_system_audio_mode(on);
        printf("  System Audio Mode Request -> %s: %s\n", on ? "ON" : "OFF", ok ? "OK" : "FAIL");

        // SAM ON = TVがオーディオ出力先としてこのデバイスを使う → 電源ON
        if (on && !s->power_on) {
            ri_power_on(s, " [SAM]");
        }
        break;
    }

    case CEC_OP_SET_SYSTEM_AUDIO_MODE: // directed to us
        if (f->len >= 3) {
            s->system_audio_mode = (f->bytes[2] != 0);
            printf("  System Audio Mode = %u\n", s->system_audio_mode ? 1 : 0);
            ok = tx_system_audio_mode_status(src, s->system_audio_mode);
            printf("  TX System Audio Mode Status: %s\n", ok ? "OK" : "FAIL");
        }
        break;

    case CEC_OP_GIVE_SYSTEM_AUDIO_MODE_STATUS:
        ok = tx_system_audio_mode_status(src, s->system_audio_mode);
        printf("  TX System Audio Mode Status (%u): %s\n", s->system_audio_mode ? 1 : 0, ok ? "OK" : "FAIL");
        break;

    case CEC_OP_GIVE_AUDIO_STATUS:
        ok = tx_report_audio_status(src);
        printf("  TX Report Audio Status (vol=%u mute=%u): %s\n", s->volume, s->mute ? 1 : 0, ok ? "OK" : "FAIL");
        break;

    case CEC_OP_SET_AUDIO_VOLUME_LEVEL: { // CEC 2.0
        if (f->len >= 3) {
            uint8_t new_vol = f->bytes[2] & 0x7F;
            uint8_t old_vol = s->volume;
            if (new_vol > 100) {
                new_vol = 100;
            }
            s->volume = new_vol;
            s->mute = false;
            printf("  Set Audio Volume Level: %u -> %u (no RI cmd)\n", old_vol, s->volume);
        }
        break;
    }

    // ---- User Control (音量連動) ----

    case CEC_OP_USER_CONTROL_PRESSED:
        if (f->len >= 3) {
            uint8_t ui = f->bytes[2];
            switch (ui) {
            case 0x41: // Volume Up
                if (s->volume < 100) {
                    s->volume += 2;
                }
                s->mute = false;
                printf("=> RI Vol Up (0x%03X) vol=%u\n", (unsigned)RI_VOL_UP, s->volume);
                ri_tx_send(RI_VOL_UP);
                break;
            case 0x42: // Volume Down
                if (s->volume >= 2) {
                    s->volume -= 2;
                }
                s->mute = false;
                printf("=> RI Vol Down (0x%03X) vol=%u\n", (unsigned)RI_VOL_DOWN, s->volume);
                ri_tx_send(RI_VOL_DOWN);
                break;
            case 0x43: // Mute Toggle
                ri_set_mute(s, !s->mute);
                break;
            case 0x65: // Mute Function (ミュートON)
                ri_set_mute(s, true);
                break;
            case 0x66: // Restore Volume (ミュート解除)
                ri_set_mute(s, false);
                break;
            case 0x40: // Power
            case 0x6C: // Power Off
                ri_power_off(s);
                break;
            case 0x6B: // Power On
                ri_power_on(s, "");
                break;
            default:
                printf("  UI command 0x%02X: not mapped\n", ui);
                break;
            }
        }
        break;

    case CEC_OP_USER_CONTROL_RELEASED:
        // 特に処理不要
        break;

    // ---- Standby (directed) ----

    case CEC_OP_STANDBY:
        ri_power_off(s);
        break;

    // ---- Abort (テスト用) ----

    case CEC_OP_ABORT:
        ok = tx_feature_abort(src, CEC_OP_ABORT, CEC_ABORT_REFUSED);
        printf("  TX Feature Abort (Abort): %s\n", ok ? "OK" : "FAIL");
        break;

    default:
        handled = false;
        break;
    }

    // 未対応 opcode → Feature Abort を返す
    if (!handled) {
        ok = tx_feature_abort(src, opcode, CEC_ABORT_UNRECOGNIZED);
        printf("  TX Feature Abort (0x%02X unrecognized): %s\n", opcode, ok ? "OK" : "FAIL");
    }

    printf("\n");
}

// ---- メイン ----

int main(void) {
    stdio_init_all();

    // USB CDC 接続待ち (最大5秒)
    absolute_time_t deadline = make_timeout_time_ms(5000);
    while (!stdio_usb_connected() && absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        sleep_ms(10);
    }

    printf("\nCEC->RI bridge (Audio System)\n");
    printf("CEC GPIO=%d  RI GPIO=%d\n", CEC_GPIO, RI_GPIO);

    cec_tx_init(CEC_GPIO);
    cec_rx_init(CEC_GPIO);
    cec_rx_set_logical_addr(CEC_LA);
    cec_rx_enable_ack(true);
    ri_tx_init(RI_GPIO);

    // バス安定待ち
    sleep_ms(5000);

    // ---- 論理アドレスネゴシエーション ----
    // ポーリング: src=dst=CEC_LA のヘッダのみ送信。
    // NACK (false) = アドレス空き → 使用可能。ACK (true) = 他デバイスが使用中。
    {
        uint8_t poll = hdr(CEC_LA, CEC_LA);
        bool addr_in_use = cec_tx_send_bytes(&poll, 1);
        if (addr_in_use) {
            printf("BOOT: LA %u is in use! Falling back to unregistered (15)\n", CEC_LA);
            cec_rx_set_logical_addr(CEC_ADDR_BROADCAST);
            cec_rx_enable_ack(false);
        } else {
            printf("BOOT: LA %u is free, claimed\n", CEC_LA);
        }
    }

    // ---- ブートアナウンス ----
    bool ok;
    ok = tx_report_physical_addr();
    printf("BOOT: Report Physical Address: %s\n", ok ? "OK" : "FAIL");

    ok = tx_device_vendor_id();
    printf("BOOT: Device Vendor ID: %s\n", ok ? "OK" : "FAIL");

    // power_on は false のまま — 初回の SAM Request で ri_power_on を発火させる

    // ---- ウォッチドッグ有効化 ----
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
    printf("Watchdog enabled (%d ms)\n", WATCHDOG_TIMEOUT_MS);

    // ---- メッセージループ ----
    while (true) {
        watchdog_update();

        cec_frame_t f = {0};
        if (!cec_rx_poll_frame(&f)) {
            tight_loop_contents();
            continue;
        }
        handle_cec_frame(&f, &g_state);
    }
}
