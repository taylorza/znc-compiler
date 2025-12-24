#include "znc.h"

#define NO_LABEL 0xFFFF     // no label defined

uint16_t retlbl = 0;        // function exit label

TOKEN tokMakeType = tokRaw; // type of make command

uint8_t infunc = 0;         // 1 if parsing a function
uint8_t inbank = 0;         // 1 if in explicit bank

uint8_t func_argcount;      // number of arguments for function being parsed
uint16_t locals_lbl;        // label of the EQU with the arg count
uint8_t localcount;         // number of live local variables
uint8_t maxlocalcount;      // highwater mark for local variables

uint16_t bp_lastlocal;      // base pointer of the last local
uint16_t localbytes;        // total bytes for locals
uint16_t exit_lbl;          // label for the global exit (return to BASIC for DOT commands)
uint16_t start_lbl;         // label for the start of the code
uint16_t stack_addr;        // stack pointer address
uint16_t stack_lbl;         // label for nex stack
uint16_t stack_size = 256;  // stack size

uint8_t hash_if_depth = 0; // depth of #if/#ifdef statements

SYMBOL declglb(TYPEREC type, SYM_CLASS klass, const char* name, int16_t value);
SYMBOL declloc(TYPEREC type, SYM_CLASS klass, const char* name, int16_t offset);

void parse_type(TYPEREC* type) MYCC;
void parse_funcdecl(TYPEREC rettype, const char* name) MYCC;

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
void parse_puts(void) MYCC;
void parse_out(void) MYCC;
void parse_nextreg(void) MYCC;
void parse_asm(int asmcol) MYCC;
void parse_org(void) MYCC;
void parse_bank(void) MYCC;
void parse_hashif(uint16_t brklbl, uint16_t contlbl) MYCC;
void parse_switch(uint16_t contlbl) MYCC;

void parse_make(const char* outfilename) MYCC;

EXPR_RESULT parse_onearg(void) MYCC;

static void parse(const char* sourcefile, const char* outfilename, uint8_t entrypoint) MYCC {
    if (!src_open(sourcefile)) {
        printf("can't open '%s'", sourcefile);
        exit(1);
    }
    printf("compiling %s\n", sourcefile);
    get_token();

    // initialization statements
    switch (tok) {
        case tokMake: parse_make(outfilename); break;            
        case tokOrg: parse_org(); break;
        default:
            // Assemble to memory default address will be $c000
            break;
    }

    uint16_t top_local_lbl = 0;
    if (entrypoint) {
        start_lbl = newlbl();
        emit_lbl(start_lbl);
        if (tokMakeType == tokNex) {
            stack_lbl = newlbl();
            emit_instr("ld sp,");
            emit_lblref(stack_lbl); emit_nl();
        }
        localbytes = 0;       
        exit_lbl = newlbl();
        emit_frame_prologue(1, exit_lbl);
        top_local_lbl = emit_alloclocals();
    }

    while (tok != tokEOS) {
        parse_statement(NO_LABEL, NO_LABEL);
    }

    if (entrypoint) {
        emit_instrln("xor a"); // clear A register and carry flag
        emit_frame_epilogue(1, exit_lbl);
        emit_lblequ16(top_local_lbl, localbytes);
    }
    src_close();
}

void emit_make_defines(TOKEN outputTok) {
    TYPEREC type = { .basetype = CHAR };
    make_const(&type);

    switch (outputTok) {
        case tokNex: declglb(type, VARIABLE, "__NEX", 1);  break;
        case tokDot: declglb(type, VARIABLE, "__DOT", 1);  break;
        case tokRaw: declglb(type, VARIABLE, "__RAW", 1);  break;
    }

    emit_strln("__NEX equ %d", outputTok == tokNex ? 1 : 0);
    emit_strln("__DOT equ %d", outputTok == tokDot ? 1 : 0);
    emit_strln("__RAW equ %d", outputTok == tokRaw ? 1 : 0);
}

