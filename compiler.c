#include "znc.h"
#include "struct.h"

uint16_t retlbl = 0;        // function exit label

TOKEN tokMakeType = tokRaw; // type of make command

uint8_t infunc = 0;         // 1 if parsing a function
uint8_t func_rettype = 0;   // return type of current function (0 = void)
uint8_t inbank = 0;         // 1 if in explicit bank

uint8_t func_argcount;      // number of arguments for function being parsed
uint16_t locals_lbl;        // label of the EQU with the arg count
uint8_t localcount;         // number of live local variables
uint8_t maxlocalcount;      // highwater mark for local variables

uint16_t bp_lastlocal;      // base pointer of the last local
uint16_t localbytes;        // total bytes for locals
uint16_t exit_lbl;          // label for the global exit (return to BASIC for DOT commands)
uint16_t start_lbl;         // label for the start of the code
uint16_t stack_lbl;         // label for nex stack
uint16_t stack_size = 256;  // stack size

uint8_t hash_if_depth = 0; // depth of #if/#ifdef statements

SYMBOL declglb(uint8_t type_id, SYM_CLASS klass, const char* name, int16_t value);
SYMBOL declloc(uint8_t type_id, SYM_CLASS klass, const char* name, int16_t offset);
SYMBOL decl_in_scope(uint8_t type_id, SYM_CLASS klass, const char* name);

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
void parse_puts(void) MYCC;
void parse_out(void) MYCC;
void parse_nextreg(void) MYCC;
void parse_asm(int asmcol) MYCC;
void parse_org(void) MYCC;
void parse_bank(void) MYCC;
void parse_hashif(uint16_t brklbl, uint16_t contlbl) MYCC;
void parse_switch(uint16_t contlbl) MYCC;

