#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "initializer.h"
#include "callgraph.h"

char* rtlfilename;          // filename for RTL include
uint16_t retlbl = 0;        // function exit label

TOKEN tokMakeType = tokRaw; // type of make command

uint16_t currfunc_id = 0;   // id of the current function being parsed (0 if not in a function)
uint8_t infunc = 0;         // 1 if parsing a function
uint8_t func_rettype = 0;   // return type of current function (0 = void)
uint8_t bankseen = 0;       // set to 1 the first time a bank directive is encountered
uint8_t currbank = 0;       // 0 if no BANK, otherwise id of current bank (1..255)
uint8_t page_count = 0;     // current page order for banked code generation

uint16_t locals_lbl;        // label of the EQU with the arg count
uint8_t localcount;         // number of live local variables
uint8_t maxlocalcount;      // highwater mark for local variables

uint16_t bp_lastlocal;      // base pointer of the last local
uint16_t localbytes;        // total bytes for locals
uint16_t exit_lbl;          // label for the global exit (return to BASIC for DOT commands)
uint16_t start_lbl;         // label for the start of the code
uint16_t stack_lbl;         // label for nex stack
uint16_t stack_base;        // base address of the stack for NEX files

uint16_t top_local_lbl = 0; // label for the top of the local variable space (for calculating offsets)
uint16_t current_org;       // current org address for output

uint8_t hash_if_depth = 0; // depth of #if/#ifdef statements

EXPR_RESULT expr_result;   // global for non-recursive expression result

uint8_t func_arg_count;                 // number of arguments for function being parsed
uint8_t func_arg_types[MAX_FUNC_ARGS];  // Collect argument types (function/delegate declarations)
uint8_t func_is_variadic;               // 1 if current function is variadic

/* Feature flags controlled by command-line options (default clear) */
uint8_t dfe_enabled = 0; /* emit markers to help assembler perform dead-function elimination */

char decl_name[MAX_IDENT_LEN + 1];       // Current declaration name

SYMBOL declglb(uint8_t type_id, SYM_CLASS_SCOPE klass, const char* name, int16_t value);
SYMBOL declloc(uint8_t type_id, SYM_CLASS_SCOPE klass, const char* name, int16_t offset);
SYMBOL decl_in_scope(uint8_t type_id, SYM_CLASS_SCOPE klass, const char* name);

void parse_type(uint8_t* type_id_out) MYCC;
void parse_funcdecl(uint8_t rettype_id, const char* name) MYCC;

void parse_include(void) MYCC;
void parse_decl(void) MYCC;
void parse_statement(uint16_t brklbl, uint16_t contlbl) MYCC;
void parse_if(uint16_t brklbl, uint16_t contlbl) MYCC;
void parse_while(void) MYCC;
void parse_for(void) MYCC;
void parse_break(uint16_t brklbl) MYCC;
void parse_continue(uint16_t contlbl) MYCC;
void parse_return(void) MYCC;
void parse_exit(void) MYCC;
void parse_putc(void) MYCC;
void parse_out(void) MYCC;
void parse_nextreg(void) MYCC;
void parse_asm(void) MYCC;
void parse_org(void) MYCC;
void parse_bank(void) MYCC;
void parse_hashif(uint16_t brklbl, uint16_t contlbl) MYCC;
void parse_switch(uint16_t contlbl) MYCC;
void parse_vastart(void) MYCC;
void parse_vaend(void) MYCC;
void parse_enum_def(void) MYCC;

/* Forward declaration for struct parser (defined later) */
void parse_struct_def(void) MYCC;
void parse_delegate_decl(void) MYCC;
void parse_make(const char* outfilename) MYCC;

/* ------------------------------------------------------------------ *
 * Far declarations for compilerex.c (BANK_47) implementations        *
 * ------------------------------------------------------------------ */
