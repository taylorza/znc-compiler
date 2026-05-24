#include "znc.h"
#include "shared.h"

#define SHARED_ARENA_SIZE 2781 // leave 1K in the code_l segment

static char arena_buf[SHARED_ARENA_SIZE];
static ARENA_MARKER arena_offset = 0;

ARENA_MARKER arena_get_marker(void) MYCC {
    return arena_offset;
}

void* arena_alloc(size_t len) MYCC {
    if (len == 0) return NULL;
    /* ensure room for NUL if used as string */
    size_t end = (size_t)arena_offset + len;
    if (end > SHARED_ARENA_SIZE) {
        error(errArenaOutOfMemory);
        return NULL;
    }
    void* p = &arena_buf[arena_offset];
    arena_offset = (ARENA_MARKER)end;
    return p;
}

void arena_free_to_marker(ARENA_MARKER m) MYCC {
    if (m <= arena_offset) {
        arena_offset = m;
    }
}

char* arena_strdup(const char* s, size_t len) MYCC {
    char* p = arena_alloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

char* arena_strappend(char* prev, size_t prev_len, const char* add, size_t add_len) MYCC {
    if (!prev) return arena_strdup(add, add_len);

    ptrdiff_t off = prev - arena_buf;
    if (off >= 0 && (size_t)off + prev_len == arena_offset) {
        /* last allocation, can extend in place */
        size_t end = (size_t)arena_offset + add_len;
        if (end + 1 > SHARED_ARENA_SIZE) {
            error(errArenaOutOfMemory);
            return NULL;
        }
        memcpy(&arena_buf[arena_offset], add, add_len);
        arena_offset = (ARENA_MARKER)end;
        arena_buf[arena_offset] = '\0';
        return prev;
    } else {
        /* need to allocate new space and copy */
        size_t newlen = prev_len + add_len;
        char* n = arena_alloc(newlen + 1);
        if (!n) return NULL;
        memcpy(n, prev, prev_len);
        memcpy(n + prev_len, add, add_len);
        n[newlen] = '\0';
        return n;
    }
}
