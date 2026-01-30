#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

#define CEC_MAX_FRAME_BYTES 16

typedef struct {
    uint8_t bytes[CEC_MAX_FRAME_BYTES];
    uint8_t len;
} cec_frame_t;

void cec_rx_init(uint cec_gpio);
void cec_rx_set_logical_addr(uint8_t logical_addr);
void cec_rx_enable_ack(bool enable);

// フレームが1つ取れたら true
bool cec_rx_poll_frame(cec_frame_t* out);
