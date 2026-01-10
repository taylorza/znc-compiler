#include "znc.h"
#include "struct.h"
#include "shared.h"

#define ASSIGN_PREC 1
#define OR_PREC 5
#define AND_PREC 10
#define BIT_OR_PREC 15
#define BIT_XOR_PREC 20
#define BIT_AND_PREC 25
#define EQ_PREC 30
#define REL_OP_PREC 35
#define SHIFT_OP_PREC 40
#define ADD_PREC 45
#define MUL_PREC 50
#define COND_PREC 3

typedef struct OP_PREC {
    TOKEN op;
    uint8_t prec;
} OP_PREC;

OP_PREC prectbl[] = {
    {tokCond,   COND_PREC},

    //{tokAssign, ASSIGN_PREC},
    
    {tokOr,     OR_PREC},
    {tokAnd,    AND_PREC},
    
    {tokBitOr,  BIT_OR_PREC},
    {tokBitXor, BIT_XOR_PREC},
    {tokBitAnd, BIT_AND_PREC},

    {tokEq,     EQ_PREC},
    {tokNeq,    EQ_PREC},

    {tokLt,     REL_OP_PREC},
    {tokLeq,    REL_OP_PREC},
    {tokGt,     REL_OP_PREC},
    {tokGeq,    REL_OP_PREC},

    {tokShl,    SHIFT_OP_PREC},
    {tokShr,    SHIFT_OP_PREC},

    {tokPlus,   ADD_PREC},
    {tokMinus,  ADD_PREC},

    {tokStar,   MUL_PREC},
    {tokDiv,    MUL_PREC},
    {tokMod,    MUL_PREC},
};

int8_t prec(TOKEN op) MYCC {
    for(uint8_t i=0; i < sizeof(prectbl)/sizeof(OP_PREC); ++i) {
        if (prectbl[i].op == op) {
            return prectbl[i].prec;
        }
    }
    return 0;
}

EXPR_RESULT parse_ternary(EXPR_RESULT expr_result, uint8_t prec) MYCC;
EXPR_RESULT parse_factor(uint8_t dereference) MYCC;
EXPR_RESULT parse_binop(TOKEN op, EXPR_RESULT ltyp, uint8_t prec) MYCC;
EXPR_RESULT far_parse_expr(uint8_t minprec) MYCC;
EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec) MYCC;
void far_parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC;

EXPR_RESULT far_parse_expr(uint8_t minprec) MYCC {
    EXPR_RESULT expr_result = far_parse_expr_delayconst(minprec);
    
    if (is_const(&expr_result.type)) {
        emit_ld_const(expr_result.value);
    }

    return expr_result;
}

EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec) MYCC {
    EXPR_RESULT expr_result = parse_factor(0);

    uint8_t p;
    while ((p = prec(tok)) && p && p >= minprec) {
        TOKEN op = tok;
        get_token(); // skip op

        if (op == tokCond) {
            expr_result = parse_ternary(expr_result, p);
        }
        else {
            expr_result = parse_binop(op, expr_result, p);
        }
    }
    return expr_result;
}

EXPR_RESULT parse_ternary(EXPR_RESULT expr_result, uint8_t prec) MYCC {
    uint16_t altlbl = newlbl();
    uint16_t donelbl = newlbl();

    if (is_const(&expr_result.type)) {
        emit_ld_const(expr_result.value);
    }
    emit_jp_false(altlbl);
    EXPR_RESULT ptyp = far_parse_expr(0);  // primary expression
    emit_jp(donelbl);
    expect(tokColon, ':');
    emit_lbl(altlbl);
    EXPR_RESULT atyp = far_parse_expr(prec); // alternate expresion
    emit_lbl(donelbl);

    if (base_type(&ptyp.type) != base_type(&atyp.type)) error(errTypeError);
    
    expr_result.type.basetype = base_type(&ptyp.type);

    return expr_result;
}

EXPR_RESULT parse_indexer(const TYPEREC *elemtype) {
    get_token();    // skip '['
    EXPR_RESULT index_result = far_parse_expr_delayconst(0);   // index expression
    expect(tokRBrack, ']'); // skip ']'

    /* Scale index by element size in bytes (handles scalars, pointers, and structs) */
    uint16_t scale = (uint16_t)type_size(elemtype);

    if (is_const(&index_result.type)) {
        /* Constant index: multiply at compile time so callers can fold immediates */
        index_result.value = (uint16_t)(index_result.value * scale);
    }
    else {
        /* Non-constant index: scale HL in-place. Optimize common cases. */
        if (scale == 1) {
            /* no scaling required */
        }
        else if (scale == 2) {
            emit_mul2();
        }
        else {
            /* If scale is power of two, apply repeated doubling */
            uint16_t s = scale;
            if ((s & (s - 1)) == 0) {
                while (s > 1) { emit_mul2(); s >>= 1; }
            } else {
                /* Use runtime RTL multiply HL = HL * DE to handle arbitrary scales. */
                emit_ldde_immed_n(scale);
                emit_rtl("ccmult");
            }
        }
        /* Non-constant index left in HL (now scaled) */
    }
    return index_result;
}

