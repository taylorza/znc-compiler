#include "znc.h"
#include "farcall.h"

/* Error messages in banked memory (BANK_42) to save shared memory */
static const char* errmsg[] = {
    "Syntax",
    "Expected '%c'",
    "Too long",
    "Too many symbols",
    "Aready defined '%s'",
    "Not defined '%s'",
    "Not an lvalue",
    "Type Error",
    "File error",
    "Argument mismatch",
    "Definition mismatch",
    "Invalid %s",
    "Expected %s",
    "Out of memory in arena",
    "Too many types",
    "Too many pointer indirections",
};

/* Banked function to copy error message to buffer */
void far_get_error_msg(ERROR err, char *buf, uint8_t bufsize) MYCC {
    const char *msg = errmsg[err];
    strncpy(buf, msg, bufsize - 1);
    buf[bufsize - 1] = '\0';  /* Ensure null termination */
}
