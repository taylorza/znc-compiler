#ifndef COMPILER_H_
#define COMPILER_H_

extern char* rtlfilename;  // filename for RTL include

extern EXPR_RESULT expr_result;   // global for non-recursive expression result

extern uint8_t func_arg_count;
extern uint8_t func_is_variadic;
extern uint16_t locals_lbl; 
extern uint16_t exit_lbl;

extern uint8_t bankseen;
extern uint8_t currbank;
extern uint16_t top_local_lbl;
extern uint8_t hash_if_depth;

extern uint16_t bp_lastlocal;
extern uint16_t localbytes;
extern uint16_t current_org;

void compile(const char *filename, char *asmfilename) MYCC;

void parse(const char* sourcefile, char* outfilename, uint8_t entrypoint) MYCC;
void parse_funccall(SYMBOL* sym, PTR_LOCATION ptr_loc, uint8_t callee_type_id) MYCC;
void parse_statement_block(uint16_t brklbl, uint16_t contlbl) MYCC;
uint8_t try_handle_variadic_intrinsic(const char* name) MYCC;

EXPR_RESULT parse_onearg(void) MYCC;

#endif //COMPILER_H_
