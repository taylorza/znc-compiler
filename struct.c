#include "znc.h"
#include "struct.h"

static STRUCTDEF struct_tab[MAX_STRUCTS];
static FIELDDEF field_pool[MAX_FIELDS];
static uint16_t field_next = 0;
static int struct_count = 0;

int far_find_struct(const char* name) MYCC {
    for (int i = 0; i < struct_count; ++i) {
        if (strncmp(struct_tab[i].name, name, MAX_IDENT_LEN) == 0) return i;
    }
    return -1;
}

int far_add_struct(const char* name) MYCC {
    if (struct_count >= MAX_STRUCTS) {
        error(errTooManySymbols);
        return -1;
    }
    if (far_find_struct(name) != -1) return -1; /* already exists */
    STRUCTDEF* s = &struct_tab[struct_count];
    strncpy(s->name, name, MAX_IDENT_LEN);
    s->first_field = 0xFFFF;
    s->fieldcount = 0;
    s->size = 0;
    return struct_count++;
}

void far_add_struct_field_with_offset(int id, const char* name, uint8_t type_id, uint16_t offset) MYCC {
    if (id < 0 || id >= struct_count) return;
    if (field_next >= MAX_FIELDS) {
        error(errTooManySymbols);
        return;
    }

    STRUCTDEF* s = &struct_tab[id];

    /* allocate a field in the global pool */
    uint16_t idx = field_next++;
    FIELDDEF* f = &field_pool[idx];
    strncpy(f->name, name, MAX_IDENT_LEN);
    f->type_id = type_id;
    f->offset = offset;

    if (s->first_field == 0xFFFF) s->first_field = idx;
    s->fieldcount++;
}

int far_find_struct_field(int id, const char* name) MYCC {
    if (id < 0 || id >= struct_count) return -1;
    STRUCTDEF* s = &struct_tab[id];
    if (s->first_field == 0xFFFF) return -1;
    uint16_t idx = s->first_field;
    for (int i = 0; i < s->fieldcount; ++i) {
        if (strncmp(field_pool[idx + i].name, name, MAX_IDENT_LEN) == 0) return i;
    }
    return -1;
}

uint16_t far_get_struct_size(int id) MYCC {
    if (id < 0 || id >= struct_count) return 0;
    return struct_tab[id].size;
}

void far_set_struct_size(int id, uint16_t size) MYCC {
    if (id < 0 || id >= struct_count) return;
    struct_tab[id].size = size;
}

int far_get_field_count(int id) MYCC {
    if (id < 0 || id >= struct_count) return 0;
    return struct_tab[id].fieldcount;
}

FIELDINFO far_get_struct_field(int id, int fid) MYCC {
    FIELDINFO fi;
    fi.offset = 0;
    fi.type_id = TYPE_ID_VOID;  /* return void type by default */

    if (id < 0 || id >= struct_count) return fi;
    STRUCTDEF* s = &struct_tab[id];
    if (s->first_field == 0xFFFF) return fi;
    if (fid < 0 || fid >= s->fieldcount) return fi;
    uint16_t idx = s->first_field + fid;
    fi.type_id = field_pool[idx].type_id;
    fi.offset = field_pool[idx].offset;
    return fi;
}