/* Helper: apply indexing to a symbol's base address.
 * Computes the address of the element (base+offset + index) into HL.
 * - sym: symbol for base
 * - offset: byte offset of the field within the struct (0 for plain arrays)
 * - ftype: the type of the field (array or element type)
 * On return HL points to the element and the caller should call
 * make_scalar(out_type) to set the element type.
 */
static void emit_indexed_sym_address(SYMBOL *sym, uint16_t offset, const TYPEREC *ftype, TYPEREC *out_type, uint8_t base_is_pointer) {
    EXPR_RESULT idx = parse_indexer(ftype);
    if (is_const(&idx.type)) {
        /* Constant index: compute base then add immediate. Try to fold constant
         * offsets when the base is a global symbol so we can emit _sym+imm.
         */
        if (base_is_pointer) {
            /* base is a pointer stored at the field address: load pointer value */
            if (is_ptr(&sym->type)) emit_ld_symval(sym); else emit_ld_symaddr(sym);
            if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
            if (idx.value) { emit_ldde_immed(); emit_n(idx.value); emit_nl(); emit_add16(); }
            emit_load_word_from_hl(); /* HL = *(base+offset) */
        } else {
            /* For non-pointer base, if sym is global we can fold offsets */
            if (sym->scope == GLOBAL) {
                emit_ld_symaddr_offset(sym, offset + idx.value);
            } else {
                if (is_ptr(&sym->type)) emit_ld_symval(sym); else emit_ld_symaddr(sym);
                if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
                if (idx.value) { emit_ldde_immed(); emit_n(idx.value); emit_nl(); emit_add16(); }
            }
        }

        *out_type = *ftype;
        make_scalar(out_type);
    } else {
        /* Non-constant index: parse_indexer already left scaled index in HL.
         * Save index, compute base, restore index and add so the final add
         * happens at the end (better for optimizers and correctness).
         */
        emit_push(); /* push index (HL) */

        if (base_is_pointer) {
            if (is_ptr(&sym->type)) emit_ld_symval(sym); else emit_ld_symaddr(sym);
            if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
            emit_load_word_from_hl(); /* HL = *(base+offset) */
        } else {
            if (is_ptr(&sym->type)) emit_ld_symval(sym); else emit_ld_symaddr(sym);
            if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
        }

        emit_pop(); /* pop index into DE */
        emit_add16();

        *out_type = *ftype;
        make_scalar(out_type);
    }
}

