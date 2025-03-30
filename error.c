#include "znc.h"

int errcnt = 0;
void error(ERROR err) {
    static const char* errmsg[] = {
        "Syntax",
        "Expect '\"'",
        "Expect '('",
        "Expect ')'",
        "Expect '{'",
        "Expect '}'",
        "Expect '['",
        "Expect ']'",
        "Expect ';'",
        "Too long",
        "Too many symbols",
        "Aready defined",
        "Not defined",
        "Not an lvalue",
        "Type Error",
        "File error",
    };
   
    printf("%c%s(%d,%d): error: %s%c", NL, loc[fileid].filename, token_line, token_col, errmsg[err], NL);
    exit(1);
    ++errcnt;
}