#include "znc.h"
#include "farcall.h"
#include "rtl.h"

uint8_t far_inc_rtl(const char* fn);
void far_dump_rtl(void);

// Wrapper for RTL functions compiled in BANK 42
uint8_t inc_rtl(const char* fn) MYCC {
    PROLOG(42)
    uint8_t r = far_inc_rtl(fn);
    EPILOG_RETURN(r);
}

void dump_rtl(void) MYCC {
    PROLOG(42)
    far_dump_rtl();
    EPILOG
}
