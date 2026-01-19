#include "znc.h"
#include "farcall.h"

void far_get_error_msg(ERROR err, char *buf, uint8_t bufsize) MYCC;

/* Stub function in shared memory */
void get_error_msg(ERROR err, char *buf, uint8_t bufsize) MYCC {
    PROLOG(42)
    far_get_error_msg(err, buf, bufsize);
    EPILOG
}
