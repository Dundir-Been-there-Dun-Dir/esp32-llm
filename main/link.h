#pragma once

// Text link from the ESP32-WROOM-32 on UART1: IO42 = RX (from WROOM GPIO17),
// IO41 = TX (to WROOM GPIO16), 115200 8N1. Every received line is printed and spoken.
// UART0 (GPIO 43/44) is left alone: it is the primary console.
void link_init(void);
