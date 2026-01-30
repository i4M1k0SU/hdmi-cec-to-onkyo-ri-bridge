#include "cec_od.h"

static uint g_cec_gpio;

void cec_od_init(uint gpio) {
    g_cec_gpio = gpio;

    gpio_init(g_cec_gpio);

    // 初期状態は解放（Hi-Z）
    gpio_set_dir(g_cec_gpio, false);

    // CECは基本プルアップが外部にあるが、テスト用に内部プルアップを有効にしておく
    // （外部プルアップを付けた場合でも大抵は問題にならない程度の弱さ）
    gpio_pull_up(g_cec_gpio);
}

void cec_od_drive_low(void) {
    // 出力Lowのみを使う（High出力は禁止）
    gpio_put(g_cec_gpio, 0);
    gpio_set_dir(g_cec_gpio, true);
}

void cec_od_release(void) {
    // 入力(Hi-Z)に戻す（外部プルアップでHighになる）
    gpio_set_dir(g_cec_gpio, false);
}

bool cec_od_read(void) {
    return gpio_get(g_cec_gpio);
}

uint cec_od_gpio(void) {
    return g_cec_gpio;
}
