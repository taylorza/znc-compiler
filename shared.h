#ifndef SHARED_H_
#define SHARED_H_

#include "znc.h"

typedef uint16_t ARENA_MARKER;

ARENA_MARKER arena_get_marker(void) MYCC;
void* arena_alloc(size_t len) MYCC;
void arena_free_to_marker(ARENA_MARKER m) MYCC;
char* arena_strdup(const char* s, size_t len) MYCC;
char* arena_strappend(char* prev, size_t prev_len, const char* add, size_t add_len) MYCC;

#endif // SHARED_H_
