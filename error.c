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
    };

    printf("Error: %s: %s (%d, %d)\n", errmsg[err], loc[fileid].filename, token_line, token_col);
    get_token();
    ++errcnt;
}