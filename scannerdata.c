/* Banked scanner data (BANK_42) to save shared memory */
#include "znc.h"

typedef struct KEYWORD {
    char *name;
    TOKEN tok;
} KEYWORD;

KEYWORD keywords[] = {
    {"const", tokConst},
    {"void", tokVoid},
    {"char", tokChar},
    {"byte", tokChar},
    {"int", tokInt},
    {"va_list", tokInt},
    {"if", tokIf},
    {"else", tokElse},
    {"switch", tokSwitch},
    {"case", tokCase},
    {"default", tokDefault},
    {"while", tokWhile},
    {"for", tokFor},
    {"break", tokBreak},
    {"continue", tokContinue},
    {"return", tokReturn},
    {"exit", tokExit},
    {"putc", tokPutc},
    {"puts", tokPuts},
    {"in", tokIn},
    {"out", tokOut},
    {"nextreg", tokNextReg},
    {"readreg", tokReadReg},
    {"va_start", tokVaStart},
    {"va_arg", tokVaArg},
    {"va_end", tokVaEnd},
    {"__asm__", tokAsm},
    {"include", tokInclude},
    {"struct", tokStruct},
    {"delegate", tokDelegate},
    {"make", tokMake},
    {"raw", tokRaw},
    {"dot", tokDot},
    {"nex", tokNex},
    {"setsp", tokSetStack},
    {"org", tokOrg},
    {"bank", tokBank},
    {"#if", tokHashIf},
    {"#ifdef", tokHashIfDef},
    {"#ifndef", tokHashIfNDef},
    {"#else", tokHashElse},
    {"#endif", tokHashEndif},
};

/* Banked implementation of keyword lookup. Called from shared memory via
 * a small wrapper in scanner.c that switches to BANK_42.
 */
TOKEN far_lookup_keyword(const char* ident) MYCC {
    for (int i = 0; i < (int)(sizeof(keywords) / sizeof(KEYWORD)); ++i) {
        if (strncmp(ident, keywords[i].name, MAX_IDENT_LEN) == 0) return keywords[i].tok;
    }
    return tokNone;
}

/* Banked helpers and the banked get_token implementation. These access
 * shared globals (declared extern) so the banked code can update the
 * scanner state in shared memory while executing in BANK_42.
 */
#include "farcall.h"

/* Shared globals live in scanner.c; declare them extern here so the
   banked code can reference them. */
extern const char *code;
extern uint8_t fileid;
extern SOURCEPOS loc[MAX_NEST_FILE];

extern char token[MAX_STR_LEN+1];
extern uint16_t token_line;
extern uint8_t token_col;
extern TOKEN_TYPE token_type;
extern TOKEN tok;
extern uint8_t token_length;
extern int16_t intval;

extern uint8_t src_read(void) MYCC;

/* Debug instrumentation removed */

static uint8_t is_base_digit(char c, uint8_t base) MYCC {
    c = tolower((unsigned char)c);
    /* Fast direct checks avoid relying on a data table that may be relocated
     * in banked memory. Return 0..base-1 or 255 if not a digit for that base.
     */
    if (c >= '0' && c <= '9') {
        uint8_t v = (uint8_t)(c - '0');
        if (v < base) return v;
        return 255;
    }
    if (c >= 'a' && c <= 'f') {
        uint8_t v = 10 + (uint8_t)(c - 'a');
        if (v < base) return v;
    }
    return 255;
}

static char gnc(void) MYCC {
    char c = *code;
    if (c) {
        ++code;
        loc[fileid].col++;
        loc[fileid].ofs++;

        if (loc[fileid].ofs >= loc[fileid].len)
            src_read();
    }
    return c;
}

static char ch(void) MYCC {
    if (!*code) 
        return gnc();
    return *code;
}