void parse_make(const char *filename) MYCC {
    get_token(); // skip 'make'

    
    switch (tok) {
        case tokNex:            
        case tokRaw:
        case tokDot:     
            tokMakeType = tok; // remember type of make command
            
            get_token(); // skip output type
            if (tok == tokString) {
                strcpy(outfilename, token);
                get_token(); // skip string
            }
            else {
                strcpy(outfilename, filename);
                if (tokMakeType == tokNex) strcat(outfilename, ".nex");
            }
            expect_semi(); 
            
            if (tokMakeType == tokRaw || tokMakeType == tokDot)
                emit_output(outfilename, tokMakeType);
            
            if (tok == tokBank) parse_bank();
            else if (tokMakeType == tokNex) emit_bank(0, 0);

            if (tok == tokOrg) parse_org();
            else if (tokMakeType == tokNex) emit_org(0xc000);
            break;        
        default:
            error(errSyntax);
            break;
    }
    
    emit_make_defines(tokMakeType);
}

EXPR_RESULT parse_onearg(void) MYCC {
    EXPR_RESULT expr;
    get_token(); // skip leading token
    expect_LParen();
    expr = parse_expr(0);
    expect_RParen();
    return expr;
}

static void parse_statement_block(uint16_t brklbl, uint16_t contlbl) MYCC {
    get_token(); // skip '{'
    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;
    uint16_t old_bp = bp_lastlocal;
    while (tok != tokEOS && tok != tokRBrace) {
        parse_statement(brklbl, contlbl);
    }   
    if (maxlocalcount < localcount) maxlocalcount = localcount;
    bp_lastlocal = old_bp;
    localcount = old_localcount;
    expect(tokRBrace, '}');
    pop_frame(blockframe);    
}

void parse_statement(uint16_t brklbl, uint16_t contlbl) MYCC {
    switch (tok) {
        case tokConst:
        case tokVoid:
        case tokChar:
        case tokInt:
            parse_decl();
            break;

        case tokLBrace:
            parse_statement_block(brklbl, contlbl);
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
        case tokPuts: parse_puts(); break;
        case tokOut: parse_out(); break;
        case tokNextReg: parse_nextreg(); break;
        case tokAsm: parse_asm(0); break;
        case tokInclude: parse_include(); break;
        case tokOrg: parse_org(); break;
        case tokBank: parse_bank(); break;
        case tokSemi: get_token(); break; // empty statement

        case tokHashIf: 
        case tokHashIfDef:
        case tokHashIfNDef:
            parse_hashif(brklbl, contlbl); 
            break;

        case tokHashElse:
        case tokHashEndif:
            if (hash_if_depth == 0) error(errSyntax);
            break;

        default:
            parse_expr(0);
            expect_semi();
            break;
    }
}

void parse_include(void) MYCC {
    get_token(); // skip 'include'
    if (tok != tokString) error(errSyntax);
    
    parse(token, NULL, 0);

    get_token(); // skip filename    
}

void parse_decl(void) MYCC {
    TYPEREC type;

    uint8_t constdecl = 0;
    if (tok == tokConst) {
        constdecl = 1;
        get_token(); // skip 'const'
    }

    parse_type(&type);
    if (constdecl) {
        if (is_void(&type)) error(errSyntax);
        if (is_ptr(&type)) error(errSyntax);
        if (is_array(&type)) error(errSyntax);
        make_const(&type);
    }

    char name[MAX_IDENT_LEN + 1];
    strncpy(name, token, MAX_IDENT_LEN);

    if (tok != tokIdent) error(errSyntax);
    get_token(); // skip name

    if (tok == tokLParen) {
        if (infunc || constdecl) error(errSyntax);
        parse_funcdecl(type, name);
    }
    else {
        SYMBOL sym;

        if (is_void(&type) && !is_ptr(&type)) error(errSyntax);

        for (;;) {
            if (infunc || is_scoped()) {
                uint16_t size = type_size(&type); 
                sym = findloc(name);
                if (is_defined(&sym)) error(errAlreadyDefined_s, name);
                sym = declloc(type, VARIABLE, name, bp_lastlocal);
                bp_lastlocal += size;
                if (localbytes < bp_lastlocal) localbytes = bp_lastlocal;
            } else {
                sym = findglb(name);
                if (is_defined(&sym)) error(errAlreadyDefined_s, name);
                sym = declglb(type, VARIABLE, name, 0);
            }
                

            if (tok == tokAssign) {
                parse_assign(0, &sym, 0, type);
            }
            else if (constdecl) error(errSyntax);

            if (tok != tokComma) break;
            get_token(); // skip ','
            strncpy(name, token, MAX_IDENT_LEN);
            get_token(); // skip name
        }
        expect_semi();
    }
}