void far_parse_include(void) MYCC;
void far_parse_type(uint8_t* type_id_out) MYCC;
void far_parse_funcdecl(uint8_t rettype_id, const char* name) MYCC;
void far_parse_delegate_decl(void) MYCC;
void far_parse_if(uint16_t brklbl, uint16_t contlbl) MYCC;
void far_parse_for(void) MYCC;
void far_parse_switch(uint16_t contlbl) MYCC;
void far_parse_funccall(SYMBOL* sym, PTR_LOCATION ptr_loc, uint8_t callee_type_id) MYCC;
void far_parse_while(void) MYCC;
void far_parse_break(uint16_t brklbl) MYCC;
void far_parse_continue(uint16_t contlbl) MYCC;
void far_parse_putc(void) MYCC;
void far_parse_out(void) MYCC;
void far_parse_nextreg(void) MYCC;
void far_parse_return(void) MYCC;
void far_parse_exit(void) MYCC;
void far_parse_vastart(void) MYCC;
void far_parse_vaend(void) MYCC;
void far_parse_org(void) MYCC;
void far_parse_bank(void) MYCC;
void far_parse_enum(void) MYCC;
void far_parse_struct_def(void) MYCC;
void far_parse_enum_member(EXPR_RESULT *result, const char* enum_name) MYCC;
void far_parse_hashif(uint16_t brklbl, uint16_t contlbl) MYCC;

/* Called from BANK_47 parser code to skip syntax without emitting code. */
void skip_statement(void) MYCC;

void parse(const char* sourcefile, char* outfilename, uint8_t entrypoint) MYCC {
    if (!src_open(sourcefile)) {
        printf("can't open '%s'", sourcefile);
        exit(1);
    }
    printf("compiling %s\n", sourcefile);
    get_token();

    // initialization statements
    if (tok == tokIdent) {
        TOKEN t = lookup_ident_token(token);
        if (t == tokMake) parse_make(outfilename);
        else if (t == tokOrg) parse_org();
    }

    if (entrypoint) { 
        emit_instrln("options case_s");       
        if (tokMakeType == tokNex) {
            start_lbl = newlbl();
            stack_lbl = newlbl();
            emit_lbl(start_lbl);
            emit_instr("ld sp,");
            emit_n(stack_base); emit_nl();
        }
        localbytes = 0;       
        exit_lbl = newlbl();
        emit_frame_prologue(1);
        top_local_lbl = emit_alloclocals();        
    }

    while (tok != tokEOS) {
        parse_statement(NO_LABEL, NO_LABEL);
    }

    if (entrypoint && !bankseen) {
        emit_instrln("xor a"); // clear A register and carry flag
        emit_frame_epilogue(1, exit_lbl, 0, 0, tokMakeType);
        emit_lblequ16(top_local_lbl, localbytes);
    }
    src_close();
}

int get_type_id(void) MYCC {
    int found = -1;
    uint8_t id;
    if (tok == tokIdent) {
        found = find_struct(token);
        if (found < 0)
            found = type_find_by_name(token);
    }
    else if (tok == tokVoid || tok == tokChar || tok == tokByte || tok == tokUint || tok == tokInt || tok == tokFixed) {
        found = tok;
    }
    if (found < 0) return -1;

    parse_type(&id);
    return id;
}

void emit_make_defines(TOKEN outputTok) {
    uint8_t const_char_type = type_make_char(1);

    switch (outputTok) {
        case tokNex: declglb(const_char_type, VARIABLE, "__NEX", 1);  break;
        case tokDot: declglb(const_char_type, VARIABLE, "__DOT", 1);  break;
        case tokRaw: declglb(const_char_type, VARIABLE, "__RAW", 1);  break;
    }

    emit_strln("__NEX equ %d", outputTok == tokNex ? 1 : 0);
    emit_strln("__DOT equ %d", outputTok == tokDot ? 1 : 0);
    emit_strln("__RAW equ %d", outputTok == tokRaw ? 1 : 0);
}

