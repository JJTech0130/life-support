#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "hdq.pio.h"
#include "hardware/clocks.h"

#define HDQ_PIN 14
#define DFU_PIN 15

void hdq_slave_init(PIO pio, uint sm, uint offset, uint pin) {
    gpio_pull_up(pin);
    hdq_slave_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    printf("HDQ Slave enabled\n");
}

void hdq_slave_put_byte(PIO pio, uint sm, bool valid, uint8_t data) {
    uint32_t hdq_w = (data << 1) | valid; // valid bit is least significant
    pio_sm_put_blocking(pio, sm, hdq_w);

    // Wait for the FIFO to empty
    while (!pio_sm_is_tx_fifo_empty(pio, sm)) {
        tight_loop_contents();
    }
}

uint8_t hdq_slave_get_byte(PIO pio, uint sm) {
    while (true) {
        uint16_t hdq_r = pio_sm_get_blocking(pio, sm) >> 16;
        // Check bit 9 to see if it was a BREAK
        if (hdq_r & 0x80) { // Valid
            hdq_r = (hdq_r & 0xFF00) >> 8;
            return hdq_r;
        } else {
            // Clear TX FIFO
            pio_sm_clear_fifos(pio, sm);
        }
    }
}

void hdq_slave_handle(PIO pio, uint sm) {
    while (true) {
        uint8_t cmd = hdq_slave_get_byte(pio, sm);
        uint8_t addr = cmd & 0x7F;
        if (cmd & 0x80) {
            // Write
            hdq_slave_put_byte(pio, sm, false, 0x0);
            uint8_t data = hdq_slave_get_byte(pio, sm);
            printf("HDQ write: 0x%02x -> 0x%002x\n", addr, data);
            hdq_slave_put_byte(pio, sm, false, 0x0);
        } else {
            printf("HDQ read:  0x%02x\n", addr);
            hdq_slave_put_byte(pio, sm, true, addr); // echo the addr
        }
    }
}

int main() {
    stdio_init_all();


    PIO pio = pio0;
    uint offset = pio_add_program(pio, &hdq_slave_program);
    printf("Loaded program at %d\n", offset);
    
    hdq_slave_init(pio, 0, offset, HDQ_PIN);
    hdq_slave_handle(pio, 0);
}
