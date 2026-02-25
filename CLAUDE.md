# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HDMI CEC → Onkyo RI プロトコルブリッジ。Raspberry Pi Pico / Pico 2 上で動作し、TV からの CEC コマンド（音量・電源・ミュート等）を Onkyo アンプの RI コマンドに変換する。CEC 論理アドレス 5（Audio System）として振る舞う。

## Build

```bash
# 初回 configure（デフォルトは PICO_BOARD=pico）
cmake -G Ninja -B build

# Pico 2 (RP2350) 向けにビルドする場合
cmake -G Ninja -B build -DPICO_BOARD=pico2

# ビルド
cmake --build build

# 書き込み（BOOTSEL モード）
picotool load build/hdmi-cec-to-onkyo-ri-bridge.uf2 -fx

# 書き込み（SWD プローブ経由）
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000; program build/hdmi-cec-to-onkyo-ri-bridge.elf verify reset exit"
```

テストフレームワークは無い。ビルドが通ることと実機動作で検証する。

## Architecture

```
main.c  ── handle_cec_frame() でオペコードを振り分け
  ├── cec/cec_rx  (GPIO エッジ IRQ でビットデコード + アラーム ACK)
  ├── cec/cec_tx  (PIO ステートマシンで送信 + ACK サンプリング + リトライ)
  ├── cec/cec_od  (オープンドレイン GPIO 抽象化、RX/TX 共有)
  └── ri/ri_tx    (GPIO ビットバングで RI 12 ビット送信)
```

### CEC TX/RX が同一 GPIO を共有する仕組み

CEC バスは 1 本のオープンドレインラインで双方向通信する。TX 時は GPIO を PIO に切り替え (`gpio_set_function(PIO)`)、RX の GPIO IRQ を一時無効化する。TX 完了後に SIO に戻して IRQ を再有効化する。

### PIO プログラム (cec_tx.pio)

7 命令、1µs/cycle。FIFO から 32 ビットワード（上位 16 = LOW 相、下位 16 = HIGH 相）を消費してシンボルを生成。`.pio_version` 指定なし（v0 = RP2040/RP2350 両対応）。

### CEC TX ACK 検出

バイトごとに FIFO empty → `sleep_us(1050)` → `gpio_get()` でバス状態をサンプリング。Directed は LOW=ACK、Broadcast は HIGH=ACK（極性が逆）。NACK 時は最大 5 回リトライ。

### RI TX

ブロッキング `sleep_us()` で 12 ビット送信（~40ms）。送信中は CEC フレームを取りこぼす可能性がある。

## Conventions

- インデント: 4 スペース（`.editorconfig` 参照）
- 最大行長: 120 文字
- CEC オペコード名（ログ出力用文字列）は CEC 仕様準拠の英語表記のまま
