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
    {"byte", tokByte},
    {"int", tokInt},
    {"fixed", tokFixed},
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
    {"enum", tokEnum},
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
    {"#elif", tokHashElif},
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
extern uint8_t in_asm_block;
extern uint8_t token_line_start_col;

extern uint8_t src_read(void) MYCC;

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
        curr_col++;
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
            
            curr_line++;
            curr_col = 1;
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
        case '\'':
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
            c = '\0';
            break;
        case 'x': {
            gnc(); // skip 'x'
            uint8_t hi = is_base_digit(ch(), 16);
            gnc();
            uint8_t lo = is_base_digit(ch(), 16);
            if (hi == 255 || lo == 255) error(errBadEscape);
            c = (char)((hi << 4) | lo);
            break;
        }
        default:
            error(errBadEscape);
            break;
    }
    gnc();    
    return c;
}

/* Helper: Skip nested block comments with depth tracking.
 * Starts with the first '/' already consumed (before the '/' of /*).
 * Expects to see '*' as the next character, then continues until depth reaches 0.
 */
static void skip_block_comment(void) MYCC {
    uint8_t depth = 1;
    char c;
    
    gnc(); /* skip '*' (we already have '/' consumed) */
    
    while ((c = gnc()) && depth > 0) {
        if (c == '/' && ch() == '*') {
            gnc(); /* skip '*' */
            depth++;
        } else if (c == '*' && ch() == '/') {
            depth--;
        } else if (c == '\r') {
            if (ch() == '\n') gnc();
            loc[fileid].line++;
            loc[fileid].col = 1;
            curr_line++;
            curr_col = 1;
        } else if (c == '\n') {
            if (ch() == '\r') gnc();
            loc[fileid].line++;
            loc[fileid].col = 1;
            curr_line++;
            curr_col = 1;
        }
    }
    
    if (depth > 0) {
        error(errExpected_s, "*/"); /* Unclosed block comment at EOF */
    }
}

/* Raw line-at-a-time inline assembly parser.
 * Reads character-by-character, stripping ; and // comments, and emits
 * each line verbatim with correct label/instruction indentation.
 * Strings are passed through verbatim (including escape sequences).
 * Number prefixes 0x/0b are converted to $/% outside of strings.
 * asmcol: column threshold - tokens at or below this column are labels.
 */
TOKEN_TYPE far_get_token(void) MYCC; /* forward declaration */

static void asm_advance_line(char c) MYCC {
    if (c == '\r' && ch() == '\n') gnc();
    else if (c == '\n' && ch() == '\r') gnc();
    loc[fileid].line++;
    loc[fileid].col = 1;
    curr_line++;
    curr_col = 1;
}

static void asm_emit_indent(volatile uint8_t *has_content, uint8_t line_col, uint8_t asmcol) MYCC {
    if (!*has_content) {
        *has_content = 1;
        if (line_col > asmcol) emit_str("  ");
    }
}

