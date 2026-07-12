#include "znc.h"

const char *code;
uint8_t fileid = 255;
SOURCEPOS loc[MAX_NEST_FILE];

char token[MAX_STR_LEN+1];
uint16_t token_line;
uint8_t token_col;
TOKEN_TYPE token_type;
TOKEN tok;
uint8_t token_length;

int16_t intval;
uint8_t in_asm_block = 0;
uint8_t token_line_start_col = 1;

void enter_asm_block(void) MYCC {
    ++in_asm_block;
}

void exit_asm_block(void) MYCC {
    if (in_asm_block == 0) error(errSyntax);
    --in_asm_block;
}

typedef struct KEYWORD {
    char *name;
    TOKEN tok;
} KEYWORD;
/* Keyword table and lookup live in BANK_42 (scannerdata.c) to save shared memory. */
#include "farcall.h"

/* Declare the banked implementation. The actual definition is in scannerdata.c
 * which is compiled into BANK_42. The shared-memory wrapper below switches
 * to BANK_42, calls the banked function, and returns the result.
 */

TOKEN far_lookup_keyword(const char* ident) MYCC;
TOKEN far_ident_token(const char* ident) MYCC;

/* Shared-memory stub that forwards to the banked implementation. */
TOKEN lookup_keyword(const char* ident) MYCC {
    PROLOG(42)
        TOKEN t = far_lookup_keyword(ident);
    EPILOG_RETURN(t)
}

/* Wrapper to call the banked ident->token mapper. */
TOKEN lookup_ident_token(const char* ident) MYCC {
    PROLOG(42)
        TOKEN t = far_ident_token(ident);
    EPILOG_RETURN(t)
}

uint8_t far_src_open(const char* filename) MYCC;
void far_src_close(void) MYCC;
void far_src_closeall(void) MYCC;

uint8_t src_open(const char* filename) MYCC {
    PROLOG(42)
    uint8_t result = far_src_open(filename);
    EPILOG_RETURN(result)
}

void src_close(void) MYCC {
    PROLOG(42)
    far_src_close();
    EPILOG
}

void src_closeall(void) MYCC {
    PROLOG(42)
    far_src_closeall();
    EPILOG
}

uint8_t src_read(void) MYCC {
    SOURCEPOS *src = &loc[fileid];
    errno = 0;
#ifdef __ZXNEXT
    src->len = esxdos_f_read(src->handle, src->buf, MAX_READ_BUF);
#else
    src->len = (uint8_t)fread(src->buf, 1, MAX_READ_BUF, src->handle);
#endif
    if (src->len < MAX_READ_BUF) src->buf[src->len] = '\0';
    src->ofs = 0;
    code = &src->buf[0];
    return errno == 0;
}


TOKEN_TYPE far_get_token(void) MYCC;

TOKEN_TYPE get_token(void) MYCC {
    TOKEN_TYPE t;
    PROLOG(42)
        t = far_get_token();
    EPILOG_RETURN(t)
}

void far_parse_asm(void) MYCC;

void parse_asm_block(void) MYCC {
    PROLOG(42)
        far_parse_asm();
    EPILOG
}

void expect(TOKEN t, char ch) {
    if (tok == t) {
        get_token();
    } else {
        error(errExpected_c, ch);
    }
}

void expect_colon(void) MYCC {
    expect(tokColon, ':');
}

void expect_semi(void) MYCC {
    expect(tokSemi, ';');
}

void expect_comma(void) MYCC {
    expect(tokComma, ',');
}

void expect_LParen(void) MYCC {
    expect(tokLParen, '(');
}

void expect_RParen(void) MYCC {
    expect(tokRParen, ')');
}

void expect_LBrace(void) MYCC {
    expect(tokLBrace, '{');
}

void expect_RBrace(void) MYCC {
    expect(tokRBrace, '}');
}