#include <stdio.h>
#include "pico/stdlib.h"
#include "hdq.h"
#include <malloc.h>

#define HDQ_PIN 14
#define DFU_PIN 15

int main() {
    stdio_init_all();


    PIO pio = pio0;
    uint offset = pio_add_program(pio, &hdq_slave_program);
    printf("Loaded program at %d\n", offset);
    
    hdq_slave_init(pio, 0, offset, HDQ_PIN);
    uint16_t *register_mem = (uint16_t *)malloc(256);
    uint16_t *control_mem = (uint16_t *)malloc(0x90);
    // Write zeros to memory
    for (int i = 0; i < 256; i++) {
        register_mem[i] = 0;
    }
    for (int i = 0; i < 0x90; i++) {
        control_mem[i] = 0;
    }

    control_mem[2] = 0x0106; // Version    
    register_mem[0x6/2] = register_mem[0x28/2] = 3017; // Temp: 301.8K = 82.13F
    register_mem[0x0A/2] = 0x2080; // Flags
    register_mem[0x32/2] = 306; // Charging current mA
    register_mem[0x12/2] = 1629; // Full capacity mAh
    register_mem[0x10/2] = register_mem[0x20/2] = register_mem[0x22/2] = 1629; // Remaining capacity mAh
    register_mem[0x14/2] = 0; // Average current mA
    register_mem[0x2C/2] = 100; // State of charge

    hdq_slave_handle(pio, 0, register_mem, control_mem);
}
