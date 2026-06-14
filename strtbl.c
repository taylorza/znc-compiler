#include "znc.h"

char strtbl[MAX_STRTBL_SIZE];

size_t laststr = 0;
size_t str_search_base = 0; // lookupstr searches from here (0 for global, laststr for bank)

size_t far_get_laststr(void) MYCC {
    return laststr;
}

void far_reset_laststr(size_t to) MYCC {
    laststr = to;
}

void far_set_str_search_base(size_t base) MYCC {
    str_search_base = base;
}

int16_t far_lookupstr(const char* s, uint8_t len) MYCC {
    for (size_t i = str_search_base; i < laststr;) {
        const char* literal = &strtbl[i];
        if (strtbl[i + len] == '\0' && memcmp(s, literal, len) == 0) {
            return (uint16_t)i;
        }
        i += strlen(literal)+1;
    }
    if (laststr + len >= MAX_STRTBL_SIZE) error(errTooManySymbols);

    memcpy(&strtbl[laststr], s, len);
    strtbl[laststr + len] = '\0';
    laststr += len+1;
    return (uint16_t)(laststr - len - 1);
}

static void emit_strings_range(size_t from, size_t to) MYCC {
    for (size_t i = from; i < to;) {
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

void far_dump_strings_range(const char* label, size_t from, size_t to) MYCC {
    if (from == 0 && to == 0) {
        from = 0;
        to = laststr;
    }
    if (from >= to) return;
    
    emit_str(label); emit_nl();
    emit_strings_range(from, to);
}
