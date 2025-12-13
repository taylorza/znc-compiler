#ifndef FARCALL_H__
#define FARCALL_H__

// strtbl - BANK 40
int16_t lookupstr(const char* s) MYCC;
void dump_strings(void) MYCC;

// sym - BANK 41
SYMBOL findglb(const char* name) MYCC;
SYMBOL findloc(const char* name) MYCC;
SYMBOL lookupIdent(const char* name) MYCC;
void updatesym(SYMBOL* from) MYCC;

SYMBOL addglb(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC;
SYMBOL addloc(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC;

uint16_t push_frame(void) MYCC;
void pop_frame(uint16_t frame) MYCC;
uint8_t is_scoped(void) MYCC;

void dump_globals(void) MYCC;

#endif //FARCALL_H__