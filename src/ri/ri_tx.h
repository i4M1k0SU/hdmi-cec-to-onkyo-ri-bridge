#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "pico/types.h"

void ri_tx_init(uint ri_gpio);
bool ri_tx_send(uint16_t command);