void parse_make(const char *filename) MYCC {
    get_token(); // skip 'make'

    /* Output type (nex/dot/raw) is always an identifier — not a keyword. */
    if (tok != tokIdent) { error(errExpected_s, "nex/dot/raw"); return; }
    TOKEN t = lookup_ident_token(token);
    if (t != tokNex && t != tokRaw && t != tokDot) { error(errExpected_s, "nex/dot/raw"); return; }
    tokMakeType = t;

    get_token(); // skip output type

    if (tok == tokString) {
        snprintf(outfilename, MAX_FILENAME_LEN, "%s", token);
        get_token(); // skip string
    } else {
        snprintf(outfilename, MAX_FILENAME_LEN, "%s%s", filename, tokMakeType == tokNex ? ".nex" : "");
    }
    if (tokMakeType == tokNex) {
        if (tok == tokComma) {
            get_token(); // skip ','
            expr_result = parse_expr_delayconst(0, TYPE_ID_INT);
            if (!type_is_const(expr_result.type_id)) error(errConstExpected);
            stack_base = expr_result.value;
        } else {
            stack_base = 0xBFFF;
        }
    }
    
    expect_semi();

    if (tokMakeType == tokRaw || tokMakeType == tokDot)
        emit_output(outfilename, tokMakeType);

    if (tok == tokIdent && lookup_ident_token(token) == tokBank) parse_bank();
    else if (tokMakeType == tokNex) emit_bank(0, 0);

    if (tok == tokIdent && lookup_ident_token(token) == tokOrg) parse_org();
    else if (tokMakeType == tokNex) {
        emit_org(0xc000);        
    }

    emit_make_defines(tokMakeType);
}

EXPR_RESULT parse_onearg(void) MYCC {
    EXPR_RESULT expr;
    get_token(); // skip leading token
    expect_LParen();
    expr = parse_expr(0, 0);
    expect_RParen();
    return expr;
}

/* Consume one statement from the token stream without emitting any code.
   Handles a balanced { } block, compound control-flow statements, or a
   single ;-terminated statement.  Recursive for nested if/else/while/for. */
/* Called directly from compilerex.c (BANK_47) to skip a statement. */
void skip_statement(void) MYCC {
    if (tok == tokLBrace) {
        int depth = 1;
        get_token(); // skip '{'
        while (tok != tokEOS && depth > 0) {
            if (tok == tokLBrace) ++depth;
            else if (tok == tokRBrace) { if (--depth == 0) break; }
            get_token();
        }
        get_token(); // skip final '}'
    } else if (tok == tokIf || tok == tokWhile || tok == tokFor || tok == tokSwitch) {
        get_token(); // skip keyword
        // skip the '(' ... ')' header
        int depth = 1;
        get_token(); // skip '('
        while (tok != tokEOS && depth > 0) {
            if (tok == tokLParen) ++depth;
            else if (tok == tokRParen) { if (--depth == 0) break; }
            get_token();
        }
        get_token(); // skip final ')'
        skip_statement(); // skip body
        if (tok == tokElse) { get_token(); skip_statement(); } // skip else branch
    } else {
        // skip single statement up to and including ';'
        // must handle nested parens/brackets to not misread a ';' inside for(;;)
        int depth = 0;
        while (tok != tokEOS) {
            if (tok == tokLParen || tok == tokLBrack) ++depth;
            else if (tok == tokRParen || tok == tokRBrack) --depth;
            else if (tok == tokSemi && depth == 0) { get_token(); break; }
            get_token();
        }
    }
}

void parse_statement_block(uint16_t brklbl, uint16_t contlbl, uint8_t check_lbrace) MYCC {
    if (check_lbrace) expect(tokLBrace, '{');

    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;
    uint16_t old_bp = bp_lastlocal;
    while (tok != tokEOS && tok != tokRBrace) {
        uint8_t was_exit = (tok == tokReturn || tok == tokBreak || tok == tokContinue);
        parse_statement(brklbl, contlbl);
        if (was_exit) {
            while (tok == tokSemi) get_token(); // skip any extra semicolons
            if (tok != tokEOS && tok != tokRBrace) warn(errUnreachableCode);
            while (tok != tokEOS && tok != tokRBrace)
                skip_statement();
        }
    }
    if (maxlocalcount < localcount) maxlocalcount = localcount;
    bp_lastlocal = old_bp;
    localcount = old_localcount;
    expect(tokRBrace, '}');
    pop_frame(blockframe);
}

