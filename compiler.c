#include "znc.h"
#include "struct.h"
#include "shared.h"

uint16_t retlbl = 0;        // function exit label

TOKEN tokMakeType = tokRaw; // type of make command

uint8_t infunc = 0;         // 1 if parsing a function
uint8_t func_rettype = 0;   // return type of current function (0 = void)
uint8_t inbank = 0;         // 1 if in explicit bank

uint8_t func_argcount;      // number of arguments for function being parsed
uint8_t func_is_variadic = 0; // 1 if current function is variadic
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
void parse_out(void) MYCC;
void parse_nextreg(void) MYCC;
void parse_asm(void) MYCC;
void parse_org(void) MYCC;
void parse_bank(void) MYCC;
void parse_hashif(uint16_t brklbl, uint16_t contlbl) MYCC;
void parse_switch(uint16_t contlbl) MYCC;
void parse_vastart(void) MYCC;
void parse_vaend(void) MYCC;

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
        if (tokMakeType == tokNex) {
            start_lbl = newlbl();
            stack_lbl = newlbl();
            emit_lbl(start_lbl);
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
            error(errExpected_s, "nex/dot/raw");
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

/* Consume one statement from the token stream without emitting any code.
   Handles a balanced { } block, compound control-flow statements, or a
   single ;-terminated statement.  Recursive for nested if/else/while/for. */
static void skip_statement(void) MYCC {
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

static void parse_statement_block(uint16_t brklbl, uint16_t contlbl) MYCC {
    get_token(); // skip '{'
    uint16_t blockframe = push_frame();
    uint8_t old_localcount = localcount;
    uint16_t old_bp = bp_lastlocal;
    while (tok != tokEOS && tok != tokRBrace) {
        uint8_t was_exit = (tok == tokReturn || tok == tokBreak || tok == tokContinue);
        parse_statement(brklbl, contlbl);
        if (was_exit) {
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
        case tokVaStart: parse_vastart(); break;
        case tokVaEnd: parse_vaend(); break;
        case tokOut: parse_out(); break;
        case tokNextReg: parse_nextreg(); break;
        case tokAsm: parse_asm(); break;
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
    get_token(); // skip 'include'
    if (tok != tokString) error(errExpected_s, "filename");
    
    parse(token, NULL, 0);

    get_token(); // skip filename    
}

/* Helper: Convert type to const version, checking for validity */
static uint8_t make_const_type(uint8_t type_id) MYCC {
    if (type_is_void(type_id)) error(errTypeError);
    if (type_is_pointer(type_id)) error(errTypeError);
    if (type_is_array(type_id)) error(errTypeError);
    
    TypeKind kind = type_get_kind(type_id);
    if (kind == TK_CHAR) return type_make_char(1);
    else if (kind == TK_INT) return type_make_int(1);
    else if (kind == TK_FIXED) return type_make_fixed(1);
    else if (kind == TK_STRUCT) {
        uint8_t sid = type_get_struct_id(type_id);
        return type_make_struct(sid, 1);
    }
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

    char name[MAX_IDENT_LEN + 1];
    strncpy(name, token, MAX_IDENT_LEN);

    get_token(); // skip name

    if (tok == tokLParen) {
        if (infunc || constdecl) error(errTopLevelOnly);
        parse_funcdecl(type_id, name);
    }
    else {
        SYMBOL sym;

        if (type_is_void(type_id) && !type_is_pointer(type_id)) error(errTypeError);

        for (;;) {
            sym = decl_in_scope(type_id, VARIABLE, name);
               
            if (tok == tokAssign) {
                parse_assign(0, sym, 0, type_id);
            }
            else if (constdecl) error(errExpected_s, "initializer");

            if (tok != tokComma) break;
            get_token(); // skip ','
            strncpy(name, token, MAX_IDENT_LEN);
            get_token(); // skip name
        }
        expect_semi();
    }
}

void parse_struct_def(void) MYCC {
    static char name[MAX_IDENT_LEN+1];

    /* parse: struct Name { <field-decls> } ; */
    get_token(); // skip 'struct'
    if (tok != tokIdent) {
        error(errExpected_s, "identifier");
        return;
    }
    
    if (find_struct(token) != -1) {
        error(errAlreadyDefined_s, token);
        return;
    }

    int sid = add_struct(token);

    get_token(); // skip name
    expect_LBrace();

    while (tok != tokRBrace && tok != tokEOS) {
        uint8_t ftype_id;
        parse_type(&ftype_id);

        for (;;) {
            if (tok != tokIdent) error(errExpected_s, "field name");

            strncpy(name, token, MAX_IDENT_LEN);
            get_token(); // skip field name

            add_struct_field(sid, name, ftype_id);

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
    get_token(); // skip 'if'
    expect_LParen();
    EXPR_RESULT cond = parse_expr_delayconst(0, 0);
    expect_RParen();

    if (type_is_const(cond.type_id)) {
        if (cond.value) {
            parse_statement(brklbl, contlbl);           // always-true: emit true branch
            if (tok == tokElse) { get_token(); skip_statement(); } // skip else
        } else {
            skip_statement();                           // always-false: skip true branch
            if (tok == tokElse) { get_token(); parse_statement(brklbl, contlbl); } // emit else
        }
        return;
    }

    uint16_t lblEndIf = NO_LABEL;
    uint16_t lblFalse = newlbl();
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
    
    uint16_t mark = arena_get_marker();

    uint16_t* values = arena_alloc(sizeof(uint16_t) * MAX_CASE); // space for case values
    uint16_t* labels = arena_alloc(sizeof(uint16_t) * MAX_CASE); // space for case labels
    
    uint8_t case_count = 0;
    uint8_t last_break = 0;

    EXPR_RESULT sw_expr = parse_onearg(); // (expr)
    if (type_is_fixed(sw_expr.type_id))
        error(errTypeError);
    emit_jp(lblTbl);

    expect_LBrace();
    while (tok == tokCase || tok == tokDefault) {
        last_break = 0;
        if (tok == tokCase) {
            get_token(); // skip 'case'
            EXPR_RESULT expr_result = parse_expr_delayconst(0, TYPE_ID_INT);
            if (!type_is_const(expr_result.type_id)) {
                error(errConstExpected);
            }
            if (type_is_fixed(expr_result.type_id))
                error(errTypeError);
            uint16_t lblCase = newlbl();
            if (case_count == MAX_CASE) error(errInvalid_s, "case");
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
    arena_free_to_marker(mark);
}

void parse_while(void) MYCC {
    get_token(); // skip 'while'
    expect_LParen();

    uint16_t lblCond = newlbl();
    uint16_t lblEndWhile = newlbl();
    emit_lbl(lblCond); // label must precede condition code in output

    EXPR_RESULT cond = parse_expr_delayconst(0, 0);
    expect_RParen();

    if (type_is_const(cond.type_id) && !cond.value) {
        skip_statement(); // while(0) { ... } — skip body, emit nothing
        return;           // lblCond emitted but unreferenced — harmless
    }

    if (!type_is_const(cond.type_id))
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
    uint16_t old_bp = bp_lastlocal;

	uint16_t lblCond=NO_LABEL;
	uint16_t lblEndFor, brklbl;
	uint16_t lblBody = newlbl();
	uint16_t lblPost, contlbl;

	lblEndFor = brklbl = newlbl();
	lblPost = contlbl = NO_LABEL;


	// parse initializer
	uint8_t init_is_decl = 0;
	if (tok == tokConst || tok == tokChar || tok == tokInt || tok == tokFixed || tok == tokVoid) {
		init_is_decl = 1;
	} else if (tok == tokIdent) {
		if (find_struct(token) >= 0 || type_find_by_name(token) != -1)
			init_is_decl = 1;
	}
	if (init_is_decl) {
		parse_decl();
	} else {
		if (tok != tokSemi) parse_expr(0, 0);
		expect_semi();
	}

	// parse condition
	if (tok != tokSemi) {
		lblCond = newlbl();
		emit_lbl(lblCond);
		EXPR_RESULT cond = parse_expr_delayconst(0, 0);
		if (type_is_const(cond.type_id) && !cond.value) {
			// for(; 0; ...) — skip post-expr and body, emit nothing
			expect_semi();
			// skip post-expression, respecting nested parens
			int pdepth = 0;
			while (tok != tokEOS) {
				if (tok == tokLParen || tok == tokLBrack) ++pdepth;
				else if (tok == tokRParen || tok == tokRBrack) {
					if (pdepth == 0) break; // this is the for's closing ')'
					--pdepth;
				}
				get_token();
			}
			expect_RParen();
			skip_statement(); // skip body
			if (maxlocalcount < localcount) maxlocalcount = localcount;
			bp_lastlocal = old_bp;
			localcount = old_localcount;
			pop_frame(blockframe);
			return;
		}
		if (!type_is_const(cond.type_id))
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
    bp_lastlocal = old_bp;
	localcount = old_localcount;
	pop_frame(blockframe);    
}

void parse_break(uint16_t brklbl) MYCC {
    get_token(); // skip 'break'
    if (brklbl == NO_LABEL) error(errBreakOutsideLoop);
    expect_semi();    
    emit_jp(brklbl);
}

void parse_continue(uint16_t contlbl) MYCC {
    get_token(); // skip 'continue'
    if (contlbl == NO_LABEL) error(errContinueOutsideLoop);
    expect_semi();    
    emit_jp(contlbl);
}

void parse_putc(void) MYCC {
    parse_onearg(); // (expr)
    expect_semi();

    emit_rtl("putc");
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

void parse_asm(void) MYCC {
    get_token(); // skip '__asm__', tok is now tokLBrace
    if (tok != tokLBrace) { error(errExpected_c, '{'); return; }
    // Do NOT call get_token() here - far_parse_asm reads the body raw
    // starting from code which already points past '{'
    enter_asm_block();
    parse_asm_block();
    exit_asm_block();
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
        case tokFixed:
            base_type_id = TYPE_ID_FIXED;
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
                    error(errNotDefined_s, token);
                    tok = tokInt;
                    base_type_id = TYPE_ID_INT;
                }
            }
            break;
        }
        default:
            error(errNotDefined_s, token);
            tok = tokInt;
            base_type_id = TYPE_ID_INT;
            break;
    }

    get_token();

    /* Handle multiple levels of pointer/array suffixes */
    while (tok == tokStar || tok == tokLBrack) {
        if (tok == tokStar) {
            get_token(); // skip '*'
            base_type_id = type_make_pointer(base_type_id, 1);
        } else if (tok == tokLBrack) {
            get_token(); // skip '['
            if (tok == tokRBrack) {
                /* Empty brackets [] means pointer */
                base_type_id = type_make_pointer(base_type_id, 1);
            } else {
                EXPR_RESULT dim = parse_expr_delayconst(0, TYPE_ID_INT);
                if (!type_is_const(dim.type_id)) error(errConstExpected);
                /* Convert const fixed dimension to int */
                if (type_is_fixed(dim.type_id))
                    dim.value = (uint16_t)((int16_t)dim.value >> 4);
                if (dim.value > 0) {
                    if (base_type_id == TYPE_ID_VOID) error(errTypeError);
                    base_type_id = type_make_array(base_type_id, dim.value);
                } else {
                    base_type_id = type_make_pointer(base_type_id, 1); // zero means pointer
                }
            }
            expect(tokRBrack, ']');
        }
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

void parse_funccall(SYMBOL* sym, PTR_LOCATION ptr_loc) MYCC {
    get_token(); // skip '('
    uint8_t argcount = 0;
    uint8_t expected_count = 0xFF;
    uint8_t sig_id = 0xFF;
    uint8_t is_variadic = 0;
    
    if (is_func_or_proto(sym)) {
        expected_count = (uint8_t)sym->fn.arg_count;
        if (sym->fn.signature_id != 0xFF) sig_id = sym->fn.signature_id;
    } else if (sym->klass == VARIABLE) {
        /* If variable holds a function pointer (delegate), extract signature from its type */
        uint8_t t = sym->type_id;
        if (type_is_function(type_get_element_type_id(t)) && type_get_indirection(t) == 1) {
            uint8_t ftype = type_get_element_type_id(t);
            sig_id = type_get_function_sig(ftype);
            expected_count = signature_get_arg_count(sig_id);
        }
    }
    
    /* Check if function is variadic */
    if (sig_id != 0xFF) {
        is_variadic = signature_is_variadic(sig_id);
    }
    
    /* If pointer to call is already in HL, save it to a temp so it
     * survives argument evaluation. We create a temporary variable (2 bytes)
     * and store HL into it. We'll reload it after parsing args. */
    SYMBOL tmp_sym;
    uint8_t have_tmp = 0;
    if (ptr_loc == PTR_IN_HL) {
        ARENA_MARKER marker = arena_get_marker();
        char *tmpname = arena_alloc(8);
        snprintf(tmpname, 8, "t%d", newlbl());
        tmp_sym = decl_in_scope(TYPE_ID_INT, VARIABLE, tmpname);
        
        /* Store HL (the pointer) into the temp variable */
        emit_store_sym(&tmp_sym);
        ptr_loc = PTR_IN_SYMBOL; /* pointer now stored, will reload later */
        have_tmp = 1;
        arena_free_to_marker(marker);
    }

    while (tok != tokRParen) {
        /* For variadic functions, allow more args than expected_count
         * For non-variadic, check argument count inline */
        if (!is_variadic && expected_count != 0xFF && argcount >= expected_count) {
            error(errArgMismatch);
            break;
        }
        
        /* Parse the argument expression without an expected type so we get the
         * actual declared/static type of the expression. The parsed
         * `arg_result` contains a copy of the symbol when the expression is a
         * simple identifier (arg_result.has_sym) which we use for compatibility
         * checks below.
         */
        EXPR_RESULT arg_result;
        if (tok == tokLBrace) {
            /* Brace-initializer literal passed as argument.
             * Emit it as inline static data and pass its address in HL. */
            uint8_t elem_type_id = TYPE_ID_CHAR; /* default: byte */
            uint16_t expected_arr_len = 0; /* 0 = no fixed-size constraint */
            if (sig_id != 0xFF && argcount < expected_count) {
                uint8_t expected_type = signature_get_arg_type(sig_id, argcount);
                if (type_is_array(expected_type) || type_is_pointer(expected_type))
                    elem_type_id = type_get_element_type_id(expected_type);
                if (type_is_array(expected_type))
                    expected_arr_len = type_get_array_length(expected_type);
            }

            get_token(); /* skip '{' */

            uint16_t skiplbl = newlbl();
            uint16_t datalbl = newlbl();

            /* Load address of the inline data into HL */
            emit_ld_immed(); emit_lblref(datalbl); emit_nl();

            /* Jump over the data so the CPU does not execute it */
            emit_jp(skiplbl);
            emit_lbl(datalbl);
            emit_ch(' ');

            uint16_t actual_count = parse_brace_initializer_elements(elem_type_id);
            expect(tokRBrace, '}');

            /* Validate element count against a fixed-size array parameter */
            if (expected_arr_len > 0 && actual_count != expected_arr_len)
                error(errArgMismatch);

            emit_lbl(skiplbl);

            arg_result.type_id = type_make_pointer(elem_type_id, 1);
            arg_result.has_sym = 0;
            arg_result.value   = 0;
        } else {
            arg_result = parse_expr(0, 0);
        }
        
        /* Error if passing a struct by value - must use & to pass pointer */
        if (type_is_struct(arg_result.type_id) && !type_is_pointer(arg_result.type_id)) {
            error(errTypeError);
        }
        
        /* Check argument type inline if signature available and within fixed params */
        if (sig_id != 0xFF && argcount < expected_count && argcount < MAX_FUNC_ARGS) {
            uint8_t expected_type = signature_get_arg_type(sig_id, argcount);
            /* Prefer the original symbol type if this argument started as a simple
             * identifier (peeked earlier). This ensures we preserve array types
             * declared on the identifier even if parse_expr loads or decays them.
             */
            /* Use the expression's resolved type. If the expression is a simple
             * identifier, prefer the symbol's declared type (arg_result.has_sym).
             * Do NOT use the peeked identifier's type blindly: for member
             * accesses like `obj.member` the token stream starts with an
             * identifier but the overall expression's type is the member's
             * type, which is already reflected in arg_result.type_id.
             */
            uint8_t actual_type = arg_result.type_id;
            if (arg_result.has_sym) actual_type = arg_result.sym.type_id;

            /* Use the central compatibility checker (from=actual, to=expected). */
            if (!type_check_compatible(actual_type, expected_type)) {
                error(errTypeError);
            }

            /* Emit fixed-point conversion if needed between compatible scalar types */
            if (type_is_fixed(expected_type) && !type_is_fixed(actual_type) &&
                (type_is_int(actual_type) || type_is_char(actual_type) ||
                 type_is_const(actual_type))) {
                /* int/char -> fixed: shift left 4 */
                emit_int_to_fixed();
            } else if (!type_is_fixed(expected_type) && type_is_fixed(actual_type) &&
                       (type_is_int(expected_type) || type_is_char(expected_type))) {
                /* fixed -> int/char: shift right 4 (arithmetic) */
                emit_fixed_to_int();
            }
        }
        
        emit_push();
        ++argcount;
        
        if (tok != tokComma) break;
        get_token(); // skip ','
    }
    
    /* Calculate variadic argument count */
    uint8_t variadic_count = 0;
    if (is_variadic && expected_count != 0xFF) {
        if (argcount < expected_count) {
            error(errArgMismatch);
        }
        variadic_count = argcount - expected_count;
    }
    
    /* For variadic functions, push the count of variadic args */
    if (is_variadic) {
        emit_ld_immed_n(variadic_count);
        emit_push();
    }
    
    /* Final argument count check for non-variadic functions */
    if (!is_variadic && expected_count != 0xFF && argcount != expected_count) {
        error(errArgMismatch);
    }
    
    expect_RParen();

    /* If we saved the pointer, reload it now into HL */
    if (have_tmp) {
        emit_ld_symval(&tmp_sym);
        ptr_loc = PTR_IN_HL;
    }

    emit_callsym(sym, ptr_loc);
    /* Cleanup stack: for variadic functions, also account for pushed count */
    int cleanup_count = (expected_count != 0xFF) ? expected_count : argcount;
    if (is_variadic) {
        cleanup_count = argcount + 1;  /* +1 for the count itself */
    }
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

    if (infunc) {
        if (type_is_void(func_rettype)) {
            if (tok != tokSemi) error(errReturnValueUnexpected);
        }
        else {
            if (tok == tokSemi) error(errReturnValueExpected);
            expr_result = parse_expr(0, func_rettype);
            if (!type_check_compatible(expr_result.type_id, func_rettype)) {
                error(errTypeError);
            }
            expect_semi();
        }

        emit_jp(retlbl);
    }
    else {
        if (tok != tokSemi) {
            expr_result = parse_expr(0, 0);
        }
        else {
            expr_result.type_id = TYPE_ID_INT;
            expr_result.value = 0;
        }
        do_exit(expr_result);
    }
}

void parse_exit(void) MYCC {
    EXPR_RESULT expr_result = parse_onearg();
    do_exit(expr_result);
}

void parse_vastart(void) MYCC {
    get_token(); // skip 'va_start'
    expect_LParen();
    SYMBOL valist = lookupIdent(token);
    if (valist.klass != VARIABLE) {
        error(errTypeError);
    }
    get_token(); // skip valist
    if (tok == tokComma) {
        get_token(); // skip ','
        SYMBOL last_fixed = lookupIdent(token);
        if (last_fixed.klass != ARGUMENT) {
            error(errTypeError);
        }
        get_token(); // skip last_fixed
        emit_ld_symaddr(&last_fixed);
    }
    else {
        emit_rtl("ccvafirst");
        emit_instrln("inc de");
        emit_instrln("inc de"); // Point to just before the first variadic argument
    }
    expect_RParen();
    expect_semi(); 
    emit_store_sym(&valist);
}

void parse_vaend(void) MYCC {
    get_token(); // skip 'va_end'
    expect_LParen();
    SYMBOL valist = lookupIdent(token);
    if (valist.klass != VARIABLE) {
        error(errTypeError);
    }
    get_token(); // skip valist
    expect_RParen();
    expect_semi();
}

void parse_funcdecl(uint8_t rettype_id, const char* name) MYCC {
    /* ERROR: Struct return types must be declared as pointers explicitly */
    if (type_is_struct(rettype_id) && !type_is_pointer(rettype_id)) {
        error(errTypeError);
    }

    get_token(); // skip '('

    uint8_t argtype_id;
    static uint8_t arg_types[MAX_FUNC_ARGS];  // Collect argument types

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
    uint8_t is_variadic = 0;
    
    /* Parse arguments and collect their types */
    while (tok != tokRParen && tok != tokEllipsis) {
        parse_type(&argtype_id);
        /* Preserve parsed type for the function signature; convert arrays to pointers
         * only for the local argument variable storage.
         */
        uint8_t sig_arg_type = argtype_id;
        if (type_is_array(argtype_id)) {
            /* Convert array to pointer for the declared/local argument */
            uint8_t elem_type = type_get_element_type(argtype_id);
            argtype_id = type_make_pointer(elem_type, 1);
        }

        /* ERROR: Struct parameters must be declared as pointers explicitly */
        if (type_is_struct(argtype_id) && !type_is_pointer(argtype_id)) {
            error(errTypeError);
        }

        /* Store argument type for signature (preserve arrays as arrays) */
        if (func_argcount < MAX_FUNC_ARGS) {
            arg_types[func_argcount] = sig_arg_type;
        }
        
        declloc(argtype_id, ARGUMENT, token, func_argcount++);
        get_token(); // skip arg name
        if (tok == tokComma) get_token(); // skip ','
    }
    
    /* Check for variadic function (...)  */
    if (tok == tokEllipsis) {
        is_variadic = 1;
        get_token(); // skip '...'
    }
    
    expect_RParen();
   
    /* Check signature compatibility if already declared */
    if (defined || symfunc.klass == FUNCTION_PROTO) {
        if (func_argcount != symfunc.fn.arg_count) {
            error(errDeclMismatch);
        } else if (symfunc.fn.signature_id != 0xFF) {
            /* Verify argument types and variadic flag match */
            uint8_t match = 1;
            if (signature_get_arg_count(symfunc.fn.signature_id) == func_argcount &&
                signature_is_variadic(symfunc.fn.signature_id) == is_variadic) {
                for (uint8_t i = 0; i < func_argcount; i++) {
                    if (signature_get_arg_type(symfunc.fn.signature_id, i) != arg_types[i]) {
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
    if (symfunc.fn.signature_id == 0xFF) {
        if (is_variadic) {
            symfunc.fn.signature_id = signature_create_variadic(rettype_id, func_argcount, arg_types);
        } else {
            symfunc.fn.signature_id = signature_create(rettype_id, func_argcount, arg_types);
        }
        if (symfunc.fn.signature_id == 0xFF) {
            error(errTooManyTypes);
        }
    }

    symfunc.fn.arg_count = func_argcount;
    if (tok == tokSemi) {        
        if (!defined) symfunc.klass = FUNCTION_PROTO;
    } else {
        if (defined) error(errAlreadyDefined_s, name);

        symfunc.klass = FUNCTION;
        updatesym(&symfunc); // update before parsing body so recursive calls see correct arg_count
        infunc = 1;
        func_rettype = rettype_id;
        func_is_variadic = is_variadic;
        uint16_t skiplbl = newlbl();
        emit_jp(skiplbl);

        emit_sname(name); emit_nl();

        if (tok == tokAsm) {
            parse_asm();
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
        func_is_variadic = 0;
        retlbl = oldretlbl;
    }
    updatesym(&symfunc);
}

void parse_org(void) MYCC {
    get_token(); // skip 'org'
    EXPR_RESULT expr_result = parse_expr_delayconst(0, 0);
    if (!type_is_const(expr_result.type_id)) error(errConstExpected);
    emit_org(expr_result.value);
    expect_semi();
}

void parse_bank(void) MYCC {
    if (inbank) error(errTopLevelOnly);

    inbank = 1;
    get_token(); // skip bank
    uint8_t bankid;
    uint16_t offset = 0;
    expect_LParen();

    EXPR_RESULT bankid_result = parse_expr_delayconst(0, TYPE_ID_INT);
    if (!type_is_const(bankid_result.type_id)) error(errConstExpected);
    if (bankid_result.value > 255) error(errInvalid_s, "bank");
    bankid = (uint8_t)bankid_result.value;
 
    if (tok == tokComma) {
        get_token(); // skip ','
        
        EXPR_RESULT offset_result = parse_expr_delayconst(0, TYPE_ID_INT);
        if (!type_is_const(offset_result.type_id)) error(errConstExpected);
        offset = offset_result.value;
    }
    expect_RParen();
    emit_bank(bankid, offset);
    uint16_t bank_gbl_start = get_lastgbl();
    size_t bank_str_start = get_laststr();
    char bank_str_lbl[16];
    snprintf(bank_str_lbl, sizeof(bank_str_lbl), "str_b%d", bankid);
    set_strref_ctx(bank_str_lbl, (uint16_t)bank_str_start);
    set_str_search_base(bank_str_start);
    parse_statement_block(NO_LABEL, NO_LABEL);
    uint16_t bank_gbl_end = get_lastgbl();
    size_t bank_str_end = get_laststr();
    dump_globals_range(bank_gbl_start, bank_gbl_end);
    dump_strings_range(bank_str_lbl, bank_str_start, bank_str_end);
    reset_lastgbl(bank_gbl_start);
    reset_laststr(bank_str_start);
    set_str_search_base(0);
    set_strref_ctx("str", 0);
    inbank = 0;
}

void parse_conditional(uint8_t active, uint16_t brklbl, uint16_t contlbl) MYCC {
    uint8_t current_if_depth = hash_if_depth;
    if (active) {
        while (tok != tokHashElif && tok != tokHashElse && tok != tokHashEndif && tok != tokEOS) {
            parse_statement(brklbl, contlbl);
        }
    } else {
        while ((current_if_depth != hash_if_depth || (tok != tokHashElif && tok != tokHashElse && tok != tokHashEndif)) && tok != tokEOS) {
            if (tok == tokHashIf || tok == tokHashIfDef || tok == tokHashIfNDef) {
                ++hash_if_depth;
            } else if (tok == tokHashEndif) {
                if (--hash_if_depth == 0) error(errUnexpectedEndif);
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
        if (!type_is_const(expr_result.type_id)) error(errConstExpected);
        active = expr_result.value != 0;
    }

    uint8_t branch_taken = active;
    ++hash_if_depth;
    parse_conditional(active, brklbl, contlbl);

    // handle zero or more #elif branches
    while (tok == tokHashElif) {
        get_token(); // skip '#elif'
        EXPR_RESULT elif_expr = parse_expr_delayconst(0, 0);
        if (!type_is_const(elif_expr.type_id)) error(errConstExpected);
        active = !branch_taken && (elif_expr.value != 0);
        if (active) branch_taken = 1;
        parse_conditional(active, brklbl, contlbl);
    }

    if (tok == tokHashElse) {
        get_token(); // skip '#else'
        parse_conditional(!branch_taken, brklbl, contlbl);
    }

    if (tok != tokHashEndif) error(errExpected_s, "#endif");
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
    if ((infunc || is_scoped()) && (!inbank || infunc)) {
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

    /* ERROR: Struct return types must be declared as pointers explicitly */
    if (type_is_struct(return_type) && !type_is_pointer(return_type)) {
        error(errTypeError);
    }

    if (tok != tokIdent) {
        error(errExpected_s, "identifier");
        return;
    }
    static char name[MAX_IDENT_LEN+1];
    strncpy(name, token, MAX_IDENT_LEN);
    get_token(); /* skip typename */

    expect_LParen();
    static uint8_t arg_types[MAX_FUNC_ARGS];
    uint8_t argcount = 0;
    if (tok != tokRParen) {
        for (;;) {
            uint8_t atype;
            parse_type(&atype);
            if (argcount < MAX_FUNC_ARGS) arg_types[argcount++] = atype;
            else error(errTooManyTypes);

            if (tok == tokIdent) {
                /* Optional argument name - skip it */
                get_token();
            }
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