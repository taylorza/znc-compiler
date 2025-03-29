#include "util.h"

const char* find_char_in_str(const char *str, char val) {
    if (val == '\0') return 0;
    while (*str) {
        if (*str == val) return str;
        ++str;
    }
    return 0;
}