static void skipws(void) MYCC {
    char c;
    uint8_t nl_seen = 0;
    while ((c = ch()) && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
        nl_seen = 0;
        gnc();
        if (c == '\r') {
            if (ch() == '\n') gnc();
            nl_seen = 1;
        } else if (c == '\n') {
            if (ch() == '\r') gnc();
            nl_seen = 1;
        } 

        if (nl_seen) {
            loc[fileid].line++;
            loc[fileid].col = 1;
        }
    }
}

static uint8_t escape(void) MYCC {
    char c = 0;
    token_col = loc[fileid].col;
    gnc(); // skip '\\'
    switch (ch()) {
        case '\\':
        case '"':
            c = ch();
            break;
        case 't':
            c = '\t';
            break;
        case 'r': 
            c = '\r';
            break;
        case 'n':
            c = '\n';
            break;
        case '0':
            gnc();
            c = '\0';
            break;
        case 'x': {
            gnc(); // skip 'x'
            uint8_t hi = is_base_digit(ch(), 16);
            gnc();
            uint8_t lo = is_base_digit(ch(), 16);
            if (hi == 255 || lo == 255) error(errSyntax);
            c = (char)((hi << 4) | lo);
            break;
        }
        default:
            error(errSyntax);
            break;
    }
    gnc();    
    return c;
}

/* Banked implementation of get_token. Uses far_lookup_keyword (local)
 * instead of the shared `lookup_keyword` wrapper.
 */
