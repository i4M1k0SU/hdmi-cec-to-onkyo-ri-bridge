#pragma once
#include <stdbool.h>
#include "pico/stdlib.h"

// 擬似オープンドレイン：
// - drive_low(): 出力Lowでバスを引き下げ
// - release() : 入力(Hi-Z)でバスを解放（Highはプルアップで上がる）

void cec_od_init(uint gpio);
void cec_od_drive_low(void);
void cec_od_release(void);
bool cec_od_read(void);
uint cec_od_gpio(void);