EXPR_RESULT parse_factor(uint8_t dereference) MYCC {
    SYMBOL sym;
    EXPR_RESULT factor_result = { .type = int_type, .has_sym = 0 };
    uint8_t prefix_inc = 0;
    uint8_t prefix_dec = 0;
    uint8_t neg = 0;
    uint8_t not = 0;
    uint8_t cmpl = 0;
    uint8_t loadval = 1;
    uint8_t addr_in_hl = 0; /* set when we computed field address into HL */
    uint8_t initial_deref = dereference; /* snapshot of caller intent to prevent inner consumption */
    uint16_t skiplbl;
    uint16_t tmplbl;
    uint16_t counter = 0;
    
    /* handle prefix ++/-- */
    while (tok == tokInc || tok == tokDec) {
        if (tok == tokInc) prefix_inc = 1;
        if (tok == tokDec) prefix_dec = 1;
        get_token();
    }

    while (tok == tokPlus || tok == tokMinus || tok == tokNot || tok == tokBitNot) {
        if (tok == tokMinus) neg ^= 1;
        if (tok == tokNot) not ^= 1;
        if (tok == tokBitNot) cmpl ^= 1;
        get_token();
    }

    switch (tok) {
        case tokLBrace:
            get_token(); // skip '{'
            skiplbl = newlbl();
            emit_jp(skiplbl);
            tmplbl = newlbl();
            emit_lbl(tmplbl);
            while (tok != tokRBrace) {
                EXPR_RESULT element = far_parse_expr_delayconst(0);
                if (!is_const(&element.type)) error_expect_const();
                if (counter++ > 0) emit_ch(','); else emit_instr("db ");
                emit_n(element.value);
                if (tok == tokRBrace) break;
                expect_comma();
                if (counter == 8) {
                    emit_nl();                    
                    counter = 0;
                }
            }
            if (counter) emit_nl();
            expect_RBrace();
            emit_lbl(skiplbl);
            emit_ld_immed(); emit_lblref(tmplbl); emit_nl();
            factor_result.type.basetype = CHAR;
            make_ptr(&factor_result.type);
            break;
        case tokLParen:
            get_token(); // skip '('
            factor_result = far_parse_expr_delayconst(0);
            expect_RParen();
            break;

        case tokIn:
            get_token(); // skip 'in'
            expect_LParen();
            far_parse_expr(0);
            expect_RParen();
            emit_copy_hl_to_bc();
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            factor_result.type = char_type; 
            break;

        case tokReadReg:
            get_token(); // skip 'readreg'
            expect_LParen();
            far_parse_expr(0);
            emit_instrln("ld bc,9275");
            emit_instrln("out (c),l");
            emit_instrln("inc b");
            expect_RParen();            
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            factor_result.type = char_type;
            break;

        case tokString: {
            factor_result.type = string_type;
            ARENA_MARKER _am = arena_get_marker();
            char *sbuf = NULL;
            size_t slen = 0;
            
            while (tok == tokString) {
                sbuf = arena_strappend(sbuf, slen, token, token_length);
                if (!sbuf) error(errTooManySymbols);
                slen += token_length;
                get_token();
            }

            if (!sbuf) sbuf = arena_strdup("", 0);
            intval = lookupstr(sbuf, (uint8_t)slen);
            emit_ld_immed(); emit_strref(intval); emit_nl();

            arena_free_to_marker(_am);
            break;
        }

        case tokAmp:
            get_token(); // skip '&'
            if (tok != tokIdent) error(errNotlvalue);
            sym = lookupIdent(token);
            if (not_defined(&sym)) {
                error(errNotDefined_s, token);
            }
                
            get_token(); // skip identifier
            factor_result.type = sym.type;

            if (tok == tokLBrack) {
                if (is_ptr(&factor_result.type)) {
                    emit_ld_symval(&sym);
                } else if (is_array(&factor_result.type) || sym.klass == FUNCTION) {
                    emit_ld_symaddr(&sym);
                    make_ptr(&factor_result.type);
                } else {
                    error(errSyntax);
                }
                emit_push();
                parse_indexer(&factor_result.type);
                emit_pop();
                emit_add16();
            } else {
                emit_ld_symaddr(&sym);
            }            
            make_ptr(&factor_result.type);
            break;

        case tokStar:
            get_token();    // skip '*'
            /* Parse inner expression with dereference set so inner postfix behavior
             * (like s++ or ++s) computes and returns the pointer value correctly
             * and inner assignment handling will write to the dereferenced location.
             */
            factor_result = parse_factor(1);
            if (is_const(&factor_result.type)) {
                emit_ld_const(factor_result.value);
                factor_result.type.basetype = base_type(&factor_result.type);
                make_ptr(&factor_result.type);
            }
            if (tok == tokAssign) {
                /* Assignment to dereferenced value: HL must contain address to write to */
                far_parse_assign(is_ptr(&factor_result.type), NULL, 0, factor_result.type);
            } else {
                /* Non-assignment: load the value pointed to by HL */
                emit_load(factor_result.type);
            }
            break;

        case tokNumber:
            if (neg) {
                intval = -intval;
                neg = 0;
            }
            if (not) {
                intval = !intval;
                not = 0;
            }
            get_token();
            if (dereference) {
                emit_ld_const(intval);
                emit_load(int_type);
                factor_result.type = int_type;                
            } 
            else {
                factor_result.type = int_type;
                factor_result.value = intval;
                make_const(&factor_result.type);
            }
            break;
                               
        case tokIdent:
            sym = lookupIdent(token);
            if (not_defined(&sym)) {
                error(errNotDefined_s, token);
                return factor_result;
            }

            get_token(); // skip identifier

            /* record simple identifier snapshot so callers (like unary '*') can
             * handle writes to the address without triggering inner-assignment
             * code that may cause extra loads. If later we process a member or
             * indexer, clear the has_sym flag as the effective address is no
             * longer represented by the simple symbol copy.
             */
            factor_result.sym = sym;
            factor_result.has_sym = 1;

            /* If this identifier is immediately indexed (e.g. s[i]), handle it
             * now using the unified helper so we compute base first (for const)
             * or otherwise save and restore the index so the final add is last.
             */
            if (tok == tokLBrack) {
                factor_result.has_sym = 0;
                if (!is_ptr(&sym.type) && !is_array(&sym.type)) error(errSyntax);
                /* Compute element type for the indexer: build a scalar element type
                 * from the base type for arrays/pointers (TYPEREC doesn't have elem).
                 */
                TYPEREC elemtype = { .basetype = sym.type.basetype, .dim = 1, .struct_id = sym.type.struct_id };

                /* Parse index first so we can optimize constant-index global reads */
                EXPR_RESULT idx = parse_indexer(&elemtype);
                if (is_const(&idx.type)) {
                    uint16_t total_offset = idx.value; /* already scaled by parse_indexer */
                    /* If base is a global symbol and element size is a word (2), and
                     * we're not assigning into it, load the word directly from the
                     * computed address: ld hl,(_sym+total_offset). This avoids a
                     * runtime add + two byte loads.
                     */
                    if (sym.scope == GLOBAL && type_size(&elemtype) == 2 && tok != tokAssign) {
                        emit_instr("ld hl,("); emit_sname(sym.name); emit_ch('+'); emit_n(total_offset); emit_strln(")");
                        factor_result.type = elemtype; make_scalar(&factor_result.type);
                        /* HL already contains the value, avoid an extra load */
                        dereference = 0;
                        addr_in_hl = 0;
                    } else {
                        /* Fallback: compute address immediately without re-parsing index */
                        if (is_ptr(&sym.type)) emit_ld_symval(&sym); else emit_ld_symaddr(&sym);
                        if (total_offset) { emit_ldde_immed(); emit_n(total_offset); emit_nl(); emit_add16(); }
                        factor_result.type = elemtype; make_scalar(&factor_result.type);
                        dereference = 1;
                        addr_in_hl = 1;
                    }
                } else {
                    /* Non-constant index: parse_indexer already left scaled index in HL.
                     * Save index, compute base and add it so final add happens at end.
                     */
                    emit_push(); /* push index (HL) */
                    if (is_ptr(&sym.type)) emit_ld_symval(&sym); else emit_ld_symaddr(&sym);
                    emit_pop(); /* pop index into DE */
                    emit_add16();

                    *(&factor_result.type) = elemtype; make_scalar(&factor_result.type);
                    dereference = 1;
                    addr_in_hl = 1;
                }
            }

            if (!addr_in_hl) factor_result.type = sym.type;
            if (is_const(&sym.type)) {
                factor_result.value = sym.offset;
                if (neg) factor_result.value = -factor_result.value;
                if (not) factor_result.value = !factor_result.value;
                if (cmpl) factor_result.value = ~factor_result.value;
                return factor_result;
            }
            if (dereference) make_scalar(&factor_result.type);

            /* Support struct member access: "ident . field" */
            while (tok == tokMember) {
                factor_result.has_sym = 0;
                get_token(); // skip '.'
                if (!is_struct(&factor_result.type)) {
                    error(errSyntax);
                    return factor_result;
                }
                if (tok != tokIdent) { error(errSyntax); return factor_result; }
                char fname[MAX_IDENT_LEN+1];
                strncpy(fname, token, MAX_IDENT_LEN);
                int sid = (int)factor_result.type.struct_id - 1;
                int fid = find_struct_field(sid, fname);
                if (fid < 0) error(errNotDefined_s, fname);

                FIELDINFO fi = get_struct_field(sid, fid);
                uint16_t offset = fi.offset;

                /* Advance past the field name so we can detect an indexer following the
                 * member access (e.g. p.name[1]). If there is an indexer, parse it first
                 * so we can push the index and then compute the base+offset without
                 * clobbering the index value.
                 */
                get_token(); // skip field name

                if (tok == tokLBrack) {
                    /* Use helper that handles both constant and non-constant indexes
                     * and ensures the final addition happens after the base computation.
                     */
                    /* Determine the element type for the field (arrays/pointers). Build
                     * a scalar element type from the base type since TYPEREC has no elem
                     * pointer in this simpler representation.
                     */
                    TYPEREC elemtype = { .basetype = fi.type.basetype, .dim = 1, .struct_id = fi.type.struct_id };

                    EXPR_RESULT idx = parse_indexer(&elemtype);
                    if (is_const(&idx.type)) {
                        uint16_t total_offset = offset + idx.value;
                        if (sym.scope == GLOBAL && type_size(&elemtype) == 2 && tok != tokAssign) {
                            emit_instr("ld hl,("); emit_sname(sym.name); emit_ch('+'); emit_n(total_offset); emit_strln(")");
                            factor_result.type = elemtype; make_scalar(&factor_result.type);
                            dereference = 0;
                            addr_in_hl = 0;
                        } else {
                            /* Fallback: compute base+offset and add constant index value */
                            if (is_ptr(&sym.type)) emit_ld_symval(&sym); else emit_ld_symaddr(&sym);
                            if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
                            if (idx.value) { emit_ldde_immed(); emit_n(idx.value); emit_nl(); emit_add16(); }
                            factor_result.type = elemtype; make_scalar(&factor_result.type);
                            dereference = 1;
                            addr_in_hl = 1;
                        }
                    } else {
                        /* Non-constant index: push index, compute base+offset, pop and add */
                        emit_push(); /* index in HL */
                        if (is_ptr(&sym.type)) emit_ld_symval(&sym); else emit_ld_symaddr(&sym);
                        if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
                        emit_pop(); /* pop index into DE */
                        emit_add16();
                        factor_result.type = elemtype; make_scalar(&factor_result.type);
                        dereference = 1;
                        addr_in_hl = 1;
                    }
                }
                else {
                    /* No indexer: compute field address now. If we already computed the
                     * element address into HL (addr_in_hl), do not reload the symbol;
                     * simply add the field offset if needed. If the symbol is global
                     * and the offset is a constant we can fold it into the load.
                     */
                    if (!addr_in_hl) {
                        if (is_ptr(&sym.type)) {
                            emit_ld_symval(&sym); /* pointer value = base */
                        } else {
                            emit_ld_symaddr_offset(&sym, offset); /* base address (+offset folded if global) */
                        }
                    } else {
                        if (offset) {
                            emit_ldde_immed(); emit_n(offset); emit_nl();
                            emit_add16();
                        }
                    }

                    /* now HL points at field */
                    factor_result.type = fi.type;
                    if (is_array(&factor_result.type)) {
                        make_ptr(&factor_result.type);
                        dereference = 0;
                    } else {
                        dereference = 1;
                    }
                    addr_in_hl = 1;
                }
            }

            /* If address is already in HL (member access), don't load the symbol.
             * Keep HL as the address (arrays decay to pointer) and avoid emitting
             * a second symbol load which would clobber HL.
             */
            if (addr_in_hl) loadval = 0;

            if (tok == tokLParen) {
                factor_result.has_sym = 0;
                parse_funccall(&sym);   
                loadval = 0;
            }

            /* handle prefix ++/-- for identifiers (supports indexed lvalues)
             * Capture whether we have an address in HL now so later modifications don't
             * confuse the optimizer. For indexed lvalues the parse_indexer already
             * pushed the index on the stack.
             */
            uint8_t had_addr_in_hl = addr_in_hl;
            if ((prefix_inc || prefix_dec)) {
                if (is_const(&sym.type)) {
                    error(errSyntax);
                } else {
                    /* step size: pointer element size for pointer types, otherwise scalar step */
                    uint16_t step;
                    if (is_ptr(&sym.type)) {
                        TYPEREC elem = { .basetype = sym.type.basetype, .dim = 1, .struct_id = sym.type.struct_id };
                        step = type_size(&elem);
                    } else {
                        /* Scalar increment/decrement always adjust by 1 (not by sizeof(int)). */
                        step = 1;
                    }
                    if (had_addr_in_hl) {
                        /* Address already computed into HL (member or indexed access). */
                        emit_push(); /* push address for store */
                        emit_load(factor_result.type); /* HL = original value */

                        if (prefix_dec) {
                            emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                        } else {
                            emit_instrln("ld de,%d", step);
                        }
                        emit_add16(); /* HL = new value */

                        emit_store(factor_result.type); /* pop address and store HL */
                        loadval = 0;
                        dereference = 0;
                    } else {
                        /* simple identifier */
                        emit_ld_symval(&sym);
                        if (prefix_dec) {
                            emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                        } else {
                            emit_instrln("ld de,%d", step);
                        }
                        emit_add16();
                        emit_store_sym(&sym);
                        loadval = 0;
                    }
                }
            }
            if (tok == tokAssign) {
                /* If this parse was invoked within a unary dereference (e.g., parsing
                 * the inner expression of '*inner'), do NOT consume the assignment here;
                 * let the outer dereference handle assignment to the location. This
                 * avoids extra loads/stores and preserves HL for the outer handler.
                 */
                if (!initial_deref) {
                    if (had_addr_in_hl) {
                        /* HL already contains address of field, so pass sym==NULL to indicate that */
                        far_parse_assign(1, NULL, 0, factor_result.type);
                    } else {
                        far_parse_assign(dereference, &sym, 0, factor_result.type);
                    }
                }
            } else {
                /* handle postfix ++/-- */
                if (tok == tokInc || tok == tokDec) {
                    uint8_t isdec = (tok == tokDec);
                    get_token();
                    if (is_const(&sym.type)) {
                        error(errSyntax);
                    } else {
                        uint16_t step;
                        if (is_ptr(&sym.type)) {
                            TYPEREC elem = { .basetype = sym.type.basetype, .dim = 1, .struct_id = sym.type.struct_id };
                            step = type_size(&elem);
                        } else {
                            /* Scalar increment/decrement always adjust by 1 (not by sizeof(int)). */
                            step = 1;
                        }
                        if (had_addr_in_hl) {
                            /* Address already computed into HL (member or indexed access). */
                            emit_push(); /* push address */
                            emit_load(factor_result.type); /* HL = original value */

                            /* save original in BC */
                            emit_copy_hl_to_bc();

                            /* compute new value */
                            if (isdec) {
                                emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                                emit_add16();
                            } else {
                                emit_ldde_immed_n(step);
                                emit_add16();
                            }

                            emit_store(factor_result.type); /* pop address and store new value */

                            /* restore original from BC */
                            emit_copy_bc_to_hl(); 

                            loadval = 0;
                            dereference = 0;
                        } else {
                            /* simple identifier postfix */
                            if (loadval) {
                                if (is_func_or_proto(&sym)) {
                                    emit_ld_immed(); emit_sname(sym.name); emit_nl();
                                } else {
                                    emit_ld_symval(&sym);
                                }
                            }

                            /* save original, compute new, store, restore original */
                            emit_push();
                            if (isdec) {
                                emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                            } else {
                                emit_ldde_immed_n(step);
                            }
                            emit_add16();
                            emit_store_sym(&sym);
                            emit_pop();
                            emit_swap();
                            /* now HL contains original value for postfix expression */
                            loadval = 0;
                        }
                    }
                }

                if (loadval) {
                    if (is_func_or_proto(&sym)) {
                        emit_ld_immed(); emit_sname(sym.name); emit_nl();
                    } else {
                        emit_ld_symval(&sym);
                    }
                }

                if (dereference) {
                    emit_load(factor_result.type);
                }
            }
            break; 
        default:
            error(errSyntax);            
            break;
    }
    if (neg) emit_neg();
    if (not) emit_rtl("ccnot");
    if (cmpl) emit_rtl("cccom");
    return factor_result;
}