void parse_if(uint16_t brklbl, uint16_t contlbl) MYCC {
    static uint16_t lblEndIf = 0;
    uint16_t lblFalse = newlbl();
    
    uint16_t old_endif = lblEndIf;
    lblEndIf = 0;
    parse_onearg(); // (expr)
    emit_jp_false(lblFalse);
    parse_statement(brklbl, contlbl);

    if (tok == tokElse) {
        get_token(); // skip 'else'
        if (lblEndIf == 0) lblEndIf = newlbl();
        emit_jp(lblEndIf);
    }        
    emit_lbl(lblFalse);
    
    if (lblEndIf)
    {
        parse_statement(brklbl, contlbl);
        if (lblEndIf) {
            emit_lbl(lblEndIf);
            lblEndIf = 0;
        }
    }
    lblEndIf = old_endif;
}

void parse_switch(uint16_t contlbl) MYCC {
    uint16_t lblTbl = newlbl();
    uint16_t lblDefault = NO_LABEL;
    uint16_t lblDone = newlbl();
    
    uint16_t values[MAX_CASE];
    uint16_t labels[MAX_CASE];
    uint8_t case_count = 0;
    uint8_t last_break = 0;

    parse_onearg(); // (expr)
    emit_jp(lblTbl);

    expect_LBrace();
    while (tok == tokCase || tok == tokDefault) {
        last_break = 0;
        if (tok == tokCase) {
            get_token(); // skip 'case'
            EXPR_RESULT expr_result = parse_expr_delayconst(0);
            if (!is_const(&expr_result.type)) {
                error_expect_const();
            }
            uint16_t lblCase = newlbl();
            if (case_count == MAX_CASE) error(errTooManySymbols);
            values[case_count] = expr_result.value;
            labels[case_count++] = lblCase;
            emit_lbl(lblCase);
        } else if (tok == tokDefault) {
            if (lblDefault != NO_LABEL) error(errAlreadyDefined_s, "default");
            get_token(); // skip 'default'
            lblDefault = newlbl();   
            emit_lbl(lblDefault);
        }
        expect_colon();
        if (tok == tokLBrace) 
            parse_statement_block(lblDone, contlbl);
        else
            while (tok != tokEOS && tok != tokBreak && tok != tokCase && tok != tokRBrace) {
                parse_statement(lblDone, contlbl);
            }
        if (tok == tokBreak) {
            parse_break(lblDone);            
            last_break = 1;          
        }
    }
    if (!last_break) emit_jp(lblDone);
    expect_RBrace();
    emit_lbl(lblTbl);
    emit_instrln("ld b,%d", case_count);
    emit_rtl("ccswitch");
    for(int i=0; i<case_count; ++i) {
        emit_instr("dw %d,", values[i]); emit_lblref(labels[i]);
        emit_nl();
    }
    if (lblDefault != NO_LABEL) emit_jp(lblDefault);
    emit_lbl(lblDone);
}

void parse_while(void) MYCC {
    uint16_t lblCond = newlbl();
    uint16_t lblEndWhile = newlbl();
    
    emit_lbl(lblCond);

    parse_onearg(); // (expr)

    emit_jp_false(lblEndWhile);
    parse_statement(lblEndWhile, lblCond);
    emit_jp(lblCond);

    emit_lbl(lblEndWhile);    
}

void parse_for(void) MYCC {
    get_token(); // skip 'for'
    expect_LParen();

    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;

    uint16_t lblCond=NO_LABEL;
    uint16_t lblEndFor, brklbl;
    uint16_t lblBody = newlbl();
    uint16_t lblPost, contlbl;

    lblEndFor = brklbl = newlbl();
    lblPost = contlbl = NO_LABEL;


    // parse initializer
    if (tok == tokChar || tok == tokInt) {
        parse_decl();
    }
    else {
        if (tok != tokSemi) parse_expr(0);
        expect_semi();
    }
    
    // parse condition
    if (tok != tokSemi) {
        lblCond = newlbl();
        emit_lbl(lblCond);
        parse_expr(0); 
        emit_jp_false(lblEndFor);
    }
    expect_semi();

    // parse post statement
    if (tok != tokRParen) {
        emit_jp(lblBody);
        lblPost = contlbl = newlbl();
        emit_lbl(lblPost);
        parse_expr(0);
        if (lblCond != NO_LABEL) emit_jp(lblCond);
    } else {
        contlbl = lblBody;
    }
    expect_RParen();

    emit_lbl(lblBody);
    parse_statement(brklbl, contlbl);
    if (lblPost != NO_LABEL)
        emit_jp(lblPost);
    else if (lblCond != NO_LABEL)
        emit_jp(lblCond);
    else
        emit_jp(lblBody);
    emit_lbl(lblEndFor);

    if (maxlocalcount < localcount) maxlocalcount = localcount;
    localcount = old_localcount;
    pop_frame(blockframe);    
}

