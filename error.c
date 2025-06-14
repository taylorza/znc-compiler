#include <stdarg.h>

#include "znc.h"

int errcnt = 0;
void error(ERROR err, ...) {
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
    };
    
    char buf[64];
    va_list v;
    va_start(v, err);
    vsnprintf(buf, sizeof(buf), (char*)errmsg[err], v);
    va_end(v);

    printf("%c%s(%d,%d): error: %s%c", NL, loc[fileid].filename, token_line, token_col, buf, NL);
    exit(1);
    ++errcnt;
}

void error_expect_const(void) {
    error(errExpected_s, "constant");
}