# HDMI CEC to ONKYO RI Bridge

Raspberry Pi Pico / Pico 2 用ファームウェア。TV の HDMI CEC コマンドを ONKYO アンプの RI (Remote Interactive) コマンドに変換し、CEC 非対応の ONKYO アンプを TV のリモコンで制御できるようにする。

## 機能

- CEC 論理アドレス 5 (Audio System) としてバスに参加
- 起動時の論理アドレスネゴシエーション (衝突時は fallback)
- System Audio Mode 対応 — TV が SAM を有効化すると ONKYO アンプを自動電源 ON
- 音量 Up/Down、ミュート ON/OFF/トグルを RI コマンドに変換
- 電源 ON/OFF (Standby) を RI コマンドに変換 (デバウンス付き)
- CEC TX: PIO ハードウェアによるビットバング + バイト単位 ACK 検出 + 自動リトライ
- CEC RX: GPIO エッジ割り込みによるビットデコード + 自動 ACK 応答
- インジケータ LED 対応 (CEC RX / CEC TX / RI TX で個別点滅)
- 5 秒ウォッチドッグタイマー
- USB CDC シリアルでデバッグログ出力

## 対応 CEC コマンド

| CEC コマンド | 動作 |
|---|---|
| Standby (broadcast/directed) | RI Power OFF |
| System Audio Mode Request | RI Power ON + Input Sel / OFF |
| User Control Pressed (Vol Up) | RI Vol Up |
| User Control Pressed (Vol Down) | RI Vol Down |
| User Control Pressed (Mute Toggle) | RI Mute / Unmute |
| User Control Pressed (Power On/Off) | RI Power ON / OFF |
| Give Physical Address | Report Physical Address 応答 |
| Give OSD Name | "OnkyoRI-Bridge" 応答 |
| Give Audio Status | Report Audio Status 応答 |
| その他 | Feature Abort (Unrecognized) |

## 回路

```
HDMI コネクタ                    Raspberry Pi Pico / Pico 2
┌────────────┐                 ┌──────────────────────┐
│ Pin 18 +5V ┼── D1 (1N5819) ►├─ VSYS               │
│ Pin 13 CEC ┼── R1 (220Ω) ───├─ GP9                 │
│ Pin 17 GND ┼─────────────────├─ GND                 │
└────────────┘                 │              GP7 ────├── R2 (100Ω) ── RI Tip (3.5mm)
                               │                      │               RI Sleeve ── GND
                               └──────────────────────┘
```

- **D1**: ショットキーバリアダイオード — HDMI +5V → VSYS 給電。USB との同時接続可 (OR-ing)
- **R1 (220Ω)**: CEC ライン保護抵抗
- **R2 (100Ω)**: RI ライン保護抵抗

## 設定

`src/config.h` で GPIO ピン割り当てとインジケータ LED を設定する。

```c
#define CEC_GPIO        9   // HDMI CEC ライン
#define RI_GPIO         7   // ONKYO RI ライン

#define LED_CEC_RX_GPIO 17  // CEC 受信インジケータ
#define LED_CEC_TX_GPIO 16  // CEC 送信インジケータ
#define LED_RI_TX_GPIO  25  // RI 送信インジケータ
```

LED の GPIO を `0` に設定すると LED 機能が無効化される。通常の Pico / Pico 2 など LED が搭載されていないボードではすべて `0` にすること。

### インジケータ LED

個別に制御可能な LED を 3 つ持つボード (例: XIAO RP2040) で動作確認済み。イベント発生時に該当 LED が短く (80ms) 点滅する。アクティブ LOW を想定。

| チャンネル | トリガー | XIAO RP2040 での色 |
|---|---|---|
| CEC RX | CEC フレーム受信時 | Red (GP17) |
| CEC TX | CEC フレーム送信時 | Green (GP16) |
| RI TX | RI コマンド送信時 | Blue (GP25) |

## ビルド

[Raspberry Pi Pico SDK 2.x](https://github.com/raspberrypi/pico-sdk) が必要。

```bash
# Pico (RP2040) 向け
cmake -G Ninja -B build -DPICO_BOARD=pico
cmake --build build

# Pico 2 (RP2350) 向け
cmake -G Ninja -B build -DPICO_BOARD=pico2
cmake --build build
```

### 書き込み

```bash
# BOOTSEL モード
picotool load build/hdmi-cec-to-onkyo-ri-bridge.uf2 -fx

# SWD プローブ経由
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000; program build/hdmi-cec-to-onkyo-ri-bridge.elf verify reset exit"
```

## デバッグ

USB CDC シリアル (115200bps) で全 CEC フレームと RI コマンドのログが出力される。

```
CEC->RI bridge (Audio System)
CEC GPIO=9  RI GPIO=7
BOOT: LA 5 is free, claimed
BOOT: Report Physical Address: OK
BOOT: Device Vendor ID: OK
Watchdog enabled (5000 ms)
CEC RX len=3: 05 70 10 00
  opcode=0x70 (System Audio Mode Request) src=0 dst=5
  System Audio Mode Request -> ON: OK
=> RI Power ON (0x1AF) [SAM]
=> RI Input Sel (0x1A0)
```

## RI コマンドコード

[参考: ONKYO RI コード一覧](https://gist.github.com/i4M1k0SU/28cb2893a50efe4e052c1de504d60032)

| コード | 機能 |
|--------|------|
| 0x1A0 | Input Select |
| 0x1A2 | Volume Up |
| 0x1A3 | Volume Down |
| 0x1A4 | Mute |
| 0x1A5 | Unmute |
| 0x1AE | Power OFF |
| 0x1AF | Power ON |

## License

MIT
