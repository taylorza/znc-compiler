#include "znc.h"
#include "farcall.h"
#include "strtbl.h"

int16_t far_lookupstr(const char* s) MYCC;
void far_dump_strings(void) MYCC;

// Wrappers: switch to BANK 40, call far implementations, switch back
int16_t lookupstr(const char* s) MYCC {
    PROLOG(40)
    int16_t i = far_lookupstr(s);
    EPILOG_RETURN(i);
}

void dump_strings(void) MYCC {
    PROLOG(40)
    far_dump_strings();
    EPILOG
}
