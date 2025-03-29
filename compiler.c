#include "znc.h"

uint16_t retlbl = 0;        // function exit label
uint16_t brklbl = 0;        // in scope break label
uint16_t contlbl = 0;       // in scope continue label

uint8_t infunc;             // 1 if parsing a function
uint8_t func_argcount;      // number of arguments for function being parsed
uint16_t locals_lbl;   // label of the EQU with the arg count
uint8_t localcount;         // number of live local variables
uint8_t maxlocalcount;      // highwater mark for local variables

uint16_t bp_lastarg;        // base pointer of the last argument
uint16_t bp_lastlocal;      // base pointer of the last local
uint16_t localbytes;        // total bytes for locals

static void expect_semi(void) {
    expect(tokSemi, errExpectSemi);
}
SYMBOL* declglb(TYPEREC type, SYM_CLASS klass, const char* name, int16_t value);
SYMBOL* declloc(TYPEREC type, SYM_CLASS klass, const char* name, int16_t offset);

void parse_type(TYPEREC* type);
void parse_funcdecl(TYPEREC rettype, const char* name);

void parse_decl(void);
void parse_statement(void);
void parse_if(void);
void parse_while(void);
void parse_for(void);
void parse_break(void);
void parse_continue(void);
void parse_return(void);
void parse_putc(void); 
void parse_puts(void);
void parse_asm(void);

void parse_onearg(void);

static void parse(void) {
    get_token();
    while (tok != tokEOS) {
        switch (tok) {
            case tokVoid:
            case tokChar:
            case tokInt:            
                parse_decl();                
                break;
            case tokSemi: get_token(); break;
            default:            
                parse_statement();
                break;
        }
    }
#ifdef __ZXNEXT
    emit_ret();
#else
    emit_instr("jr $");
#endif
}

void parse_onearg(void) {
    get_token(); // skip leading token
    expect(tokLParen, errExpectLParen);
    parse_expr(0);
    expect(tokRParen, errExpectRParen);
}

void parse_decl(void) {
    TYPEREC type;
    
    parse_type(&type);
    
    char name[MAX_IDENT_LEN + 1];
    strncpy(name, token, MAX_IDENT_LEN);

    get_token(); // skip name

    if (tok == tokLParen) {
        if (infunc) error(errSyntax);
        parse_funcdecl(type, name);
    } else {
        SYMBOL* sym;

        if (is_void(&type) && !is_ptr(&type)) error(errSyntax);

        for(;;) {
            if (infunc) {
                uint16_t size = type_size(&type);
                bp_lastlocal = size * localcount++;
                sym = declloc(type, VARIABLE, name, bp_lastlocal);
                if (localbytes < bp_lastlocal + size) localbytes = bp_lastlocal + size;
            }
            else
                sym = declglb(type, VARIABLE, name, 0);

            if (tok == tokAssign) {
                parse_assign(0, sym, 0, type);
            }
            
            if (tok != tokComma) break;
            get_token(); // skip ','
            strncpy(name, token, MAX_IDENT_LEN);
            get_token(); // skip name
        }
        expect_semi();
    }
}

static void parse_statement_block(void) {
    get_token(); // skip '{'
    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;
    while (tok != tokEOS && tok != tokRBrace) {
        parse_statement();
    }
    if (maxlocalcount < localcount) maxlocalcount = localcount;
    localcount = old_localcount;
    pop_frame(blockframe);
    expect(tokRBrace, errSyntax);
}

void parse_statement(void) {
    switch (tok) {
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
        case tokAsm: parse_asm(); break;
        case tokSemi: get_token(); break; // empty statement
        default:
            parse_expr(0);
            expect_semi();
            break;
    }
}

void parse_if(void) {
    static uint16_t lblEndIf = 0;
    uint16_t lblFalse = newlbl();
    
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
}