void parse_statement(uint16_t brklbl, uint16_t contlbl) MYCC {
    /* If the current token is an identifier that is a known struct or enum type, treat this
     * as a declaration (e.g. `Point p;`). This must be checked before falling back
     * to expression parsing so type names can be used like in C++.
     */
    if (tok == tokIdent) {
        int sid = find_struct(token);
        if (sid >= 0) {
            parse_decl();
            return;
        }
        /* Also treat named types (e.g. delegate type names) as declarations */
        int tid = type_find_by_name(token);
        if (tid != -1) {
            parse_decl();
            return;
        }

        /* org and bank are not keywords; recognise them here as directives. */
        TOKEN special = lookup_ident_token(token);
        if (special == tokOrg) { parse_org(); return; }
        if (special == tokBank) { parse_bank(); return; }
    }

    switch (tok) {
        case tokConst:
        case tokVoid:
        case tokChar:
        case tokByte:
        case tokUint:
        case tokInt:
        case tokFixed:
            parse_decl();
            break;
        case tokDelegate:
            /* delegate declarations are top-level only (<top_decl> in the grammar) */
            if (infunc) error(errTopLevelOnly);
            parse_delegate_decl();
            break;
        case tokStruct:
            /* struct definitions are top-level only (<top_decl> in the grammar) */
            if (infunc) error(errTopLevelOnly);
            parse_struct_def();
            break;
        case tokUnion:
            /* union definitions are top-level only (<top_decl> in the grammar) */
            if (infunc) error(errTopLevelOnly);
            parse_struct_def();
            break;
        case tokEnum:
            if (infunc) error(errTopLevelOnly);
            parse_enum_def();
            break;

        case tokLBrace:
            parse_statement_block(brklbl, contlbl, 1);
            break;
        case tokIf: parse_if(brklbl, contlbl); break;
        case tokSwitch: parse_switch(contlbl); break;
        case tokWhile: parse_while(); break;
        case tokFor: parse_for(); break;
        case tokBreak: parse_break(brklbl); break;
        case tokContinue: parse_continue(contlbl); break;
        case tokReturn: parse_return(); break;
        case tokExit: parse_exit(); break;
        case tokPutc: parse_putc(); break;
        case tokVaStart: parse_vastart(); break;
        case tokVaEnd: parse_vaend(); break;
        case tokOut: parse_out(); break;
        case tokNextReg: parse_nextreg(); break;
        case tokAsm: parse_asm(); break;
        case tokInclude: parse_include(); break;
        case tokSemi: get_token(); break; // empty statement

        case tokHashIf:
        case tokHashIfDef:
        case tokHashIfNDef:
            parse_hashif(brklbl, contlbl);
            break;

        case tokHashElse:
            if (hash_if_depth == 0) error(errUnexpectedElse);
            break;
        case tokHashEndif:
            if (hash_if_depth == 0) error(errUnexpectedEndif);
            break;
        default:
            parse_expr(0, 0);
            expect_semi();
            break;
    }
}

void parse_include(void) MYCC {
    PROLOG(47)
    far_parse_include();
    EPILOG
}

static uint8_t make_const_type(uint8_t type_id) MYCC {
    if (type_is_void(type_id)) error(errTypeError);
    if (type_is_pointer(type_id)) error(errTypeError);
    if (type_is_array(type_id)) error(errTypeError);
    if (type_is_scalar(type_id)) return type_as_const(type_id);
    error(errTypeError);
    return type_id;
}

void parse_decl(void) MYCC {
    uint8_t type_id;
    uint8_t constdecl = 0;
    if (tok == tokConst) {
        constdecl = 1;
        get_token(); // skip 'const'
    }

    parse_type(&type_id);
    if (constdecl) {
        type_id = make_const_type(type_id);
    }

    if (tok != tokIdent) error(errExpected_s, "identifier");
    snprintf(decl_name, sizeof(decl_name), "%s", token);

    get_token(); // skip name

    if (tok == tokLParen) {
        if (infunc || constdecl) error(errTopLevelOnly);
        parse_funcdecl(type_id, decl_name);
    }
    else {
        SYMBOL sym;

        if (type_is_void(type_id) && !type_is_pointer(type_id)) error(errTypeError);

        for (;;) {
            sym = decl_in_scope(type_id, VARIABLE, decl_name);
               
            if (tok == tokAssign) {
                parse_assign(0, &sym, 0, type_id);
            }
            else if (constdecl) error(errExpected_s, "initializer");

            if (tok != tokComma) break;
            get_token(); // skip ','
            snprintf(decl_name, sizeof(decl_name), "%s", token);
            get_token(); // skip name
        }
        expect_semi();
    }
}