void parse_break(uint16_t brklbl) MYCC {
    get_token(); // skip 'break'
    if (brklbl == NO_LABEL) error(errSyntax);
    expect_semi();    
    emit_jp(brklbl);
}

void parse_continue(uint16_t contlbl) MYCC {
    get_token(); // skip 'continue'
    if (contlbl == NO_LABEL) error(errSyntax);
    expect_semi();    
    emit_jp(contlbl);
}

void parse_putc(void) MYCC {
    parse_onearg(); // (expr)
    expect_semi();

    emit_rtl("putc");
}

void parse_puts(void) MYCC {
    parse_onearg(); // (expr)
    expect_semi();

    emit_rtl("puts");   
}

void parse_out(void) MYCC {
    get_token(); // skip 'out'
    expect_LParen();
   
    parse_expr(0);
    emit_instrln("ld b,h");
    emit_instrln("ld c,l");    
    expect_comma();
    parse_expr(0);
    emit_instrln("out (c),l");
    expect_RParen();
    expect_semi();
}

void parse_nextreg(void) MYCC {
    get_token(); // skip 'nextreg'
    expect_LParen();
    
    parse_expr(0);
    emit_push();
    expect_comma();
    parse_expr(0);
    emit_pop();

    emit_instrln("ld bc,9275");
    emit_instrln("out (c),e");
    emit_instrln("inc b");
    emit_instrln("out (c),l");
   
    expect_RParen();
    expect_semi();    
}

void parse_asm(int asmcol) MYCC {
    if (asmcol == 0) asmcol = token_col;
    get_token(); // skip 'asm'
    expect_LBrace();
    while (tok != tokRBrace && tok != tokEOS) {
        int last_token_line = token_line;
        if (token_col <= asmcol) {
            emit_str("%s", token);
        } else {
            emit_str("  %s ", token);
        }
        TOKEN lasttok = tok; 
        TOKEN_TYPE lasttoken_type = token_type;

        get_token();
        while (token_line == last_token_line && tok != tokRBrace && tok != tokEOS) {
            lasttok = tok; 
            if (tok == tokNumber) {
                emit_n(intval);
            }
            else if (token_type == ttString) {
                emit_str("\"%s\"",token);
            }
            else {
                emit_str(token);
            }
            get_token();                        
            if ((tok == tokIdent || tok == tokNone && token_type != ttError) && (lasttok == tokIdent)) emit_ch(' ');            
        }
        emit_nl();
    }
    expect_RBrace();    
}

void parse_type(TYPEREC *type) MYCC {
    switch (tok) {
        case tokVoid: type->basetype = VOID; break;
        case tokChar: type->basetype = CHAR; break;
        case tokInt: type->basetype = INT; break;
        default: 
            error(errSyntax); 
            tok = tokInt;
            break;
    }    
    make_scalar(type);
    
    get_token(); // skip type

    if (tok == tokStar) {
        get_token(); // skip '*'
        make_ptr(type); // set as pointer
    }
    else if (tok == tokLBrack) {
        get_token(); // skip '['
        if (tok == tokRBrack) {
            make_ptr(type);
        } else {
            EXPR_RESULT dim = parse_expr_delayconst(0);
            if (!is_const(&dim.type)) error_expect_const();
            if (dim.value > 0) {
                if (is_void(type)) error(errSyntax);
                make_array(type, dim.value);
            }
            else make_ptr(type); // 0 size array is a pointer
        }
        expect(tokRBrack, errSyntax);       
    }
}

