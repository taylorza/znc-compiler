#include "znc.h"

const char *code;
uint8_t fileid = 255;
SOURCEPOS loc[MAX_NEST_FILE];

char token[MAX_STR_LEN+1];
uint16_t token_line;
uint8_t token_col;
TOKEN_TYPE token_type;
TOKEN tok;

int16_t intval;

typedef struct KEYWORD {
    char name[MAX_IDENT_LEN + 1];
    TOKEN tok;
} KEYWORD;

KEYWORD keywords[] = {
    {"void", tokVoid},
    {"char", tokChar},
    {"byte", tokChar},
    {"int", tokInt},
    {"if", tokIf},
    {"else", tokElse},
    {"while", tokWhile},
    {"for", tokFor},
    {"break", tokBreak},
    {"continue", tokContinue},    
    {"return", tokReturn},
    
    {"putc", tokPutc},
    {"puts", tokPuts},

    {"in", tokIn},
    {"out", tokOut},
    {"nextreg", tokNextReg},
    {"readreg", tokReadReg},

    {"__asm__", tokAsm},
    {"include", tokInclude},

    {"make", tokMake},
    {"raw", tokRaw},
    {"dot", tokDot},
    {"nex", tokNex},
    {"setsp", tokSetStack},
    {"org", tokOrg},
    {"bank", tokBank},
};

static TOKEN lookup_keyword(const char* ident) {
    for (int i = 0; i < sizeof(keywords) / sizeof(KEYWORD); ++i) {
        if (strncmp(ident, keywords[i].name, MAX_IDENT_LEN) == 0) return keywords[i].tok;
    }
    return tokNone;
}

static uint8_t is_base_digit(char c, uint8_t base) MYCC {
    static char digits[] = "0123456789abcdef";
    c = tolower(c);
    for (uint8_t i = 0; i < base; ++i) {
        if (c == digits[i]) return i;
    }
    return 255;
}

static uint8_t src_read(void) MYCC {
    SOURCEPOS *src = &loc[fileid];
    errno = 0;
#ifdef __ZXNEXT
    src->len = esxdos_f_read(src->handle, src->buf, MAX_READ_BUF);
#else
    src->len = (uint8_t)fread(src->buf, 1, MAX_READ_BUF-1, src->handle);
#endif
    if (src->len < MAX_READ_BUF) src->buf[src->len] = '\0';
    src->ofs = 0;
    code = &src->buf[0];
    return errno == 0;
}

uint8_t src_open(const char* filename) MYCC {
    if (fileid + 1 == MAX_NEST_FILE) return 0;

    errno = 0;
#ifdef __ZXNEXT
    uint8_t handle = esxdos_f_open(filename, ESXDOS_MODE_R | ESXDOS_MODE_OE);
    if (errno) return 0;
#else
    FILE *handle = fopen(filename, "r");
    if (!handle) return 0;
#endif
    
    SOURCEPOS* src = &loc[++fileid];
    strncpy(src->filename, filename, MAX_FILENAME_LEN);
    src->handle = handle;
    src->line = 1;
    src->ofs = 0;
    src->col = 0;

    return src_read();
}

void src_close(void) MYCC {
    SOURCEPOS *src = &loc[fileid--];
#ifdef __ZXNEXT
    esxdos_f_close(src->handle);
#else
    fclose(src->handle);
#endif
    
    src->filename[0] = '\0';
    
    code = loc[fileid].buf + loc[fileid].ofs;
}

void src_closeall(void) MYCC {
    while (fileid != 255) src_close();
}

static uint8_t isws(char c) MYCC {
    return (c == ' ');
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
    while ((c = ch()) && c == ' ' || c == '\t' || c == '\r' || c == '\n') {
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
    gnc(); // skip '\'
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
        default:
            error(errSyntax);
            break;
    }
    gnc();    
    return c;
}