/* Forward declaration for struct parser (defined later) */
void parse_struct_def(void) MYCC;
void parse_delegate_decl(void) MYCC;

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
    expr = parse_expr(0, 0);
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
    /* If the current token is an identifier that is a known struct type, treat this
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
    }

    switch (tok) {
        case tokConst:
        case tokVoid:
        case tokChar:
        case tokInt:
            parse_decl();
            break;
        case tokDelegate:
            parse_delegate_decl();
            break;
        case tokStruct:
            parse_struct_def();
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
            parse_expr(0, 0);
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
    uint8_t type_id;

    uint8_t constdecl = 0;
    if (tok == tokConst) {
        constdecl = 1;
        get_token(); // skip 'const'
    }

    parse_type(&type_id);
    if (constdecl) {
        if (type_is_void(type_id)) error(errSyntax);
        if (type_is_pointer(type_id)) error(errSyntax);
        if (type_is_array(type_id)) error(errSyntax);
        /* Make const version */
        TypeKind kind = type_get_kind(type_id);
        if (kind == TK_CHAR) type_id = type_make_char(1);
        else if (kind == TK_INT) type_id = type_make_int(1);
        else if (kind == TK_STRUCT) {
            uint8_t sid = type_get_struct_id(type_id);
            type_id = type_make_struct(sid, 1);
        }
    }

    if (tok != tokIdent) error(errSyntax);

    char name[MAX_IDENT_LEN + 1];
    strncpy(name, token, MAX_IDENT_LEN);
    
    get_token(); // skip name

    if (tok == tokLParen) {
        if (infunc || constdecl) error(errSyntax);
        parse_funcdecl(type_id, name);
    }
    else {
        SYMBOL sym;

        if (type_is_void(type_id) && !type_is_pointer(type_id)) error(errSyntax);

        for (;;) {
            sym = decl_in_scope(type_id, VARIABLE, name);
               
            if (tok == tokAssign) {
                parse_assign(0, sym, 0, type_id);
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

void parse_struct_def(void) MYCC {
    /* parse: struct Name { <field-decls> } ; */
    get_token(); // skip 'struct'
    if (tok != tokIdent) {
        error(errSyntax);
        return;
    }
    char sname[MAX_IDENT_LEN+1];
    strncpy(sname, token, MAX_IDENT_LEN);

    if (find_struct(sname) != -1) {
        error(errAlreadyDefined_s, sname);
        return;
    }

    int sid = add_struct(sname);

    get_token(); // skip name
    expect_LBrace();

    while (tok != tokRBrace && tok != tokEOS) {
        uint8_t ftype_id;
        parse_type(&ftype_id);

        for (;;) {
            if (tok != tokIdent) error(errSyntax);
            char fname[MAX_IDENT_LEN+1];
            strncpy(fname, token, MAX_IDENT_LEN);
            get_token(); // skip field name

            /* optional array suffix */
            if (tok == tokLBrack) {
                get_token(); // skip '['
                if (tok == tokRBrack) {
                    /* treat [] as pointer */
                    uint8_t elem_type = ftype_id;
                    ftype_id = type_make_pointer(elem_type, 1);
                } else {
                    EXPR_RESULT dim = parse_expr_delayconst(0, TYPE_ID_INT);
                    if (!type_is_const(dim.type_id)) error_expect_const();
                    if (dim.value > 0) {
                        uint8_t len = (dim.value > 255) ? 255 : (uint8_t)dim.value;
                        ftype_id = type_make_array(ftype_id, len);
                    } else {
                        ftype_id = type_make_pointer(ftype_id, 1);
                    }
                }
                expect(tokRBrack, errSyntax);
            }

            add_struct_field(sid, fname, ftype_id);

            if (tok == tokComma) {
                get_token(); // skip ',' and continue
                continue;
            }
            break;
        }
        expect_semi();
    }

    expect_RBrace();
    if (tok == tokSemi) get_token(); // optional semicolon
}

void parse_if(uint16_t brklbl, uint16_t contlbl) MYCC {
    uint16_t lblEndIf = NO_LABEL;
    uint16_t lblFalse = newlbl();
                
    parse_onearg(); // (expr)
    emit_jp_false(lblFalse);
    parse_statement(brklbl, contlbl);

    if (tok == tokElse) {
        get_token(); // skip 'else'
        lblEndIf = newlbl();
        emit_jp(lblEndIf);
    }
    emit_lbl(lblFalse);

    if (lblEndIf != NO_LABEL) {
        parse_statement(brklbl, contlbl);
        emit_lbl(lblEndIf);
        lblEndIf = NO_LABEL;
    }
    
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
            EXPR_RESULT expr_result = parse_expr_delayconst(0, TYPE_ID_INT);
            if (!type_is_const(expr_result.type_id)) {
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
            while (tok != tokEOS && tok != tokBreak && tok != tokCase && tok != tokRBrace && tok != tokDefault) {
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
		if (tok != tokSemi) parse_expr(0, 0);
		expect_semi();
	}

	// parse condition
	if (tok != tokSemi) {
		lblCond = newlbl();
		emit_lbl(lblCond);
		parse_expr(0, 0); 
		emit_jp_false(lblEndFor);
	}
	expect_semi();

	// parse post statement
	if (tok != tokRParen) {
		emit_jp(lblBody);
		lblPost = contlbl = newlbl();
		emit_lbl(lblPost);
		parse_expr(0, 0);
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
   
    parse_expr(0, 0);
    emit_copy_hl_to_bc();
    expect_comma();
    parse_expr(0, 0);
    emit_instrln("out (c),l");
    expect_RParen();
    expect_semi();
}

void parse_nextreg(void) MYCC {
    get_token(); // skip 'nextreg'
    expect_LParen();
    
    parse_expr(0, 0);
    emit_push();
    expect_comma();
    parse_expr(0, 0);
    emit_pop_de();

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

void parse_type(uint8_t *type_id_out) MYCC {
    /* Initialize to void */
    uint8_t base_type_id = TYPE_ID_VOID;
    uint8_t is_const_flag = 0;
    uint8_t struct_id = 0;

    switch (tok) {
        case tokVoid: 
            base_type_id = TYPE_ID_VOID;
            break;
        case tokChar: 
            base_type_id = TYPE_ID_CHAR;
            break;
        case tokInt: 
            base_type_id = TYPE_ID_INT;
            break;
        case tokIdent: {
            /* Ident could be a struct type name or a registered named type (delegate) */
            int sid = find_struct(token);
            if (sid >= 0) {
                struct_id = sid + 1; /* 1-based id */
                base_type_id = type_make_struct(struct_id, is_const_flag);
            } else {
                int t = type_find_by_name(token);
                if (t != -1) {
                    base_type_id = (uint8_t)t;
                } else {
                    error(errSyntax);
                    tok = tokInt;
                    base_type_id = TYPE_ID_INT;
                }
            }
            break;
        }
        default:
            error(errSyntax);
            tok = tokInt;
            base_type_id = TYPE_ID_INT;
            break;
    }

    get_token(); // skip base type or identifier

    /* Handle single pointer/array suffix */
    if (tok == tokStar) {
        get_token(); // skip '*'
        base_type_id = type_make_pointer(base_type_id, 1);
    } else if (tok == tokLBrack) {
        get_token(); // skip '['
        if (tok == tokRBrack) {
            base_type_id = type_make_pointer(base_type_id, 1);
        } else {
            EXPR_RESULT dim = parse_expr_delayconst(0, TYPE_ID_INT);
            if (!type_is_const(dim.type_id)) error_expect_const();
            if (dim.value > 0) {
                if (base_type_id == TYPE_ID_VOID) error(errSyntax);
                uint8_t length = (dim.value > 255) ? 255 : (uint8_t)dim.value;
                base_type_id = type_make_array(base_type_id, length);
            } else {
                base_type_id = type_make_pointer(base_type_id, 1); // zero means pointer
            }
        }
        expect(tokRBrack, errSyntax);
    }
    
    *type_id_out = base_type_id;
}

static void clean_stack(int16_t bytes) MYCC {
    if (!bytes) return;
    
    if (bytes < 8) {
        while ((bytes-2) >= 0) {
            emit_instrln("pop af");
            bytes -= 2;
        }
        while (bytes) {
            emit_instrln("dec sp");
            bytes--;
        }
        return;
    } else  {
        emit_swap();
        emit_ld_immed_n(bytes);        
        emit_instrln("add hl,sp");
        emit_instrln("ld sp,hl");        
        emit_swap();
    }
}

void parse_funccall(SYMBOL* sym, uint8_t ptr_in_hl) MYCC {
    get_token(); // skip '('
    uint8_t argcount = 0;
    uint8_t expected_count = 0xFF;
    uint8_t sig_id = 0xFF;
    if (is_func_or_proto(sym)) {
        expected_count = sym->offset;
        if (sym->signature_id != 0xFF) sig_id = sym->signature_id;
    } else if (sym->klass == VARIABLE) {
        /* If variable holds a function pointer (delegate), extract signature from its type */
        uint8_t t = sym->type_id;
        if (type_is_function(type_get_element_type_id(t)) && type_get_indirection(t) == 1) {
            uint8_t ftype = type_get_element_type_id(t);
            sig_id = type_get_function_sig(ftype);
            expected_count = signature_get_arg_count(sig_id);
        }
    }
    
    /* If pointer to call is already in HL, save it to a temp so it
     * survives argument evaluation. We create a temporary variable (2 bytes)
     * and store HL into it. We'll reload it after parsing args. */
    SYMBOL tmp_sym;
    uint8_t have_tmp = 0;
    if (ptr_in_hl) {
        char tmpname[MAX_IDENT_LEN + 1];
        snprintf(tmpname, sizeof(tmpname), "t%u", newlbl());
        tmp_sym = decl_in_scope(TYPE_ID_INT, VARIABLE, tmpname);
        
        /* Store HL (the pointer) into the temp variable */
        emit_store_sym(&tmp_sym);
        ptr_in_hl = 0; /* pointer now stored, will reload later */
        have_tmp = 1;
    }

    while (tok != tokRParen) {
        /* Check argument count inline */
        if (expected_count != 0xFF && argcount >= expected_count) {
            error(errArgMismatch);
            break;
        }
        
        /* Parse the argument expression with expected type if known */
        uint8_t expected_arg_type = (sig_id != 0xFF && argcount < MAX_FUNC_ARGS) 
            ? signature_get_arg_type(sig_id, argcount) : 0;
        EXPR_RESULT arg_result = parse_expr(0, expected_arg_type);
        
        /* Error if passing a struct by value - must use & to pass pointer */
        if (type_is_struct(arg_result.type_id) && !type_is_pointer(arg_result.type_id)) {
            error(errTypeError);
        }
        
        /* Check argument type inline if signature available */
        if (sig_id != 0xFF && argcount < MAX_FUNC_ARGS) {
            uint8_t expected_type = signature_get_arg_type(sig_id, argcount);
            uint8_t actual_type = arg_result.type_id;
            
            /* Use type compatibility checker */
            if (!type_check_compatible(expected_type, actual_type)) {
                error(errTypeError);
            }
        }
        
        emit_push();
        ++argcount;
        
        if (tok != tokComma) break;
        get_token(); // skip ','
    }
    
    /* Final argument count check */
    if (expected_count != 0xFF && argcount != expected_count) {
        error(errArgMismatch);
    }
    
    expect_RParen();

    /* If we saved the pointer, reload it now into HL */
    if (have_tmp) {
        emit_ld_symval(&tmp_sym);
        ptr_in_hl = 1;
    }

    emit_callsym(sym, ptr_in_hl);
    /* Cleanup stack: prefer expected_count if known, otherwise actual argcount */
    int cleanup_count = (expected_count != 0xFF) ? expected_count : argcount;
    clean_stack(cleanup_count * 2);
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
                } else {
                    emit_instrln("ld a,l");                    
                }
                emit_instrln("scf");
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
    EXPR_RESULT expr_result = { .type_id = TYPE_ID_VOID };

    get_token(); // skip 'return';

    if (type_is_void(func_rettype)) {
        expect_semi();
    } 
    else {
        if (tok == tokSemi) error(errReturnValueExpected);
        expr_result = parse_expr(0, func_rettype);        
        if (!type_check_compatible(func_rettype, expr_result.type_id)) {
            error(errTypeError);
        }        
        expect_semi();
    }    
    
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

void parse_funcdecl(uint8_t rettype_id, const char* name) MYCC {
    get_token(); // skip '('

    uint8_t argtype_id;
    char argName[MAX_IDENT_LEN + 1];
    uint8_t arg_types[MAX_FUNC_ARGS];  // Collect argument types

    SYMBOL symfunc = lookupIdent(name);
    uint8_t defined = 0;

    if (not_defined(&symfunc)) {
        symfunc = declglb(rettype_id, FUNCTION, name, 0);
    } else if (symfunc.klass != FUNCTION_PROTO) {
        defined = 1;
    }

    uint16_t oldretlbl = retlbl;
    retlbl = newlbl(); 
    uint16_t funcframe = push_frame();
    func_argcount = 0;
    
    /* Parse arguments and collect their types */
    while (tok != tokRParen) {
        parse_type(&argtype_id);
        if (type_is_array(argtype_id)) {
            /* Convert array to pointer */
            uint8_t elem_type = type_get_element_type(argtype_id);
            argtype_id = type_make_pointer(elem_type, 1);
        }
        
        /* ERROR: Struct parameters must be declared as pointers explicitly */
        if (type_is_struct(argtype_id) && !type_is_pointer(argtype_id)) {
            error(errTypeError);
        }
        
        /* Store argument type */
        if (func_argcount < MAX_FUNC_ARGS) {
            arg_types[func_argcount] = argtype_id;
        }
        
        strncpy(argName, token, MAX_IDENT_LEN);           
        declloc(argtype_id, ARGUMENT, argName, func_argcount++);
        get_token(); // skip arg name
        if (tok == tokComma) get_token(); // skip ',',,
    }
    expect_RParen();
   
    /* Check signature compatibility if already declared */
    if (defined || symfunc.klass == FUNCTION_PROTO) {
        if (func_argcount != symfunc.offset) {
            error(errDeclMismatch);
        } else if (symfunc.signature_id != 0xFF) {
            /* Verify argument types match */
            uint8_t match = 1;
            if (signature_get_arg_count(symfunc.signature_id) == func_argcount) {
                for (uint8_t i = 0; i < func_argcount; i++) {
                    if (signature_get_arg_type(symfunc.signature_id, i) != arg_types[i]) {
                        match = 0;
                        break;
                    }
                }
            } else {
                match = 0;
            }
            if (!match) {
                error(errDeclMismatch);
            }
        }
    }

    /* Store function signature if not already stored */
    if (symfunc.signature_id == 0xFF) {
        symfunc.signature_id = signature_create(rettype_id, func_argcount, arg_types);
        if (symfunc.signature_id == 0xFF) {
            error(errTooManySymbols);
        }
    }

    symfunc.offset = func_argcount;
    if (tok == tokSemi) {        
        if (!defined) symfunc.klass = FUNCTION_PROTO;
    } else {
        if (defined) error(errAlreadyDefined_s, name);

        symfunc.klass = FUNCTION;
        infunc = 1;
        func_rettype = rettype_id;
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
        func_rettype = TYPE_ID_VOID;
        retlbl = oldretlbl;
    }
    updatesym(&symfunc);
}

void parse_org(void) MYCC {
    get_token(); // skip 'org'
    EXPR_RESULT expr_result = parse_expr_delayconst(0, 0);
    if (!type_is_const(expr_result.type_id)) error_expect_const();
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

    EXPR_RESULT bankid_result = parse_expr_delayconst(0, TYPE_ID_INT);
    if (!type_is_const(bankid_result.type_id)) error_expect_const();
    if (bankid_result.value > 255) error(errInvalid_s, "bank");
    bankid = (uint8_t)bankid_result.value;
 
    if (tok == tokComma) {
        get_token(); // skip ','
        
        EXPR_RESULT offset_result = parse_expr_delayconst(0, TYPE_ID_INT);
        if (!type_is_const(offset_result.type_id)) error_expect_const();
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
        EXPR_RESULT expr_result = parse_expr_delayconst(0, 0);
        if (!type_is_const(expr_result.type_id)) error_expect_const();
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

SYMBOL declglb(uint8_t type_id, SYM_CLASS klass, const char* name, int16_t value) {
    SYMBOL lsym = addglb(name, klass, type_id, value);
    return lsym;
}

SYMBOL declloc(uint8_t type_id, SYM_CLASS klass, const char* name, int16_t offset) {
    SYMBOL lsym = addloc(name, klass, type_id, offset);
    return lsym;
}

SYMBOL decl_in_scope(uint8_t type_id, SYM_CLASS klass, const char* name) {
    SYMBOL sym;
    if (infunc || is_scoped()) {
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

void compile(const char *filename, const char *asmfilename) MYCC {
    /* Initialize type system */
    type_init();
    
    if (!asm_open(asmfilename)) {
        src_close();
        printf("can't create '%s'", asmfilename);
        return;        
    }
   
    // quick and dirty remove extension
    if (asmfilename != NULL) {
        char* dot = strrchr(asmfilename, '.');
        if (dot) *dot = '\0';
    }
    
    parse(filename, asmfilename, 1);
    
    dump_rtl();
    dump_globals();
    dump_strings();

    if (tokMakeType == tokNex) emit_nex(outfilename, start_lbl, stack_lbl, stack_size);

    asm_close();
}

void parse_delegate_decl(void) MYCC {
    /* parse: delegate <return-type> IDENT '(' param_list ')' ';' */
    get_token(); /* skip 'delegate' */
    uint8_t return_type;
    parse_type(&return_type);

    if (tok != tokIdent) {
        error(errSyntax);
        return;
    }
    char name[MAX_IDENT_LEN+1];
    strncpy(name, token, MAX_IDENT_LEN);
    get_token(); /* skip typename */

    expect_LParen();
    uint8_t arg_types[MAX_FUNC_ARGS];
    uint8_t argcount = 0;
    if (tok != tokRParen) {
        for (;;) {
            uint8_t atype;
            parse_type(&atype);
            if (argcount < MAX_FUNC_ARGS) arg_types[argcount++] = atype;
            else error(errTooManyTypes);

            if (tok != tokComma) break;
            get_token(); /* skip ',' */
        }
    }
    expect_RParen();
    expect_semi();

    /* Create signature and function-pointer type */
    uint8_t sig = signature_create(return_type, argcount, arg_types);
    uint8_t ftype = type_make_function(sig);
    uint8_t deleg_type = type_make_pointer(ftype, 1); /* indirection=1 */

    /* Register the type name */
    if (type_find_by_name(name) != -1) {
        error(errAlreadyDefined_s, name);
        return;
    }
    type_register_name(name, deleg_type);
}