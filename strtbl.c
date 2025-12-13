#include "znc.h"

#ifdef __ZXNEXT
extern char strtbl[];
#else
char strtbl[MAX_STRTBL_SIZE];
#endif

size_t laststr = 0;

int16_t far_lookupstr(const char* s) MYCC {
    for (size_t i = 0; i < laststr;) {
        const char* literal = &strtbl[i];
        if (strcmp(s, literal) == 0) {
            return (uint16_t)i;
        }
        i += strlen(literal)+1;
    }
    size_t len = strlen(s);
    if (laststr + len >= MAX_STRTBL_SIZE) error(errTooManySymbols);

    strcpy(strtbl + laststr, s);
    laststr += len+1;
    return (uint16_t)(laststr - len - 1);
}


void far_dump_strings(void) MYCC {
    if (!laststr) return;
    emit_str("str"); emit_nl();
    for (size_t i = 0; i < laststr;) {
        const char* literal = &strtbl[i];
        emit_str(" db ", i);
        uint8_t find_char_in_str = 0;
        size_t j;
        for (j = 0; *literal; ++j, ++i) {
            uint8_t ch = *literal++;
            if (ch == '"') emit_ch('"');
            if (!find_char_in_str && j > 0) emit_ch(',');
            if (ch < 32) {
                if (find_char_in_str) 
                    emit_str("\",%d", ch);
                else 
                    emit_str("%d", ch);                
                find_char_in_str = 0;
            } else {
                if (!find_char_in_str) emit_ch('"');
                emit_ch(ch);
                find_char_in_str = 1;
            }            
        }
        ++i;
        if (find_char_in_str) emit_ch('\"');
        if (j) emit_ch(',');
        emit_ch('0');
        emit_ch(NL);
    }
}