void far_parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC {
    get_token(); // skip '='

    if (sym && is_const(&sym->type)) {
        EXPR_RESULT r = far_parse_expr_delayconst(0);
        if (!is_const(&r.type)) error_expect_const();
        sym->offset = r.value;
        updatesym(sym);
        return;
    }

    /* If assigning a brace-initializer to a global symbol, emit data
     * directly for that symbol using the correct directive width.
     */
    if (tok == tokLBrace) {
        get_token(); // skip '{'
        uint16_t counter = 0;
        uint16_t elementcount = 0;
        BASE_TYPE bt = base_type(&typ);
        uint16_t skiplbl = newlbl();
        uint16_t datalbl = newlbl();
        uint16_t datalen = NO_LABEL;

        if (!sym && !dereference) error(errSyntax);
        
        if (!dereference) {            
            emit_ld_immed(); emit_lblref(datalbl); emit_nl();
            if (sym) {
                emit_store_sym(sym);
            }
            else {
                emit_store(typ);
            }
        }
        else {     
            datalen = newlbl();

            if (sym) {
                emit_ld_symval(sym);                
            }
            else {
                emit_load(typ);
            }
            if (indexed) {
                emit_pop();
                emit_add16();
            }
            
            emit_ldde_immed(); emit_lblref(datalbl); emit_nl();
            emit_swap();
            emit_ldbc_immed(); emit_lblref(datalen); emit_nl();
            emit_instrln("ldir");
        }
        
        /* Jump over the data so CPU doesn't execute it at runtime */
        emit_jp(skiplbl);
        /* Emit label + data for the global variable */
        emit_lbl(datalbl);
        emit_ch(' ');
        
        while (tok != tokRBrace && tok != tokEOS) {
            EXPR_RESULT element = far_parse_expr_delayconst(0);
            if (!is_const(&element.type)) error_expect_const();

            ++elementcount;
            if (counter++ > 0) emit_ch(','); else emit_instr(bt == INT ? "dw " : "db ");

            if (bt == INT) {
                emit_n(element.value);
            } else {
                emit_n(element.value & 0xff);
            }

            if (tok == tokRBrace) break;
            expect_comma();
            if (counter == 8) {
                emit_nl();
                counter = 0;
            }
        }
        if (counter) emit_nl();
        if (datalen != NO_LABEL) {
            emit_lblequ16(datalen, elementcount * (bt == INT ? 2 : 1));
        }
        expect_RBrace();

        /* place the skip label here so code continues after the data */
        emit_lbl(skiplbl);
        return;
    }

    if (!sym) {
        // HL contains address to write to
        emit_push();
        far_parse_expr(0);
        emit_store(typ);
        return;
    }
    
    if (dereference) {
        emit_ld_symval(sym);
        if (indexed) {
            emit_pop();
            emit_add16();
        }

        emit_push();        
    }
    far_parse_expr(0);

    if (dereference) {
        emit_store(typ);
    }
    else {
        emit_store_sym(sym);
    }
}