static void clean_stack(int16_t bytes) MYCC {
    if (!bytes) return;
    
    if (bytes < 8) {
        while ((bytes-2) >= 0) {
            emit_instrln("pop bc");
            bytes -= 2;
        }
        while (bytes) {
            emit_instrln("dec sp");
            bytes--;
        }
        return;
    } else  {
        emit_swap();
        emit_ld_immed(); emit_n(bytes); emit_nl();
        emit_instrln("add hl,sp");
        emit_instrln("ld sp,hl");        
        emit_swap();
    }
}

void parse_funccall(SYMBOL* sym) MYCC {
    get_token(); // skip '('
    uint8_t argcount = 0;
    while (tok != tokRParen) {
        ++argcount;
        parse_expr(0);
        emit_push();        
        if (tok != tokComma) break;
        get_token(); // skip ','
    }
    if (is_func_or_proto(sym) && argcount != sym->offset) error(errArgMismatch);
    expect_RParen();

    emit_callsym(sym);
    clean_stack(sym->offset * 2);
}

void do_exit(EXPR_RESULT exit_expr) {
    switch(tokMakeType) {
        case tokDot:                
            if (is_void(&exit_expr.type) || (is_const(&exit_expr.type) && exit_expr.value == 0)) {
                emit_instrln("xor a");                            
            } else {
                if ((is_ptr(&exit_expr.type) && is_char(&exit_expr.type)) 
                        || is_string(&exit_expr.type)) {
                emit_rtl("ccpstr"); // convert C string to BASIC string
                emit_instrln("xor a");                      
                } else {
                    emit_instrln("ld a,l");                    
                }
                emit_instrln("scf");
            }
            break;
        case tokRaw:
            if (is_void(&exit_expr.type)) {
                emit_instr("xor a");
                emit_instrln("ld b,a");
                emit_instrln("ld c,a");
            } else {
                emit_instrln("ld b,h");
                emit_instrln("ld c,l");
            }
            break;
    }
    emit_jp(exit_lbl);
}

void parse_return(void) MYCC {
    EXPR_RESULT expr_result = { .type = void_type };

    get_token(); // skip 'return';
    
    if (tok != tokSemi) {
        expr_result = parse_expr(0);
    }
    expect_semi();
    if (infunc)
        emit_jp(retlbl);
    else {
        do_exit(expr_result);
    }
}

void parse_exit(void) MYCC {
    EXPR_RESULT expr_result = parse_onearg();
    do_exit(expr_result);
}

void parse_funcdecl(TYPEREC rettype, const char* name) MYCC {
    get_token(); // skip '('

    TYPEREC argtype;
    char argName[MAX_IDENT_LEN + 1];

    SYMBOL symfunc = lookupIdent(name);
    uint8_t defined = 0;

    if (not_defined(&symfunc)) {
        symfunc = declglb(rettype, FUNCTION, name, 0);
    } else if (symfunc.klass != FUNCTION_PROTO) {
        defined = 1;
    }

    uint16_t oldretlbl = retlbl;
    retlbl = newlbl(); 
    uint16_t funcframe = push_frame();
    func_argcount = 0;
    while (tok != tokRParen) {
        parse_type(&argtype);
        if (is_array(&argtype)) make_ptr(&argtype);        
        strncpy(argName, token, MAX_IDENT_LEN);           
        declloc(argtype, ARGUMENT, argName, func_argcount++);
        get_token(); // skip arg name
        if (tok == tokComma) get_token(); // skip ','
    }
    expect_RParen();
   
    if (defined || symfunc.klass == FUNCTION_PROTO) {
        if (func_argcount != symfunc.offset) error(errDeclMismatch);
    }

    symfunc.offset = func_argcount;
    if (tok == tokSemi) {        
        if (!defined) symfunc.klass = FUNCTION_PROTO;
    } else {
        if (defined) error(errAlreadyDefined_s, name);

        symfunc.klass = FUNCTION;
        infunc = 1;
        uint16_t skiplbl = newlbl();
        emit_jp(skiplbl);

        emit_sname(name); emit_nl();

        if (tok == tokAsm) {
            parse_asm(2);
        }
        else {
            if (tok != tokLBrace) error(errExpected_c, "}");

            uint16_t oldlocalbytes = localbytes;
            maxlocalcount = 0;
            bp_lastlocal = 0;
            localbytes = 0;
            emit_frame_prologue(0, retlbl);
            locals_lbl = emit_alloclocals();

            parse_statement_block(NO_LABEL, NO_LABEL);

            emit_frame_epilogue(0, retlbl);
            
            emit_lblequ16(locals_lbl, localbytes);
            localbytes = oldlocalbytes;
        }
        emit_lbl(skiplbl);

        pop_frame(funcframe);
        infunc = 0;
        retlbl = oldretlbl;
    }
    updatesym(&symfunc);
}