void parse_while(void) {
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

void parse_for(void) {
    get_token(); // skip 'for'
    expect(tokLParen, errExpectLParen);

    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;

    uint16_t lblCond = newlbl();
    uint16_t lblEndFor = brklbl = newlbl();
    uint16_t lblBody = newlbl();
    uint16_t lblPost = contlbl = newlbl();

    // parse initializer
    if (tok == tokChar || tok == tokInt) {
        parse_decl();
    }
    else {
        parse_expr(0);
        expect_semi();
    }
    
    // parse condition
    emit_lbl(lblCond);
    parse_expr(0); expect_semi();
    emit_jp_false(lblEndFor);
    emit_jp(lblBody);

    // parse post statement
    emit_lbl(lblPost);
    parse_expr(0); expect(tokRParen, errExpectRParen);
    emit_jp(lblCond);

    emit_lbl(lblBody);
    parse_statement();
    emit_jp(lblPost);
    emit_lbl(lblEndFor);

    if (maxlocalcount < localcount) maxlocalcount = localcount;
    localcount = old_localcount;
    pop_frame(blockframe);
}

void parse_break(void) {
    get_token(); // skip 'break'
    if (!brklbl) error(errSyntax);
    expect_semi();
    
    emit_jp(brklbl);
}

void parse_continue(void) {
    get_token(); // skip 'continue'
    if (!contlbl) error(errSyntax);
    expect_semi();
    
    emit_jp(contlbl);
}

void parse_putc(void) {
    parse_onearg(); // (expr)
    expect_semi();

    emit_instr("ld a,l");
    emit_instr("rst $10");    
}

void parse_puts(void) {
    parse_onearg(); // (expr)
    expect_semi();

    emit_rtl("puts");   
}

void parse_asm(void) {
    int asmcol = token_col;
    get_token(); // skip 'asm'
    expect(tokLBrace, errExpectLBrace);
    while (tok != tokRBrace && tok != tokEOS) {
        int last_token_line = token_line;
        if (token_col < asmcol) {
            emit_str("%s"); nl();
        } else {
            emit_strf("  %s ", token);
        }
        get_token();
        while (token_line == last_token_line && tok != tokRBrace && tok != tokEOS) {
            emit_str(token);
            get_token();
        }
        nl();
    }
    if (tok != tokRBrace) error(errExpectRBrace);
    get_token(); // skip '}'
}

void parse_type(TYPEREC *type) {
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

static void clean_stack(int16_t bytes) {
    if (!bytes) return;
    
    if (bytes < 8) {
        while ((bytes-2) >= 0) {
            emit_instr("pop bc");
            bytes -= 2;
        }
        while (bytes) {
            emit_instr("dec sp");
            bytes--;
        }
        return;
    } else  {
        emit_swap();
        emit_ld_immed(); emit_n16(bytes); nl();
        emit_instr("add hl,sp");
        emit_instr("ld sp,hl");        
        emit_swap();
    }
}

void parse_funccall(SYMBOL* sym) {
    get_token(); // skip '('
    uint8_t argcount = 0;
    while (tok != tokRParen) {
        parse_expr(0);
        ++argcount;
        emit_push();
        if (tok != tokComma) break;
        get_token(); // skip ','
    }
    bp_lastarg = argcount * 2;
    expect(tokRParen, errExpectRParen);
    emit_callsym(sym);
    clean_stack(bp_lastarg);
}

void parse_return(void) {
    get_token(); // skip 'return';
    if (tok != tokSemi) {
        parse_expr(0);
    }
    expect_semi();
    emit_jp(retlbl);
}

void parse_funcdecl(TYPEREC rettype, const char* name) {
    infunc = 1;
    get_token(); // skip '('

    TYPEREC argtype;
    char argName[MAX_IDENT_LEN + 1];

    uint16_t skiplbl = newlbl();
    emit_jp(skiplbl);

    emit_sname(name); nl();
    declglb(rettype, FUNCTION, name, 0);
    
    uint16_t oldretlbl = retlbl;
    retlbl = newlbl(); 
    uint16_t funcframe = push_frame();
    func_argcount = 0;
    while (tok != tokRParen) {
        parse_type(&argtype);
        if (is_array(&argtype)) make_ptr(&argtype);

        strncpy(argName, token, MAX_IDENT_LEN);
        bp_lastarg = 2 * func_argcount++;
        declloc(argtype, ARGUMENT, argName, bp_lastarg);
        get_token(); // skip arg name
        if (tok == tokComma) get_token(); // skip ','
    }
    expect(tokRParen, errExpectRParen);
    if (tok != tokLBrace) error(errExpectLBrace);
    
    maxlocalcount = 0;
    bp_lastlocal = 0; 
    localbytes = 0;
    locals_lbl = emit_alloclocals();
    parse_statement_block();
    emit_lbl(retlbl);
    clean_stack(localbytes);
    emit_ret();
    emit_lblequ16(locals_lbl, localbytes);
    emit_lbl(skiplbl);

    pop_frame(funcframe);
    
    infunc = 0;
    retlbl = oldretlbl; 
}

SYMBOL* declglb(TYPEREC type, SYM_CLASS klass, const char* name, int16_t value) {
    return addglb(name, klass, type, value);
}

SYMBOL* declloc(TYPEREC type, SYM_CLASS klass, const char* name, int16_t offset) {
    return addloc(name, klass, type, offset);
}

void compile(const char *filename, const char *asmfilename) {
    if (!src_open(filename)) {
        printf("can't open '%s'", filename);
        return;
    }

    if (!asm_open(asmfilename)) {
        src_close();
        printf("can't create '%s'", asmfilename);
        return;        
    }
   
    // quick and dirty remove extension
    char *dot = strrchr(asmfilename, '.');
    if (dot) *dot='\0';

#ifdef __ZXNEXT
    emit_strf("  output \"/dot/%s\"\r", asmfilename);
    emit_strf("  org $2000\r");
#else
    emit_strf("  output \"/dot/%s\"\n", asmfilename);
    emit_strf("  org $2000\n");
#endif
    parse();
    dump_rtl();
    dump_globals();
    dump_strings();

    src_close();
    asm_close();
}