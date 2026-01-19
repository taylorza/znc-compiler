#include <stdarg.h>

#include "znc.h"

/* External function to get error message from banked memory */
void get_error_msg(ERROR err, char *buf, uint8_t bufsize) MYCC;

int errcnt = 0;
void error(ERROR err, ...) {
    char errmsg[32];  /* Error message template buffer */
    char buf[64];     /* Final formatted message buffer */
    va_list v;
    
    /* Retrieve error message template from banked memory */
    get_error_msg(err, errmsg, sizeof(errmsg));
    
    /* Format the message with variadic arguments */
    va_start(v, err);
    vsnprintf(buf, sizeof(buf), errmsg, v);
    va_end(v);
    
    printf("%c%s(%d,%d): error: %s%c", NL, loc[fileid].filename, token_line, token_col, buf, NL);
    exit(1);
    ++errcnt;
}

void error_expect_const(void) {
    error(errExpected_s, "constant");
}