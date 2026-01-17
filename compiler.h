#ifndef COMPILER_H_
#define COMPILER_H_

extern uint8_t func_argcount;
extern uint8_t func_is_variadic;
extern uint16_t locals_lbl; 
extern uint16_t exit_lbl;

extern uint16_t bp_lastlocal;
extern uint16_t localbytes;

void compile(const char *filename, const char *asmfilename) MYCC;

void parse_funccall(SYMBOL* sym, uint8_t ptr_in_hl) MYCC;
uint8_t try_handle_variadic_intrinsic(const char* name) MYCC;

#endif //COMPILER_H_
