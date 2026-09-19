#include <stdarg.h>

#include "znc.h"

/* External function to get error message from banked memory */
void get_error_msg(ERROR err, char *buf, uint8_t bufsize) MYCC;

void notify(ERROR err, uint8_t error, va_list v) {
    char errmsg[32];  /* Error message template buffer */
    char buf[64];     /* Final formatted message buffer */

    get_error_msg(err, errmsg, sizeof(errmsg));
    
    /* Format the message with variadic arguments */
    vsnprintf(buf, sizeof(buf), errmsg, v);
    
    if (fileid != 255)         
        printf("%c%s(%d,%d): %s: %s%c", NL, loc[fileid].filename, curr_line, curr_col, error ? "error" : "warn", buf, NL);
    else
        printf("%c%s: %s%c", NL, error ? "error" : "warn", buf, NL);
    if (error) exit(1);
}

void error(ERROR err, ...) {
    va_list v;

#ifdef __ZXNEXT0
    __asm
    db 0xfd, 0x00
        __endasm;
#endif 
    va_start(v, err);
    notify(err, 1, v);
    va_end(v);
}

void warn(ERROR err, ...) {
    va_list v;

#ifdef __ZXNEXT0
    __asm
        db 0xfd, 0x00
    __endasm;
#endif 
    va_start(v, err);
    notify(err, 0, v);
    va_end(v);
}
