#ifndef RTL_H_
#define RTL_H_

#define FLAG_RTL_NONE    0x00
#define FLAG_RTL_INCLUDE 0x01
#define FLAG_RTL_INLINE  0x80

uint8_t inc_rtl(const char* fn) MYCC;
void dump_rtl(void) MYCC;

#endif //RTL_H_