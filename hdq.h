#include <stdio.h>
#include "hardware/pio.h"
#include "hdq.pio.h"

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

void hdq_slave_handle(PIO pio, uint sm, void *register_mem, void *control_mem) {
    while (true) {
        uint8_t cmd = hdq_slave_get_byte(pio, sm);
        uint8_t addr = cmd & 0x7F;
        if (cmd & 0x80) {
            // Write
            hdq_slave_put_byte(pio, sm, false, 0x0);
            uint8_t data = hdq_slave_get_byte(pio, sm);
            printf("HDQ write: 0x%02x -> 0x%002x\n", addr, data);
            ((uint8_t *)register_mem)[addr] = data;
            hdq_slave_put_byte(pio, sm, false, 0x0);
        } else {
            uint8_t data = ((uint8_t *)register_mem)[addr];
            if (addr <= 0x01) {
                // Special case for the control register
                // Byte swap the address
                uint16_t ctrl_addr = ((uint16_t *)register_mem)[0];
                //ctrl_addr = __builtin_bswap16(ctrl_addr);
                data = ((uint8_t *)control_mem)[ctrl_addr + addr];
                printf("HDQ control read: 0x%02x -> 0x%02x\n", ctrl_addr + addr, data);
            } else {
                printf("HDQ read:  0x%02x -> 0x%02x\n", addr, data);
            }
            hdq_slave_put_byte(pio, sm, true, data);
        }
    }
}