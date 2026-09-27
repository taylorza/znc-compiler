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

int far_add_struct(const char* name, uint8_t is_union) MYCC {
    if (struct_count >= MAX_STRUCTS) {
        error(errTooManySymbols);
        return -1;
    }
    if (far_find_struct(name) != -1) return -1; /* already exists */
    STRUCTDEF* s = &struct_tab[struct_count];
    strncpy(s->name, name, MAX_IDENT_LEN);
    s->first_field = 0xFFFF;
    s->last_field = 0xFFFF;
    s->fieldcount = 0;
    s->is_union = is_union;
    s->size = 0;
    return struct_count++;
}

void add_struct_field_with_offset(int id, const char* name, uint8_t type_id, uint16_t offset) MYCC {
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
    f->next = 0xFFFF;
    f->anonymous_id = 0xFF;

    if (s->first_field == 0xFFFF) s->first_field = idx;
    else field_pool[s->last_field].next = idx;
    s->last_field = idx;
    s->fieldcount++;
}

void far_add_struct_field(int id, const char* name, uint8_t type_id) MYCC {
    /* Union members share offset zero; a union's size is its largest member. */
    uint16_t cur = far_get_struct_size(id);
    uint16_t inc = type_size(type_id);
    uint16_t newsize = cur + inc;

    if (id >= 0 && id < struct_count && struct_tab[id].is_union) {
        cur = 0;
        newsize = inc > far_get_struct_size(id) ? inc : far_get_struct_size(id);
    }

    add_struct_field_with_offset(id, name, type_id, cur);
    far_set_struct_size(id, newsize);    
}

void far_add_struct_anonymous_field(int parent_id, int child_id, uint8_t field_id,
                                    uint16_t base) MYCC {
    uint16_t child_index = struct_tab[child_id].first_field;
    for (uint8_t i = 0; i < field_id && child_index != 0xFFFF; ++i)
        child_index = field_pool[child_index].next;
    if (child_index == 0xFFFF) return;
    FIELDDEF* child_field = &field_pool[child_index];
    add_struct_field_with_offset(parent_id, child_field->name,
                                 child_field->type_id, base + child_field->offset);
    field_pool[struct_tab[parent_id].last_field].anonymous_id = (uint8_t)child_id;
}

static uint16_t get_field_index(int id, int field_id) {
    if (id < 0 || id >= struct_count || field_id < 0 ||
        field_id >= struct_tab[id].fieldcount)
        return 0xFFFF;

    uint16_t index = struct_tab[id].first_field;
    for (int i = 0; i < field_id && index != 0xFFFF; ++i)
        index = field_pool[index].next;
    return index;
}

int far_find_struct_field(int id, const char* name) MYCC {
    if (id < 0 || id >= struct_count) return -1;
    STRUCTDEF* s = &struct_tab[id];
    if (s->first_field == 0xFFFF) return -1;
    uint16_t idx = s->first_field;
    for (int i = 0; i < s->fieldcount; ++i) {
        if (idx == 0xFFFF) return -1;
        if (strncmp(field_pool[idx].name, name, MAX_IDENT_LEN) == 0) return i;
        idx = field_pool[idx].next;
    }
    return -1;
}

uint16_t far_get_struct_size(int id) MYCC {
    if (id < 0 || id >= struct_count) return 0;
    return struct_tab[id].size;
}

uint8_t far_is_struct_union(int id) MYCC {
    if (id < 0 || id >= struct_count) return 0;
    return struct_tab[id].is_union;
}

int far_get_anonymous_field_struct_id(int id, int fid) MYCC {
    uint16_t idx = get_field_index(id, fid);
    if (idx == 0xFFFF || field_pool[idx].anonymous_id == 0xFF) return -1;
    return field_pool[idx].anonymous_id;
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
    uint16_t idx = get_field_index(id, fid);
    if (idx == 0xFFFF) return fi;
    fi.type_id = field_pool[idx].type_id;
    fi.offset = field_pool[idx].offset;
    return fi;
}