EXPR_RESULT parse_binop(TOKEN op, EXPR_RESULT l_result, uint8_t opprec) MYCC {
    uint16_t scaleL = 0;
    uint16_t scaleR = 0;
    
    EXPR_RESULT r_result = { .type = void_type};
    EXPR_RESULT short_circuit_result = { .type = int_type };

    /* Determine element scale when one operand is a pointer and the other is an integer.
     * scaleR is the byte-scale to apply to the right operand (HL) when the left
     * operand is a pointer. scaleL is the byte-scale to apply to the left operand
     * (DE) when the right operand is a pointer.
     */
    if (op == tokOr || op == tokAnd) {
        uint16_t short_circuit_lbl = newlbl();
        uint8_t short_circuit = 0;
        if (is_const(&l_result.type)) {
            emit_ld_const(l_result.value);
            if (op == tokOr) {
                short_circuit = l_result.value != 0;               
            }
            else if (op == tokAnd) {
                short_circuit = l_result.value == 0;                
            }            
        }
        
        if (!short_circuit) {
            switch (op) {
                case tokOr:
                    emit_jp_true(short_circuit_lbl);
                    break;
                case tokAnd:
                    emit_jp_false(short_circuit_lbl);
                    break;
            }
        } else {
            emit_jp(short_circuit_lbl);
        }
        l_result = far_parse_expr(opprec + 1);
        emit_lbl(short_circuit_lbl);

        return short_circuit_result;
    }

    if (!is_const(&l_result.type)) {
        emit_push();        
    }

    r_result = far_parse_expr_delayconst(opprec + 1);

    /* Treat arrays as decaying to pointers for arithmetic. Compute element
     * size for pointer+int arithmetic so we can scale the integer operand by
     * the pointed-to element size (handles int pointers and struct pointers).
     */
    if ((is_ptr(&l_result.type) || is_array(&l_result.type)) && base_type(&r_result.type) == INT) {
        TYPEREC elem = { .basetype = l_result.type.basetype, .dim = 1, .struct_id = l_result.type.struct_id };
        scaleR = type_size(&elem);
    }
    if ((is_ptr(&r_result.type) || is_array(&r_result.type)) && base_type(&l_result.type) == INT) {
        TYPEREC elem = { .basetype = r_result.type.basetype, .dim = 1, .struct_id = r_result.type.struct_id };
        scaleL = type_size(&elem);
    }

    if (scaleL && scaleR) error(errSyntax);

    uint8_t pointer = is_ptr(&l_result.type) || is_ptr(&r_result.type);

    if (pointer) {
        // Check for invalid pointer operations
        switch(op) {
        case tokStar:
        case tokDiv:
        case tokMod:
        case tokOr:
        case tokAnd:
        case tokShl:
        case tokShr:
            error(errSyntax);
            break;
        }
    }

    if (is_const(&l_result.type) && is_const(&r_result.type)) {
        switch (op) {
            case tokLt: 
                if (pointer) l_result.value = l_result.value < r_result.value; 
                else l_result.value = (int16_t)l_result.value < (int16_t)r_result.value; 
                break;
            case tokLeq: if (pointer) l_result.value = l_result.value <= r_result.value; 
            else l_result.value = (int16_t)l_result.value <= (int16_t)r_result.value; 
            break;
            case tokGt: 
                if (pointer) l_result.value = l_result.value > r_result.value; 
                else l_result.value = (int16_t)l_result.value > (int16_t)r_result.value; 
                break;
            case tokGeq: 
                if (pointer) l_result.value = l_result.value >= r_result.value; 
                else l_result.value = (int16_t)l_result.value >= (int16_t)r_result.value; 
                break;
            case tokEq: l_result.value = l_result.value == r_result.value; break;
            case tokNeq: l_result.value = l_result.value != r_result.value; break;
            case tokPlus:
                if (scaleR) l_result.value = l_result.value + (r_result.value * scaleR);
                else if (scaleL) l_result.value = (l_result.value * scaleL) + r_result.value;
                else l_result.value = l_result.value + r_result.value;
                break;
            case tokMinus:
                if (scaleR) l_result.value = l_result.value - (r_result.value * scaleR);
                else if (scaleL) l_result.value = (l_result.value * scaleL) - r_result.value;
                else l_result.value = l_result.value - r_result.value;
                break;
            case tokStar: l_result.value = l_result.value * r_result.value; break;
            case tokDiv: l_result.value = l_result.value / r_result.value; break;
            case tokMod: l_result.value = l_result.value % r_result.value; break;
            case tokOr: l_result.value = l_result.value | r_result.value; break;
            case tokAnd: l_result.value = l_result.value & r_result.value; break;
            case tokShl: l_result.value = l_result.value << r_result.value; break;
            case tokShr: l_result.value = l_result.value >> r_result.value; break;
            case tokBitOr: l_result.value = l_result.value | r_result.value; break;
            case tokBitAnd: l_result.value = l_result.value & r_result.value; break;
            case tokBitXor: l_result.value = l_result.value ^ r_result.value; break;
            default:
                error(errSyntax);
                break;
        }        
        return l_result;
    } 
    
    if (is_const(&l_result.type)) {
        uint16_t val = l_result.value;
        if (scaleL) val = val * scaleL; /* pre-scale left constant when right is a pointer */
        emit_ldde_immed();
        emit_n(val);
        scaleL = 0;
        emit_nl();
        l_result.type.basetype = base_type(&l_result.type);
    }
    else if (is_const(&r_result.type)) {
        uint16_t val = r_result.value;
        if (scaleR) val = val * scaleR; /* pre-scale right constant when left is a pointer */
        emit_ld_immed();
        emit_n(val);
        scaleR = 0; /* constant already scaled */
        emit_nl();
        r_result.type.basetype = base_type(&r_result.type);
        emit_pop();
    } else {
        emit_pop();
    }

    switch (op) {
        case tokLt:
            emit_rtl(pointer ? "ccult" : "cclt");
            break;

        case tokLeq:
            emit_rtl(pointer ? "ccule" : "ccle");
            break;

        case tokGt:
            emit_rtl(pointer ? "ccugt" : "ccgt");
            break;

        case tokGeq:
            emit_rtl(pointer ? "ccuge" : "ccge");
            break;

        case tokEq:
            emit_rtl("cceq");
            break;

        case tokNeq:
            emit_rtl("ccne");
            break;

        case tokPlus:
            /* If the right operand is a constant and we computed a scale earlier,
             * apply scaling directly to HL (the constant). For non-constant RHS
             * the index is already left in HL and scaleR will be applied the same way.
             */
            if (scaleR) {
                if (scaleR == 2) {
                    emit_mul2();
                } else {
                    uint16_t s = scaleR;
                    if ((s & (s - 1)) == 0) {
                        while (s > 1) { emit_mul2(); s >>= 1; }
                    } else {
                        emit_ldde_immed_n(scaleR);
                        emit_rtl("ccmult");
                    }
                }
            }

            if (scaleL) {
                if (scaleL == 2) {
                    emit_mulDE2();
                } else {
                    uint16_t s = scaleL;
                    if ((s & (s - 1)) == 0) {
                        while (s > 1) { emit_mulDE2(); s >>= 1; }
                    } else {
                        /* General case: multiply DE (left) by scaleL while preserving HL (right).
                         * Sequence:
                         *  - copy right (HL) to BC
                         *  - swap so HL=left
                         *  - load DE with scaleL and ccmult (HL = left * scaleL)
                         *  - swap back and restore right from BC so HL=right and DE=scaled_left
                         */
                        emit_copy_hl_to_bc();
                        emit_swap(); /* HL = left */
                        emit_ldde_immed_n(scaleL);
                        emit_rtl("ccmult"); /* HL = left * scaleL */
                        emit_swap(); /* HL = scale, DE = left*scale */
                        emit_copy_bc_to_hl(); /* restore right into HL */
                    }
                }
            }

            emit_add16();
            break;
        case tokMinus:
            if (scaleR) {
                if (scaleR == 2) {
                    emit_mul2();
                } else {
                    uint16_t s = scaleR;
                    if ((s & (s - 1)) == 0) {
                        while (s > 1) { emit_mul2(); s >>= 1; }
                    } else {
                        emit_ldde_immed_n(scaleR);
                        emit_rtl("ccmult");
                    }
                }
            }

            if (scaleL) {
                if (scaleL == 2) {
                    emit_mulDE2();
                } else {
                    uint16_t s = scaleL;
                    if ((s & (s - 1)) == 0) {
                        while (s > 1) { emit_mulDE2(); s >>= 1; }
                    } else {
                        emit_copy_hl_to_bc();
                        emit_swap(); /* HL = left */
                        emit_ldde_immed_n(scaleL);
                        emit_rtl("ccmult"); /* HL = left * scaleL */
                        emit_swap(); /* HL = scale, DE = left*scale */
                        emit_copy_bc_to_hl(); /* restore right into HL */
                    }
                }
            }

            emit_sub16();
            break;
        case tokStar:
            emit_rtl("ccmult");
            break;
        case tokDiv:
            emit_rtl("ccdiv");
            break;
        case tokMod:
            emit_rtl("ccdiv");
            emit_swap();
            break;
        case tokShl:
            emit_instrln("ld b,l");  // B  = shift count
            emit_instrln("bsla de,b");
            emit_swap();            // HL = result;
            break;
        case tokShr:
            emit_instrln("ld b,l");  // B  = shift count
            emit_instrln("bsra de,b");
            emit_swap();            // HL = result;
            break;

        case tokBitOr:
            emit_rtl("ccor");
            break;

        case tokBitAnd:
            emit_rtl("ccand");
            break;

        case tokBitXor:
            emit_rtl("ccxor");
            break;

        default:
            error(errSyntax);
            break;
    }
    return l_result;
}