void far_parse_asm(void) MYCC {
    volatile uint8_t asmcol = token_line_start_col;
    for (;;) {
        /* Skip leading whitespace on each line, tracking newlines */
        char c;
        while ((c = ch()) && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
            if (c == '\r' || c == '\n') {
                gnc();
                asm_advance_line(c);
            } else {
                gnc();
            }
        }

        c = ch();
        if (c == '\0' || c == '}') break;

        volatile uint8_t line_col = loc[fileid].col;
        volatile uint8_t has_content = 0;
        char prev = 0;

        while ((c = ch()) && c != '\r' && c != '\n' && c != '}') {
            /* -- handle single quote string -- */
            if (c == '\'') {
                asm_emit_indent(&has_content, line_col, asmcol);
                char c1 = gnc();
                char c2 = gnc();
                char c3 = gnc();                
                if (c1 == '\'' && c3 == '\'') {
                    emit_n(c2); /* emit char literal 'x' as chr(x) */
                }
                else {
                    /* Not a valid char literal - emit as normal chars */                    
                    if (c1) emit_ch(c1);
                   
                    if (c2) {
                       emit_ch(c2);
                       if (c2 == '\r' || c2 == '\n') asm_advance_line(c2);                                                    
                    }

                    if (c3) {
                        emit_ch(c3);
                        if (c3 == '\r' || c3 == '\n') asm_advance_line(c3);
                    }
                }               
                continue;
            }

            /* -- string literal: pass through verbatim -- */
            if (c == '"') {
                asm_emit_indent(&has_content, line_col, asmcol);
                emit_ch('"'); gnc();/* emit opening quote */
                while ((c = ch()) && c != '\r' && c != '\n' && c != '"') {
                    if (c == '\\') {
                        emit_ch(gnc()); /* emit backslash */
                        if (ch() && ch() != '\r' && ch() != '\n')
                            emit_ch(gnc()); /* emit escaped char */
                    } else {
                        emit_ch(gnc());
                    }
                }
                if (c == '"') emit_ch('"'); gnc(); /* emit closing quote */
                prev = 0; /* treat end-of-string as non-alnum boundary */
                continue;
            }

            /* -- ; comment: discard rest of line -- */
            if (c == ';') {
                while ((c = ch()) && c != '\r' && c != '\n') gnc();
                break;
            }

            /* -- // comment: discard rest of line -- */
            if (c == '/') {
                gnc();
                if (ch() == '/') {
                    while ((c = ch()) && c != '\r' && c != '\n') gnc();
                    break;
                }
                asm_emit_indent(&has_content, line_col, asmcol);
                emit_ch('/');
                prev = '/';
                continue;
            }

            /* -- 0x / 0b prefix conversion (only when not inside an identifier) -- */
            if (c == '0' && !isalnum((unsigned char)prev) && prev != '_') {
                gnc(); /* consume '0' */
                char p = ch() | 0x20; /* fast lowercase for ascii letters */
                asm_emit_indent(&has_content, line_col, asmcol);
                if (p == 'x') {
                    gnc(); /* consume 'x'/'X' */
                    if (isxdigit((unsigned char)ch())) {
                        emit_ch('$'); /* valid hex literal */
                    } else {
                        emit_ch('0'); emit_ch(p); /* not valid - emit original */
                    }
                } else if (p == 'b') {
                    gnc(); /* consume 'b'/'B' */
                    char nc = ch();
                    if (nc == '0' || nc == '1') {
                        emit_ch('%'); /* valid binary literal */
                    } else {
                        emit_ch('0'); emit_ch(p); /* not valid - emit original */
                    }
                } else {
                    emit_ch('0');
                }
                prev = '0';
                continue;
            }

            /* -- ordinary character -- */
            asm_emit_indent(&has_content, line_col, asmcol);
            emit_ch(c);
            prev = c;
            gnc();
        }

        if (has_content) emit_nl();

        if (c == '\r' || c == '\n') {
            gnc();
            asm_advance_line(c);
        }
    }

    if (ch() == '}') gnc(); /* consume '}' */
    else error(errSyntax);

    /* Re-sync the scanner - call far_get_token directly since we are
     * already executing in BANK_42, no bank switch needed */
    far_get_token();
}

/* Banked implementation of get_token.
 * instead of the shared `lookup_keyword` wrapper.
 */
