#ifndef COMPILER_H_
#define COMPILER_H_

extern EXPR_RESULT expr_result;   // global for non-recursive expression result

extern uint8_t func_argcount;
extern uint8_t func_is_variadic;
extern uint16_t locals_lbl; 
extern uint16_t exit_lbl;

extern uint16_t bp_lastlocal;
extern uint16_t localbytes;
extern uint16_t current_org;

void compile(const char *filename, char *asmfilename) MYCC;

void parse(const char* sourcefile, char* outfilename, uint8_t entrypoint) MYCC;
void parse_funccall(SYMBOL* sym, PTR_LOCATION ptr_loc) MYCC;
uint8_t try_handle_variadic_intrinsic(const char* name) MYCC;

EXPR_RESULT parse_onearg(void) MYCC;

#endif //COMPILER_H_
