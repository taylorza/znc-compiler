 #ifndef STRUCT_H_
#define STRUCT_H_

#include "znc.h"

#define MAX_STRUCTS 64
#define MAX_FIELDS_PER_STRUCT 32
#define MAX_FIELDS 512 /* pooled field storage to reduce per-struct memory */

typedef struct FIELDDEF {
    char name[MAX_IDENT_LEN+1];
    uint8_t type_id;
    uint16_t offset;
} FIELDDEF;

typedef struct STRUCTDEF {
    char name[MAX_IDENT_LEN+1];
    uint16_t first_field; /* index into global FIELDDEF pool, 0xFFFF = none */
    uint8_t fieldcount;
    uint16_t size;
} STRUCTDEF;

typedef struct FIELDINFO { uint8_t type_id; uint16_t offset; } FIELDINFO;

/* Main-bank stubs (wrappers) - implemented in main bank and call into BANK_44 */
int find_struct(const char* name) MYCC; // returns index or -1
int add_struct(const char* name) MYCC; // returns index (>=0) or -1
void add_struct_field(int id, const char* name, uint8_t type_id) MYCC;
int find_struct_field(int id, const char* name) MYCC; // returns field index or -1
uint16_t get_struct_size(int id) MYCC;
int get_field_count(int id) MYCC;
FIELDINFO get_struct_field(int id, int fid) MYCC; // returns type+offset by value

/* Far (bank 44) implementations - defined in BANK 44 */
int far_find_struct(const char* name) MYCC;
int far_add_struct(const char* name) MYCC;
/* Far API implemented in BANK 44. Keep these functions minimal and avoid heavy runtime calls. */
void far_add_struct_field_with_offset(int id, const char* name, uint8_t type_id, uint16_t offset) MYCC;
int far_find_struct_field(int id, const char* name) MYCC;
uint16_t far_get_struct_size(int id) MYCC;
void far_set_struct_size(int id, uint16_t size) MYCC;
int far_get_field_count(int id) MYCC;
FIELDINFO far_get_struct_field(int id, int fid) MYCC;
#endif //STRUCT_H_