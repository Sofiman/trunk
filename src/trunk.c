#include "trunk.h"

#include <unistd.h>

__attribute__((weak)) int trunk_transmit(const uint8_t x)
{
  return write(STDOUT_FILENO, &x, 1);
}

void trunk_start_frame(uint32_t msg_id)
{
    trunk_transmit(TRUNK_ESCAPE_SYMBOL);
    trunk_transmit(TRUNK_START_OF_FRAME);
    trunk_send_u32(msg_id);
}

void trunk_send_u8(uint8_t x) {
    if (x == TRUNK_ESCAPE_SYMBOL)
        trunk_transmit(x++);
    trunk_transmit(x);
}

void trunk_send_u32(uint32_t x)
{
    // Force transmit in little endian
    trunk_send_u8((x >>  0) & 0xFF);
    trunk_send_u8((x >>  8) & 0xFF);
    trunk_send_u8((x >> 16) & 0xFF);
    trunk_send_u8((x >> 24) & 0xFF);
}