void parse_org(void) MYCC {
    get_token(); // skip 'org'
    EXPR_RESULT expr_result = parse_expr_delayconst(0);
    if (!is_const(&expr_result.type)) error_expect_const();
    emit_org(expr_result.value);
    expect_semi();
}

void parse_bank(void) MYCC {
    if (inbank) error(errSyntax);

    inbank = 1;
    get_token(); // skip bank
    uint8_t bankid;
    uint16_t offset = 0;
    expect_LParen();

    EXPR_RESULT bankid_result = parse_expr_delayconst(0);
    if (!is_const(&bankid_result.type)) error_expect_const();
    if (bankid_result.value > 255) error(errInvalid_s, "bank");
    bankid = (uint8_t)bankid_result.value;
 
    if (tok == tokComma) {
        get_token(); // skip ','
        
        EXPR_RESULT offset_result = parse_expr_delayconst(0);
        if (!is_const(&offset_result.type)) error_expect_const();
        offset = offset_result.value;
    }
    expect_RParen();
    emit_bank(bankid, offset);
    parse_statement_block(NO_LABEL, NO_LABEL);
    inbank = 0;
}

void parse_conditional(uint8_t active, uint16_t brklbl, uint16_t contlbl) MYCC {
    uint8_t current_if_depth = hash_if_depth;
    if (active) {
        while (tok != tokHashElse && tok != tokHashEndif && tok != tokEOS) {
            parse_statement(brklbl, contlbl);
        }
    } else {
        while ((current_if_depth != hash_if_depth || (tok != tokHashElse && tok != tokHashEndif)) && tok != tokEOS) {
            if (tok == tokHashIf || tok == tokHashIfDef) {
                ++hash_if_depth;
            } else if (tok == tokHashEndif) {
                if (--hash_if_depth == 0) error(errSyntax);
            }
            get_token();
        }
    }
}

void parse_hashif(uint16_t brklbl, uint16_t contlbl) MYCC {
    TOKEN op = tok;

    get_token(); // skip '#if' or '#ifdef' or '#ifndef'
    
    uint8_t active;
    if (op == tokHashIfDef || op == tokHashIfNDef) {
        SYMBOL sym = lookupIdent(token);
        active = is_defined(&sym);
        if (op == tokHashIfNDef) active = !active;
        get_token(); // skip identifier
    } else {
        EXPR_RESULT expr_result = parse_expr_delayconst(0);
        if (!is_const(&expr_result.type)) error_expect_const();
        active = expr_result.value != 0;
    }

    ++hash_if_depth;
    parse_conditional(active, brklbl, contlbl);
    if (tok == tokHashElse) {
        get_token(); // skip '#else'
        parse_conditional(!active, brklbl, contlbl);
    }
    
    if (tok != tokHashEndif) error(errSyntax);
    --hash_if_depth;
    get_token(); // skip '#endif'
}

SYMBOL declglb(TYPEREC type, SYM_CLASS klass, const char* name, int16_t value) {
    SYMBOL lsym = addglb(name, klass, type, value);
    return lsym;
}

SYMBOL declloc(TYPEREC type, SYM_CLASS klass, const char* name, int16_t offset) {
    SYMBOL lsym = addloc(name, klass, type, offset);
    return lsym;
}

void compile(const char *filename, const char *asmfilename) MYCC {
    if (!asm_open(asmfilename)) {
        src_close();
        printf("can't create '%s'", asmfilename);
        return;        
    }
   
    // quick and dirty remove extension
    char *dot = strrchr(asmfilename, '.');
    if (dot) *dot='\0';
    
    parse(filename, asmfilename, 1);
    
    dump_rtl();
    dump_globals();
    dump_strings();

    if (tokMakeType == tokNex) emit_nex(outfilename, start_lbl, stack_lbl, stack_size);

    asm_close();
}