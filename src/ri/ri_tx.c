#include "ri_tx.h"
#include "pico/stdlib.h"

// RI protocol timing
#define RI_HEADER_MARK_US    3000
#define RI_HEADER_SPACE_US   1000
#define RI_BIT_MARK_US       1000
#define RI_BIT_ONE_SPACE_US  2000
#define RI_BIT_ZERO_SPACE_US 1000
#define RI_FOOTER_MARK_US    1000
#define RI_FRAME_GAP_MS      20
#define RI_FRAME_BITS        12

static uint g_ri_gpio;

static inline void ri_mark_us(uint32_t us) {
    gpio_put(g_ri_gpio, 1);
    sleep_us(us);
}

static inline void ri_space_us(uint32_t us) {
    gpio_put(g_ri_gpio, 0);
    sleep_us(us);
}

static void ri_send_header(void) {
    ri_mark_us(RI_HEADER_MARK_US);
    ri_space_us(RI_HEADER_SPACE_US);
}

static void ri_send_bit(bool one) {
    ri_mark_us(RI_BIT_MARK_US);
    ri_space_us(one ? RI_BIT_ONE_SPACE_US : RI_BIT_ZERO_SPACE_US);
}

static void ri_send_footer(void) {
    ri_mark_us(RI_FOOTER_MARK_US);
    ri_space_us(0);
    sleep_ms(RI_FRAME_GAP_MS);
}

void ri_tx_init(uint ri_gpio) {
    g_ri_gpio = ri_gpio;

    gpio_init(g_ri_gpio);
    gpio_set_dir(g_ri_gpio, true);
    gpio_put(g_ri_gpio, 0);
}

bool ri_tx_send(uint16_t command) {
    ri_send_header();

    uint16_t v = command;
    for (int i = 0; i < RI_FRAME_BITS; i++) {
        bool bit = (v & 0x800) != 0;
        v <<= 1;
        ri_send_bit(bit);
    }

    ri_send_footer();
    return true;
}