TOKEN_TYPE far_get_token(void) MYCC {
    char *temp;
get_token_start:
    temp = &token[0];
    *temp = '\0';
    
    skipws();
    token_length = 0;
    if (loc[fileid].line != token_line) token_line_start_col = loc[fileid].col;
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
                } else if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokAddAssign;
                }
                break;
            case '-':
                *temp++ = gnc();
                tok = tokMinus;
                if (ch() == '-') {
                    *temp++ = gnc();
                    tok = tokDec;
                } else if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokSubAssign;
                }
                break;
            case '/':
                *temp++ = gnc();
                tok = tokDiv; 
                if (ch() == '/') {
                    /* Single-line comment: skip to end of line */
                    while ((c = ch()) && c != '\r' && c != '\n') 
                        gnc();
                    goto get_token_start;
                } else if (ch() == '*') {
                    /* Block comment: use nested comment handler */
                    skip_block_comment();
                    goto get_token_start;
                } else if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokDivAssign;
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
                    if (ch() == '=') { *temp++ = gnc(); tok = tokShlAssign; }
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
                    if (ch() == '=') { *temp++ = gnc(); tok = tokShrAssign; }
                }
                break;
            case '|':
                *temp++ = gnc();
                tok = tokBitOr;
                if (ch() == '|') {
                    *temp++ = gnc();
                    tok = tokOr;
                } else if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokOrAssign;
                }
                break;
            case '&':
                *temp++ = gnc();
                tok = tokBitAnd;
                if (ch() == '&') {
                    *temp++ = gnc();
                    tok = tokAnd;
                } else if (ch() == '=') {
                    *temp++ = gnc();
                    tok = tokAndAssign;
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
            case '*': tok = tokStar; if (ch() == '=') { *temp++ = gnc(); *temp = '\0'; tok = tokMulAssign; } break;
            case '%': tok = tokMod; if (ch() == '=') { *temp++ = gnc(); *temp = '\0'; tok = tokModAssign; } break;
            case '~': tok = tokBitNot; break;
            case '^': tok = tokBitXor; if (ch() == '=') { *temp++ = gnc(); *temp = '\0'; tok = tokXorAssign; } break;
            case ';':                
                if (in_asm_block) {
                    /* Single-line comment: skip to end of line */
                    while ((c = ch()) && c != '\r' && c != '\n')
                        gnc();
                    goto get_token_start;
                }
                tok = tokSemi; break;
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
                if (ch() == '.') {
                    *temp++ = gnc(); /* consume second '.' */
                    if (ch() == '.') {
                        *temp++ = gnc(); /* consume third '.' */
                        tok = tokEllipsis;
                    } else {
                        error(errExpected_s, ".");
                    }
                } else {
                    tok = tokMember;
                }
                break;
            default:
                error(errInvalid_s, token);
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
        if (ch() != '\'') error(errExpected_c, '\'');
        gnc(); /* skip '\'' */
        tok = tokNumber;
        return (token_type = ttNumber);
    } else if (c == '"') {
        l = MAX_STR_LEN;
        gnc(); /* skip '"' */
        while ((c = ch()) && c != '"' && c != '\r' && c != '\n' && --l) {
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
        if (l == 0) error(errTooLong);

        /* Check for fractional part: only for base-10, followed by '.' then digit.
         * Consume '.' first, then use ch() to safely check the next char without
         * raw pointer arithmetic (which is unsafe at buffer chunk boundaries). */
        if (base == 10 && ch() == '.') {
            gnc(); /* consume '.' */
            if (!isdigit(ch())) {
                /* '.' not followed by digit - not a decimal point.
                 * Member access on a number literal is invalid in this language
                 * so the consumed '.' will surface as a downstream syntax error.
                 * Fall through to emit the integer token. */
                goto emit_integer;
            }
            /* Parse fractional digits and convert to 12.4 fixed-point:
             * integer part already in intval, shift it left 4.
             * Then parse up to 4 fractional decimal digits and compute
             * the fractional bits by rounding to nearest 1/16.
             */
            intval = intval << 4; /* integer part in Q4 */
            *temp++ = '.'; /* record the '.' we already consumed */

            /* Accumulate fractional decimal value as fixed * 10000 */
            uint16_t frac_num = 0;
            uint16_t frac_den = 1;
            while (isdigit(ch()) && frac_den <= 1000) {
                frac_num = frac_num * 10 + (ch() - '0');
                frac_den = frac_den * 10;
                *temp++ = gnc();
            }
            /* Skip any remaining fractional digits (beyond precision) */
            while (isdigit(ch())) gnc();

            /* Convert decimal fraction to Q4: frac_bits = round(frac_num * 16 / frac_den) */
            uint16_t frac_bits = (uint16_t)(((uint32_t)frac_num * 16 + frac_den / 2) / frac_den);
            if (frac_bits > 15) frac_bits = 15;
            intval = intval + (int16_t)frac_bits;

            *temp = '\0';
            tok = tokFixedLit;
            token_length = (uint8_t)(temp - &token[0]);
            return (token_type = ttNumber);
        }

        emit_integer:
        *temp = '\0';
        tok = tokNumber;
        token_length = (uint8_t)(temp - &token[0]);
        return (token_type = ttNumber);
    }

    if (isalpha(c) || c == '_' || (token_col == 1 && c == '#')) {
        if (c == '#') {
            *temp++ = gnc(); /* skip '#' */
        }
        while ((c = ch()) && (isalnum(c) || c == '_') && --l) {
            *temp++ = gnc();
        }
        *temp = '\0';
        if (l == 0) error(errTooLong);

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
