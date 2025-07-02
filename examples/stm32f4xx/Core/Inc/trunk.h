#ifndef TRUNK_H
#define TRUNK_H

#include <stdint.h>
#include <stdarg.h>

int trunk_transmit(uint8_t x);
void trunk_send_u8(uint8_t x);
void trunk_send_u32(uint32_t x);
void trunk_start_frame(uint32_t msg_id);

#define trunkf_send(X) _Generic((X),    \
                   int: trunk_send_u32, \
          unsigned int: trunk_send_u32, \
              uint32_t: trunk_send_u32, \
                                        \
                  char: trunk_send_u8,  \
         unsigned char: trunk_send_u8   \
              )(X)

#define TRUNKF_EXPAND_1(...) __VA_ARGS__
#define TRUNKF_EXPAND_2(...) TRUNKF_EXPAND_1(TRUNKF_EXPAND_1(__VA_ARGS__))
#define TRUNKF_EXPAND_4(...) TRUNKF_EXPAND_2(TRUNKF_EXPAND_2(__VA_ARGS__))
#define TRUNKF_EXPAND_8(...) TRUNKF_EXPAND_4(TRUNKF_EXPAND_4(__VA_ARGS__))
#define TRUNKF_EXPAND_16(...) TRUNKF_EXPAND_8(TRUNKF_EXPAND_8(__VA_ARGS__))

#define TRUNKF_PARENS ()
#define TRUNKF_FOR_EACH(macro, ...)                                    \
  __VA_OPT__(TRUNKF_EXPAND_16(TRUNKF_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define TRUNKF_FOR_EACH_HELPER(macro, a1, ...)                         \
  macro(a1)                                                            \
  __VA_OPT__(TRUNKF_FOR_EACH_AGAIN TRUNKF_PARENS (macro, __VA_ARGS__))
#define TRUNKF_FOR_EACH_AGAIN() TRUNKF_FOR_EACH_HELPER

#define __trunkf_eval(X) trunkf_send(X);
#define TRUNK_ESCAPE_SYMBOL 0x42
#define TRUNK_START_OF_FRAME 0x00
#define trunkf(Message, ...) do {                                                         \
            static __attribute__((section(".trunk"))) const char __trunk_msg[] = Message; \
            trunk_start_frame((uint32_t)&__trunk_msg);                                    \
            TRUNKF_FOR_EACH(__trunkf_eval, __VA_ARGS__)                                   \
        } while (0)

#endif /* !TRUNK_H */
