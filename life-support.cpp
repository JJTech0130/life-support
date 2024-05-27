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


void hdq_program_init(PIO pio, uint sm, uint offset, uint pin) {
    pio_gpio_init(pio, pin);
    pio_gpio_init(pio, 13);
    //pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    pio_sm_config c = hdq_slave_program_get_default_config(offset);
   // pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    sm_config_set_set_pins(&c, pin, 1);
    float div = clock_get_hz (clk_sys) * (1e-6 * 8); // 1/8 MHz
    sm_config_set_clkdiv (&c, div);
    sm_config_set_in_shift (
        &c,
        true,           // shift direction: right
        false,           // autopush: enabled
        9   // autopush threshold
    );

    // Output Shift Register configuration settings
    sm_config_set_out_shift (
        &c,
        true,           // shift direction: right
        false,           // autopull: enabled
        9   // autopull threshold
    );

    // configure the input and sideset pin groups to start at `pin_num`
    sm_config_set_in_pins (&c, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, 13, 1, true);
    sm_config_set_sideset_pins (&c, 13);
    sm_config_set_jmp_pin (&c, pin);
    pio_sm_init(pio, sm, offset, &c);
}

int main() {
    stdio_init_all();

    //sleep_ms(3000); // delay for 3 seconds

    // PIO Blinking example
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &hdq_slave_program);
    printf("Loaded program at %d\n", offset);
    // Make the HDQ pin pull-up when idle
    gpio_pull_up(HDQ_PIN);
    
    hdq_program_init(pio, 0, offset, HDQ_PIN);
    pio_sm_set_enabled(pio, 0, true);
    printf("HDQ Slave enabled\n");

    for (int i = 0; i < 3; i++) {
        //printf("Hello, world!\n");
        // Read from the HDQ FIFO
       // (uint8_t)(pio_sm_get_blocking (pio, 0) >> 24);  // shift response into bits 0..7
        uint32_t hdq_debug2 = pio_sm_get_blocking (pio, 0);
        printf("HDQ sent: 0x%32x\n", hdq_debug2);
        uint16_t hdq_debug = hdq_debug2 >> 16;
        //printf("HDQ sent: 0x%32x\n", hdq_debug);
        // Check bit 9 to see if it was a BREAK
        if (hdq_debug & 0x80) { // Valid
            hdq_debug = (hdq_debug & 0xFF00) >> 8;
            // Check read/write bit
            if (hdq_debug & 0x80) {
                hdq_debug = hdq_debug & 0x7F;
                printf("HDQ write: 0x%32x\n", hdq_debug);
            } else {
                hdq_debug = hdq_debug & 0x7F;
                printf("HDQ read: 0x%32x\n", hdq_debug);
            }
        } else {
            printf("HDQ BREAK\n");
        }

        // if (hdq_debug >= 0xfff00000) {
        //     printf("HDQ RESET\n");
        // } else {
        //     printf("HDQ sent: 0x%32x\n", hdq_debug);
        // }
        //printf("HDQ sent: %d\n", (uint8_t)(pio_sm_get_blocking (pio, 0) >> 24));
        //sleep_ms(10);
    }
    // Stop pio
    pio_sm_set_enabled(pio, 0, false);
}
