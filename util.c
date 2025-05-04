#include "znc.h"

const char* find_char_in_str(const char *str, char val) MYCC {
    if (val == '\0') return 0;
    while (*str) {
        if (*str == val) return str;
        ++str;
    }
    return 0;
}

void set_file_ext(char *filename, const char *ext) MYCC {
    char *dot = strrchr(filename, '.');
    if (dot) *dot='\0';
    if (strlen(ext) > 0) {
        if (*ext != '.') strcat(filename, ".");
        strcat(filename, ext);
    }
}