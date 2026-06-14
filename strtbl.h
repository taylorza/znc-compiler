#ifndef STRTBL_H_
#define STRTBL_H_

#include <stddef.h>

int16_t lookupstr(const char* s, uint8_t length) MYCC;

size_t get_laststr(void) MYCC;
void reset_laststr(size_t to) MYCC;
void set_str_search_base(size_t base) MYCC;
void dump_strings_range(const char* label, size_t from, size_t to) MYCC;

#endif //STRTBL_H_