TOKEN_TYPE far_get_token(void) MYCC {
    char *temp;
get_token_start:
    temp = &token[0];
    *temp = '\0';
    
    skipws();
    token_length = 0;
    token_line = loc[fileid].line;
    token_col = loc[fileid].col;
    
    char c = ch();

    if (c == '\0') {
        *temp = '\0';
        tok = tokEOS;       
        return (token_type = ttDelimiter);
    }
    
    /* Handle potential double char tokens */
    if (find_char_in_str("+-=!<>|&/", c)) {
        switch(c) {
            case '+':
                *temp++ = gnc(); /* skip '+' */
                tok = tokPlus;
                if (ch() == '+') {
                    *temp++ = gnc();
                    tok = tokInc;
                }
                break;
            case '-':
                *temp++ = gnc();
                tok = tokMinus;
                if (ch() == '-') {
                    *temp++ = gnc();
                    tok = tokDec;
                }
                break;
            case '/':
                *temp++ = gnc();
                tok = tokDiv; 
                if (ch() == '/') {
                    while ((c = ch()) && c != '\r' && c != '\n') 
                        gnc();
                    goto get_token_start;    
                }
                break;
            case '=':
                *temp++ = gnc();
                tok = tokAssign;
                if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokEq;                
                }
                break;
            case '!':
                *temp++ = gnc();
                tok = tokNot;
                if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokNeq;
                }
                break;
            case '<':
                *temp++ = gnc();
                tok = tokLt;
                if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokLeq;
                } else if (ch() == '<') {
                    *temp++ = gnc();
                    tok = tokShl;
                }
                break;
            case '>':
                *temp++ = gnc();
                tok = tokGt;
                if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokGeq;
                } else if (ch() == '>') {
                    *temp++ = gnc();
                    tok = tokShr;
                }
                break;
            case '|':
                *temp++ = gnc();
                tok = tokBitOr;
                if (ch() == '|') {
                    *temp++ = gnc();
                    tok = tokOr;
                }
                break;
            case '&':
                *temp++ = gnc();
                tok = tokBitAnd;
                if (ch() == '&') {
                    *temp++ = gnc();
                    tok = tokAnd;
                }
                break;
        }
        if (token[0]) {
            *temp = '\0';
            token_length = (uint8_t)(temp - &token[0]);
            return (token_type = ttDelimiter);
        }
    }

    /* Single char tokens */
    if (find_char_in_str("*%~^;,(){}[]?:.", c)) {
        *temp++ = gnc();
        *temp = '\0';
        token_type = ttDelimiter;
        switch(c) {
            case '*': tok = tokStar; break;            
            case '%': tok = tokMod; break;
            case '~': tok = tokBitNot; break;
            case '^': tok = tokBitXor; break;
            case ';': tok = tokSemi; break;
            case ',': tok = tokComma; break;
            case '(': tok = tokLParen; break;
            case ')': tok = tokRParen; break;
            case '{': token_type = ttBlock; tok = tokLBrace; break;
            case '}': token_type = ttBlock; tok = tokRBrace; break;
            case '[': tok = tokLBrack; break;
            case ']': tok = tokRBrack; break;
            case '?': tok = tokCond; break;
            case ':': tok = tokColon; break;
            case '.': 
                /* Check for ... (ellipsis) */
                if (ch() == '.' && *(code+1) == '.') {
                    *temp++ = gnc(); /* consume second '.' */
                    *temp++ = gnc(); /* consume third '.' */
                    *temp = '\0';
                    tok = tokEllipsis;
                } else {
                    tok = tokMember;
                }
                break;
            default:
                error(errSyntax);
                break;
        }
        token_length = (uint8_t)(temp - &token[0]);
        return token_type;
    }

    uint8_t l = MAX_IDENT_LEN; 
    if (c == '\'') {
        gnc(); /* skip '\'' */
        if (ch() == '\\') {
            c = escape();
            *temp++ = c;
            intval = (int16_t)c;
        } else {
            *temp++ = ch();
            intval = (int16_t)ch();
            gnc();
        }
        *temp = '\0';
        if (ch() != '\'') error(errSyntax);
        gnc(); /* skip '\'' */
        tok = tokNumber;
        return (token_type = ttNumber);
    } else if (c == '"') {
        l = MAX_STR_LEN;
        gnc(); /* skip '"' */
        while (--l && (c = ch()) && c != '"' && c != '\r' && c != '\n') {
            if (c == '\\') {
                *temp++ = escape();
            } else *temp++ = gnc();
        }
        *temp = '\0';
        if (l == 0) error(errTooLong);
        else if (c != '"') error(errExpected_c, '"');
        else gnc();
        tok = tokString;
        token_length = (uint8_t)(temp - &token[0]);
        return (token_type = ttString);
    }

    if (isdigit(c)) {
        intval = 0;
        uint8_t base = 10;

        /* Consume the first digit */
        char first = c;
        *temp++ = gnc(); /* consume first digit into token */
        intval = (first - '0');

        /* Handle 0x / 0b prefix after leading zero */
        if (first == '0') {
            char p = tolower(ch());
            if (p == 'x') { base = 16; *temp++ = gnc(); }
            else if (p == 'b') { base = 2; *temp++ = gnc(); }
        }

        /* Consume remaining digits for the chosen base */
        while (--l) {
            char nc = ch();
            uint8_t dv = is_base_digit(nc, base);
            if (dv == 255) break;
            intval = (intval * base) + dv;
            *temp++ = gnc();
        }
        if (l == 0) error(errSyntax);
        *temp = '\0';
        tok = tokNumber;
        token_length = (uint8_t)(temp - &token[0]);
        return (token_type = ttNumber);
    }

    if (isalpha(c) || c == '_' || (token_col == 1 && c == '#')) {
        if (c == '#') {
            *temp++ = gnc(); /* skip '#' */
        }
        while (--l && (c = ch()) && (isalnum(c) || c == '_')) {
            *temp++ = gnc();
        }
        *temp = '\0';

        /* call bank-local keyword lookup */
        tok = far_lookup_keyword(token);
        if (tok != tokNone) {
            return (token_type = ttKeyword);
        }

        tok = tokIdent;
        return (token_type = ttIdent);
    }

    *temp++ = gnc();
    *temp = '\0';
    tok = tokNone;
    token_length = (uint8_t)(temp - &token[0]);
    return (token_type = ttError);
}
