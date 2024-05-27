#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "blink.pio.h"
#include "hdq.pio.h"
#include "hardware/clocks.h"

#define HDQ_PIN 14
#define DFU_PIN 15

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    printf("Blinking pin %d at %d Hz\n", pin, freq);
    

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}


void hdq_slave_init(PIO pio, uint sm, uint offset, uint pin) {
    gpio_pull_up(pin);
    hdq_slave_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    printf("HDQ Slave enabled\n");
}

uint8_t hdq_slave_get_byte(PIO pio, uint sm) {
    while (true) {
        uint16_t hdq_r = pio_sm_get_blocking(pio, sm) >> 16;
        // Check bit 9 to see if it was a BREAK
        if (hdq_r & 0x80) { // Valid
            hdq_r = (hdq_r & 0xFF00) >> 8;
            return hdq_r;
        } else {
            //printf("HDQ BREAK\n");
        }
    }
}

void hdq_slave_handle(PIO pio, uint sm) {
    while (true) {
        uint8_t cmd = hdq_slave_get_byte(pio, sm);
        uint8_t addr = cmd & 0x7F;
        if (cmd & 0x80) {
            // Write
            uint8_t data = hdq_slave_get_byte(pio, sm);
            printf("HDQ write: 0x%02x -> 0x%002x\n", addr, data);
        } else {
            printf("HDQ read:  0x%02x\n", addr);
        }
    }
}

int main() {
    stdio_init_all();

    //sleep_ms(3000); // delay for 3 seconds

    // PIO Blinking example
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &hdq_slave_program);
    printf("Loaded program at %d\n", offset);
    
    hdq_slave_init(pio, 0, offset, HDQ_PIN);
    //pio_sm_set_enabled(pio, 0, true);
    printf("HDQ Slave enabled\n");

    hdq_slave_handle(pio, 0);
   
    // Stop pio
    pio_sm_set_enabled(pio, 0, false);
}