void parse_funcdecl(uint8_t rettype_id, const char* name) MYCC {
    PROLOG(47)
    far_parse_funcdecl(rettype_id, name);
    EPILOG
}

void parse_struct_def(void) MYCC {
    PROLOG(47)
    far_parse_struct_def();
    EPILOG
}

void parse_enum_def(void) MYCC {
    PROLOG(47)
    far_parse_enum();
    EPILOG
}

void parse_if(uint16_t brklbl, uint16_t contlbl) MYCC {
    PROLOG(47)
    far_parse_if(brklbl, contlbl);
    EPILOG
}

void parse_switch(uint16_t contlbl) MYCC {
    PROLOG(47)
    far_parse_switch(contlbl);
    EPILOG
}

void parse_while(void) MYCC {
    PROLOG(47)
    far_parse_while();
    EPILOG
}

void parse_for(void) MYCC {
    PROLOG(47)
    far_parse_for();
    EPILOG
}

void parse_break(uint16_t brklbl) MYCC {
    PROLOG(47)
    far_parse_break(brklbl);
    EPILOG
}

void parse_continue(uint16_t contlbl) MYCC {
    PROLOG(47)
    far_parse_continue(contlbl);
    EPILOG
}

void parse_putc(void) MYCC {
    PROLOG(47)
    far_parse_putc();
    EPILOG
}

void parse_out(void) MYCC {
    PROLOG(47)
    far_parse_out();
    EPILOG
}

void parse_nextreg(void) MYCC {
    PROLOG(47)
    far_parse_nextreg();
    EPILOG
}

void parse_asm(void) MYCC {
    get_token(); // skip '__asm__', tok is now tokLBrace
    if (tok != tokLBrace) { error(errExpected_c, '{'); return; }
    // Do NOT call get_token() here - far_parse_asm reads the body raw
    // starting from code which already points past '{'
    enter_asm_block();
    emit_strln(";#OPT_OFF");
    parse_asm_block();    
    emit_strln(";#OPT_ON");
    exit_asm_block();
}

void parse_type(uint8_t *type_id_out) MYCC {
    PROLOG(47)
    far_parse_type(type_id_out);
    EPILOG
}

void parse_funccall(SYMBOL* sym, PTR_LOCATION ptr_loc, uint8_t callee_type_id) MYCC {
    PROLOG(47)
    far_parse_funccall(sym, ptr_loc, callee_type_id);
    EPILOG
}

void do_exit(EXPR_RESULT exit_expr) {
    switch(tokMakeType) {
        case tokDot:                
            if (type_is_void(exit_expr.type_id) || (type_is_const(exit_expr.type_id) && exit_expr.value == 0)) {
                emit_instrln("xor a");                            
            } else {
                if ((type_is_pointer(exit_expr.type_id) && type_is_char(type_get_element_type_id(exit_expr.type_id)))) {
                    emit_rtl("ccpstr"); // convert C string to BASIC string
                    emit_instrln("xor a");
                    emit_instrln("scf");
                } else {
                    emit_instrln("ld a,l");
                    emit_instrln("or a");
                    emit_instr("jp z,"); emit_lblref(exit_lbl); emit_nl();
                    emit_instrln("scf");                    
                }                
            }
            break;
        case tokRaw:
            if (type_is_void(exit_expr.type_id)) {
                emit_instr("xor a");
                emit_instrln("ld b,a");
                emit_instrln("ld c,a");
            } else {
                emit_copy_hl_to_bc();
            }
            break;
    }
    emit_jp(exit_lbl);
}

void parse_return(void) MYCC {
    PROLOG(47)
    far_parse_return();
    EPILOG
}

