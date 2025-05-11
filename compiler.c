#include "znc.h"

uint16_t retlbl = 0;        // function exit label
uint16_t brklbl = 0;        // in scope break label
uint16_t contlbl = 0;       // in scope continue label

uint8_t emitNex = 0;        // 1 if generating a NEX file
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

SYMBOL* declglb(TYPEREC type, SYM_CLASS klass, const char* name, int16_t value);
SYMBOL* declloc(TYPEREC type, SYM_CLASS klass, const char* name, int16_t offset);

void parse_type(TYPEREC* type) MYCC;
void parse_funcdecl(TYPEREC rettype, const char* name) MYCC;

void parse_include(void) MYCC;
void parse_decl(void) MYCC;
void parse_statement(void) MYCC;
void parse_if(void) MYCC;
void parse_while(void) MYCC;
void parse_for(void) MYCC;
void parse_break(void) MYCC;
void parse_continue(void) MYCC;
void parse_return(void) MYCC;
void parse_putc(void) MYCC; 
void parse_puts(void) MYCC;
void parse_out(void) MYCC;
void parse_nextreg(void) MYCC;
void parse_asm(int asmcol) MYCC;
void parse_org(void) MYCC;
void parse_bank(void) MYCC;
void parse_hashif(void) MYCC;

void parse_make(const char* outfilename) MYCC;

void parse_onearg(void) MYCC;

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
        if (emitNex) {
            stack_lbl = newlbl();
            emit_instr("ld sp,");
            emit_lblref(stack_lbl); emit_nl();
        }
        localbytes = 0;       
        exit_lbl = newlbl();
        emit_frame_prologue();
        top_local_lbl = emit_alloclocals();
    }

    while (tok != tokEOS) {
        parse_statement();
    }

    if (entrypoint) {
        emit_lbl(exit_lbl);
        emit_frame_epilogue();
#ifdef __ZXNEXT
        emit_instrln("xor a"); // Clear A=0 and carry for clean exit from DOT command
        emit_ret();
#else
        emit_instrln("jr $");
#endif
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
    TOKEN outputTok = tokNone;
    get_token(); // skip 'make'
    switch (tok) {
        case tokNex:
            emitNex = (tok == tokNex);
        case tokRaw:
        case tokDot:         
            outputTok = tok;            
            get_token(); // skip output type
            if (tok == tokString) {
                strcpy(outfilename, token);
                get_token(); // skip string
            }
            else {
                strcpy(outfilename, filename);
                if (outputTok == tokNex) strcat(outfilename, ".nex");
            }
            expect_semi(); 
            
            if (outputTok == tokRaw || outputTok == tokDot)
                emit_output(outfilename, outputTok);
            
            if (tok == tokBank) parse_bank();
            else if (outputTok == tokNex) emit_bank(0, 0);

            if (tok == tokOrg) parse_org();
            else if (outputTok == tokNex) emit_org(0xc000);
            break;        
        default:
            error(errSyntax);
            break;
    }
    
    emit_make_defines(outputTok);
}

void parse_onearg(void) MYCC {
    get_token(); // skip leading token
    expect_LParen();
    parse_expr(0);
    expect_RParen();
}

static void parse_statement_block(void) MYCC {
    get_token(); // skip '{'
    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;
    uint16_t old_bp = bp_lastlocal;
    while (tok != tokEOS && tok != tokRBrace) {
        parse_statement();
    }   
    if (maxlocalcount < localcount) maxlocalcount = localcount;
    bp_lastlocal = old_bp;
    localcount = old_localcount;
    pop_frame(blockframe);
    expect(tokRBrace, '}');
}

