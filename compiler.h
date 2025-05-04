#ifndef COMPILER_H_
#define COMPILER_H_

extern uint8_t func_argcount;
extern uint16_t locals_lbl; 
extern uint16_t exit_lbl;

extern uint16_t bp_lastlocal;
extern uint16_t localbytes;

void compile(const char *filename, const char *asmfilename) MYCC;

void parse_funccall(SYMBOL* sym) MYCC;

#endif //COMPILER_H_