void parse_exit(void) MYCC {
    PROLOG(47)
    far_parse_exit();
    EPILOG
}

void parse_vastart(void) MYCC {
    PROLOG(47)
    far_parse_vastart();
    EPILOG
}

void parse_vaend(void) MYCC {
    PROLOG(47)
    far_parse_vaend();
    EPILOG
}

/* Helper: parse a parameter signature list.
* Tracks argument types in func_arg_types[] and count in func_arg_count
* Sets func_is_variadic if an ellipsis is present.
* Declares argument locals if declare_locals is set.
* Called from parse_funcdecl() and parse_delegate_decl()
*/
void parse_org(void) MYCC {
    PROLOG(47)
    far_parse_org();
    EPILOG
}

 void parse_enum_member(EXPR_RESULT *result, const char* enum_name) MYCC {
    PROLOG(47)
    far_parse_enum_member(result, enum_name);
    EPILOG
}

void parse_bank(void) MYCC {
    PROLOG(47)
    far_parse_bank();
    EPILOG
}

void parse_hashif(uint16_t brklbl, uint16_t contlbl) MYCC {
    PROLOG(47)
    far_parse_hashif(brklbl, contlbl);
    EPILOG
}

SYMBOL declglb(uint8_t type_id, SYM_CLASS_SCOPE klass, const char* name, int16_t value) {
    SYMBOL lsym = addglb(name, klass, type_id, value);
    return lsym;
}

SYMBOL declloc(uint8_t type_id, SYM_CLASS_SCOPE klass, const char* name, int16_t offset) {
    SYMBOL lsym = addloc(name, klass, type_id, offset);
    return lsym;
}

SYMBOL decl_in_scope(uint8_t type_id, SYM_CLASS_SCOPE  klass, const char* name) {
    SYMBOL sym;
    if ((infunc || is_scoped()) && (!currbank || infunc)) {
        uint16_t size = type_size(type_id); 
        sym = findloc(name);
        if (is_defined(&sym)) error(errAlreadyDefined_s, name);
        sym = declloc(type_id, klass, name, bp_lastlocal);
        bp_lastlocal += size;
        if (localbytes < bp_lastlocal) localbytes = bp_lastlocal;        
    } else {
        sym = findglb(name);
        if (is_defined(&sym)) error(errAlreadyDefined_s, name);
        sym = declglb(type_id, klass, name, 0);
    }
    return sym;
}

void compile(const char *filename, char *outfilename) MYCC {
    /* Initialize type system */
    type_init();
   
    if (!asm_open(outfilename)) {
        src_close();
        printf("can't create '%s'", outfilename);
        return;        
    }
   
    // quick and dirty remove extension
    if (outfilename != NULL) {
        char* dot = strrchr(outfilename, '.');
        if (dot) *dot = '\0';
    }

    // reserve space for RTL include filename including ".rtl" extension
    // this might be a little larger than needed, but it's safe and avoids buffer overflows
    // if the base filename does not include an extension, we still have space for ".rtl" and null terminator
    rtlfilename = arena_alloc(strlen(filename) + 4 + 1); 
    const char* src = filename;
    char* dst = rtlfilename;
    while (*src && *src != '.') {
        *dst++ = *src++;
    }
    *dst = '\0';
    snprintf(rtlfilename, MAX_FILENAME_LEN, "%s.rtl", rtlfilename);
    
    parse(filename, outfilename, 1);
 
    if (!bankseen) {
        dump_globals_range(0, 0);
        dump_strings_range("str", 0, 0);
        emit_instrln("include \"%s\"", rtlfilename);
    }
    check_undefined();

    if (tokMakeType == tokNex) emit_nex(outfilename, start_lbl, stack_base);
    else if (tokMakeType == tokDot) emit_strln("PAGE_COUNT equ %d", page_count);
    if (dfe_enabled) {
        callgraph_mark_reachable();
        dump_function_dependencies();
    }
    asm_close();
    
    dump_rtl(rtlfilename);    
}

void parse_delegate_decl(void) MYCC {
    PROLOG(47)
    far_parse_delegate_decl();
    EPILOG
}