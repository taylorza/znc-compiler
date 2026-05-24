#include <stdarg.h>

#include "znc.h"

/* External function to get error message from banked memory */
void get_error_msg(ERROR err, char *buf, uint8_t bufsize) MYCC;

void error(ERROR err, ...) {
    static char errmsg[32];  /* Error message template buffer */
    static char buf[64];     /* Final formatted message buffer */
    va_list v;
#if ZNC_DEBUG
#  ifdef __ZXNEXT
    __asm
        db 0xfd, 0x00
    __endasm;
#  endif
#endif
  
    /* Retrieve error message template from banked memory */
    get_error_msg(err, errmsg, sizeof(errmsg));
    
    /* Format the message with variadic arguments */
    va_start(v, err);
    vsnprintf(buf, sizeof(buf), errmsg, v);
    va_end(v);
    
    printf("%c%s(%d,%d): error: %s%c", NL, loc[fileid].filename, token_line, token_col, buf, NL);
    exit(1);
}
