#include "znc.h"
#include "farcall.h"

/* Error messages in banked memory (BANK_42) to save shared memory */
static const char* errmsg[] = {
    "Syntax",
    "Expected '%c'",
    "Too long",
    "Too many symbols",
    "Already defined '%s'",
    "Not defined '%s'",
    "Not an lvalue",
    "Type Error",
    "File error",
    "Argument mismatch",
    "Decl mismatch",
    "Invalid %s",
    "Expected %s",
    "Out of memory",
    "Too many types",
    "Too many indirections",
    "Return value expected",
    "Return value unexpected",
    "Bad escape",
    "Top level only",
    "Unexpected #else",
    "Unexpected #endif",
    "Too many args",
    "Function expected",
    "Const expected",
    "break outside loop",
    "continue outside loop",
    "Not a struct",
    "Illegal operation",
    "Variadics no callee cleanup",
};

/* Banked function to copy error message to buffer */
void far_get_error_msg(ERROR err, char *buf, uint8_t bufsize) MYCC {
    const char *msg = errmsg[err];
    strncpy(buf, msg, bufsize - 1);
    buf[bufsize - 1] = '\0';  /* Ensure null termination */
}