TOKEN_TYPE get_token(void) MYCC {
    char *temp;
get_token_start:
    temp = &token[0];
    *temp = '\0';
    
    skipws();
    token_line = loc[fileid].line;
    token_col = loc[fileid].col;
    
    char c = ch();

    if (c == '\0') {
        *temp = '\0';
        tok = tokEOS;        
        return (token_type = ttDelimiter);
    }
    
    // Handle potential double char tokens
    if (find_char_in_str("=!<>|&/", c)) {
        switch(c) {
            case '/':
                *temp++ = gnc(); // skip '=' 
                tok = tokDiv; 
                if (ch() == '/') {
                    while ((c = ch()) && c != '\r' && c != '\n') 
                        gnc();
                    goto get_token_start;    
                }
                break;
            case '=':
                *temp++ = gnc(); // skip '='
                tok = tokAssign;
                if (ch() == '=') {
                    *temp++ = gnc(); // skip '='
                    tok = tokEq;                
                }
                break;
            
            case '!':
                *temp++ = gnc(); // skip '!'
                tok = tokNot;
                if (ch() == '=') {
                    *temp++ = gnc(); // skip '='
                    tok = tokNeq;
                }
                break;

            case '<':
                *temp++ = gnc(); // skip '<'
                tok = tokLt;

                if (ch() == '=') {
                    *temp++ = gnc(); // skip '='
                    tok = tokLeq;
                }
                else if (ch() == '<') {
                    *temp++ = gnc(); // skip '<'
                    tok = tokShl;
                }
                break;

            case '>':
                *temp++ = gnc(); // skip '>'
                tok = tokGt;
                if (ch() == '=') {
                    *temp++ = gnc(); // skip '='
                    tok = tokGeq;
                } else if (ch() == '>') {
                    *temp++ = gnc(); // skip '>'
                    tok = tokShr;
                }
                break;

            case '|':
                *temp++ = gnc(); // skip '|'
                tok = tokBitOr;
                if (ch() == '|') {
                    *temp++ = gnc(); // skip '|'
                    tok = tokOr;
                }
                break;

            case '&':
                *temp++ = gnc(); // skip '&'
                tok = tokBitAnd;
                if (ch() == '&') {
                    *temp++ = gnc(); // skip '&'
                    tok = tokAnd;
                }
                break;
        }
        if (token[0]) {
            *temp = '\0';
            return (token_type = ttDelimiter);
        }
    }

    // Handle single char tokens
    if (find_char_in_str("+-*%^;,(){}[]?:", c)) {
        *temp++ = gnc();
        *temp = '\0';
        token_type = ttDelimiter;
        switch(c) {
            case '+': tok = tokPlus; break;
            case '-': tok = tokMinus; break;
            case '*': tok = tokStar; break;            
            case '%': tok = tokMod; break;
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
            default:
                error(errSyntax);
                break;
        }
        return token_type;
    }

    uint8_t l = MAX_IDENT_LEN; 
    if (c == '\'') {
        gnc(); // skip '
        if (ch() == '\\') {
            c = escape();
            *temp++ = c;
            intval = (int16_t)c;
        } 
        else {
            *temp++ = ch();
            intval = (int16_t)ch();
            gnc();
        }
        *temp = '\0';
        
        
        if (ch() != '\'') error(errSyntax);
        gnc(); // skip '
        tok = tokNumber;
        return (token_type = ttNumber);
    } else if (c == '"') {
        l = MAX_STR_LEN;
        gnc(); // skip '"'
        while (--l && (c = ch()) && c != '"' && c != '\r' && c != '\n') {
            if (c == '\\') {
                gnc();  // skip '\'
                switch(c = ch()) {
                    case '\\':
                    case '"': 
                        gnc(); 
                        *temp++ = c;
                        break;
                    case 't':
                        gnc();
                        *temp++ = '\t';
                        break;
                    case 'r':
                        gnc();
                        *temp++ = '\r';
                        break;
                    case 'n':
                        gnc();
                        *temp++ = '\n';
                        break;
                    default:
                        token_col = loc[fileid].col;
                        error(errSyntax);
                        break;
                }                
            } else *temp++ = gnc();
        }
        *temp = '\0';
        
        if (l == 0) error(errTooLong);
        else if (c != '"') error(errExpected_c, '"');
        else gnc();
        tok = tokString;
        return (token_type = ttString);
    }


    if (isdigit(c)) {
        intval = 0;
        uint8_t base = 10;

        *temp++ = c;
        intval = (intval * 10) + (c - '0');
        gnc();
        if (intval == 0) {
            c = tolower(ch());
            switch (c) {                
                case 'x': base = 16; *temp++ = gnc(); break;
                case 'b': base = 2; *temp++ = gnc(); break;
            }            
        }

        uint8_t digit_value;
        while (--l && (c = ch()) && (digit_value = is_base_digit(c, base))!=255) {
            intval = (intval * base) + digit_value;
            *temp++ = gnc();
        }
        if (l == 0) error(errSyntax);
        *temp = '\0';       
        tok = tokNumber;
        return (token_type = ttNumber);
    }

    if (isalpha(c) || c == '_') {                
        while (--l && (c = ch()) && (isalnum(c) || c == '_')) {
            *temp++ = gnc();
        }
        *temp = '\0';

        tok = lookup_keyword(token);
        if (tok != tokNone) {
            return (token_type = ttKeyword);
        }

        tok = tokIdent;
        return (token_type = ttIdent);
    }

    *temp++ = gnc();
    *temp = '\0';
    tok = tokNone;
    return (token_type = ttError);
}

void expect(TOKEN t, char ch) {
    if (tok == t) {
        get_token();
    } else {
        error(errExpected_c, ch);
    }
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