void parse_statement(void) MYCC {
    switch (tok) {
        case tokConst:
        case tokVoid:
        case tokChar:
        case tokInt:
            parse_decl();
            break;

        case tokLBrace:
            parse_statement_block();
            break;
        case tokIf: parse_if(); break;
        case tokWhile: parse_while(); break;
        case tokFor: parse_for(); break;
        case tokBreak: parse_break(); break;
        case tokContinue: parse_continue(); break;
        case tokReturn: parse_return(); break;
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
            parse_hashif(); 
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
        SYMBOL* sym;

        if (is_void(&type) && !is_ptr(&type)) error(errSyntax);

        for (;;) {
            if (infunc || is_scoped()) {
                uint16_t size = type_size(&type); 
                sym = findloc(name);
                if (sym) error(errAlreadyDefined_s, name);
                sym = declloc(type, VARIABLE, name, bp_lastlocal);
                bp_lastlocal += size;
                if (localbytes < bp_lastlocal) localbytes = bp_lastlocal;
            } else {
                sym = findglb(name);
                if (sym) error(errAlreadyDefined_s, name);
                sym = declglb(type, VARIABLE, name, 0);
            }
                

            if (tok == tokAssign) {
                parse_assign(0, sym, 0, type);
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

void parse_if(void) MYCC {
    static uint16_t lblEndIf = 0;
    uint16_t lblFalse = newlbl();
    
    uint16_t old_endif = lblEndIf;
    lblEndIf = 0;
    parse_onearg(); // (expr)
    emit_jp_false(lblFalse);
    parse_statement();

    if (tok == tokElse) {
        get_token(); // skip 'else'
        if (lblEndIf == 0) lblEndIf = newlbl();
        emit_jp(lblEndIf);
    }        
    emit_lbl(lblFalse);
    
    if (lblEndIf)
    {
        parse_statement();
        if (lblEndIf) {
            emit_lbl(lblEndIf);
            lblEndIf = 0;
        }
    }
    lblEndIf = old_endif;
}

void parse_while(void) MYCC {
    uint16_t old_brklbl = brklbl;
    uint16_t old_contlbl = contlbl;

    uint16_t lblCond = contlbl = newlbl();
    uint16_t lblEndWhile = brklbl = newlbl();
    
    emit_lbl(lblCond);

    parse_onearg(); // (expr)

    emit_jp_false(lblEndWhile);
    parse_statement();
    emit_jp(lblCond);

    emit_lbl(lblEndWhile);
    
    brklbl = old_brklbl;
    contlbl = old_contlbl;
}

void parse_for(void) MYCC {
    get_token(); // skip 'for'
    expect_LParen();

    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;

    uint16_t old_brklbl = brklbl;
    uint16_t old_contlbl = contlbl;

    uint16_t lblCond=65535;
    uint16_t lblEndFor = brklbl = newlbl();
    uint16_t lblBody = newlbl();
    uint16_t lblPost = contlbl = 65535;

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
        if (lblCond != 65535) emit_jp(lblCond);
    } else {
        contlbl = lblBody;
    }
    expect_RParen();

    emit_lbl(lblBody);
    parse_statement();
    if (lblPost != 65535)
        emit_jp(lblPost);
    else if (lblCond != 65535)
        emit_jp(lblCond);
    else
        emit_jp(lblBody);
    emit_lbl(lblEndFor);

    if (maxlocalcount < localcount) maxlocalcount = localcount;
    localcount = old_localcount;
    pop_frame(blockframe);

    brklbl = old_brklbl;
    contlbl = old_contlbl;
}

void parse_break(void) MYCC {
    get_token(); // skip 'break'
    if (!brklbl) error(errSyntax);
    expect_semi();
    
    emit_jp(brklbl);
}

void parse_continue(void) MYCC {
    get_token(); // skip 'continue'
    if (!contlbl) error(errSyntax);
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
        }
        else if (tok != tokNumber || intval < 0) {
            error(errSyntax);
        }
        else {
            if (intval > 0) {
                if (is_void(type)) error(errSyntax);
                make_array(type, intval);
            }
            else make_ptr(type); // 0 size array is apointer
            get_token(); // skip dimension
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

void parse_return(void) MYCC {
    get_token(); // skip 'return';
    if (tok != tokSemi) {
        parse_expr(0);
    }
    expect_semi();
    emit_jp(retlbl);
}

void parse_funcdecl(TYPEREC rettype, const char* name) MYCC {
    get_token(); // skip '('

    TYPEREC argtype;
    char argName[MAX_IDENT_LEN + 1];

    SYMBOL* symfunc = lookupIdent(name);
    uint8_t defined = 0;

    if (!symfunc) {
        symfunc = declglb(rettype, FUNCTION, name, 0);
    } else if (symfunc->klass != FUNCTION_PROTO) {
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
   
    if (defined || symfunc->klass == FUNCTION_PROTO) {
        if (func_argcount != symfunc->offset) error(errDeclMismatch);
    }

    symfunc->offset = func_argcount;
    if (tok == tokSemi) {        
        if (!defined) symfunc->klass = FUNCTION_PROTO;
    } else {
        if (defined) error(errAlreadyDefined_s, name);

        symfunc->klass = FUNCTION;
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
            emit_frame_prologue();
            locals_lbl = emit_alloclocals();

            parse_statement_block();

            emit_lbl(retlbl);
            emit_frame_epilogue();
            emit_ret();

            emit_lblequ16(locals_lbl, localbytes);
            localbytes = oldlocalbytes;
        }
        emit_lbl(skiplbl);

        pop_frame(funcframe);
        infunc = 0;
        retlbl = oldretlbl;
    }
}

void parse_org(void) MYCC {
    get_token(); // skip 'org'
    EXPR_RESULT expr_result = parse_expr_delayconst(0);
    if (!is_const(&expr_result.type)) error_expect_const();
    emit_org(expr_result.value);
    get_token(); // skip address
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
    get_token(); // skip bankid
    if (tok == tokComma) {
        get_token(); // skip ','
        if (token_type != ttNumber) error(errSyntax);
        offset = intval;
        get_token(); // skip offset 
    }
    expect_RParen();
    emit_bank(bankid, offset);
    parse_statement_block();
    inbank = 0;
}

void parse_conditional(uint8_t active) MYCC {
    uint8_t current_if_depth = hash_if_depth;
    if (active) {
        while (tok != tokHashElse && tok != tokHashEndif && tok != tokEOS) {
            parse_statement();
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

void parse_hashif(void) MYCC {
    TOKEN op = tok;

    get_token(); // skip '#if' or '#ifdef' or '#ifndef'
    
    uint8_t active;
    if (op == tokHashIfDef || op == tokHashIfNDef) {
        active = lookupIdent(token) != NULL;
        if (op == tokHashIfNDef) active = !active;
        get_token(); // skip identifier
    } else {
        EXPR_RESULT expr_result = parse_expr_delayconst(0);
        if (!is_const(&expr_result.type)) error_expect_const();
        active = expr_result.value != 0;
    }

    ++hash_if_depth;
    parse_conditional(active);
    if (tok == tokHashElse) {
        get_token(); // skip '#else'
        parse_conditional(!active);
    }
    
    if (tok != tokHashEndif) error(errSyntax);
    --hash_if_depth;
    get_token(); // skip '#endif'
}

SYMBOL* declglb(TYPEREC type, SYM_CLASS klass, const char* name, int16_t value) {
    return addglb(name, klass, type, value);
}

SYMBOL* declloc(TYPEREC type, SYM_CLASS klass, const char* name, int16_t offset) {
    return addloc(name, klass, type, offset);
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

    if (emitNex) emit_nex(outfilename, start_lbl, stack_lbl, stack_size);

    asm_close();
}