#include "znc.h"
#include "struct.h"
#include "shared.h"

/* Parse one or more adjacent string tokens ("a" "b") and return string id */
uint16_t far_parse_concat_string_literal(void) MYCC {
    ARENA_MARKER _am = arena_get_marker();
    char *sbuf = NULL;
    size_t slen = 0;
    while (tok == tokString) {
        sbuf = arena_strappend(sbuf, slen, token, token_length);
        if (!sbuf) error(errArenaOutOfMemory);
        slen += token_length;
        get_token();
    }
    if (!sbuf) sbuf = arena_strdup("", 0);
    uint16_t sid = lookupstr(sbuf, (uint8_t)slen);
    arena_free_to_marker(_am);
    intval = sid; /* keep global consistent with previous behavior */
    return sid;
}

/* Parse and emit elements for brace-initializer expressions.
 * Returns the number of elements processed.
 * element_type_id: expected type for each element (0 = infer from data)
 */
uint16_t far_parse_brace_initializer_elements(uint8_t element_type_id) MYCC {
    uint16_t counter = 0;
    uint16_t elementcount = 0;

    /* Tracks whether the currently open directive line is db (1) or dw (0).
     * When the next value has a different width we flush and start a new line. */
    uint8_t last_is_char = type_size(element_type_id) == 1;

    /* For struct types, iterate fields so we know each field's type */
    uint8_t is_struct = type_is_struct(element_type_id);
    int struct_id = -1;
    int field_count = 0;
    int field_idx = 0;
    if (is_struct) {
        struct_id = (int)type_get_struct_id(element_type_id) - 1;
        field_count = get_field_count(struct_id);
    }

    uint8_t delegate_sig_id = 0xFF;
    uint8_t is_delegate = type_is_delegate(element_type_id);
    if (is_delegate) {
        delegate_sig_id = type_get_function_sig(element_type_id);
    }

    /* Emit a single constant value, grouping consecutive same-width values onto
     * one line (up to 16 per line) and starting a new line when width changes. */
#define EMIT_VAL(is_c, val) \
    do { \
        uint8_t _ic = (uint8_t)(is_c); \
        uint16_t _v = (uint16_t)(val); \
        if (counter > 0 && _ic != last_is_char) { emit_nl(); counter = 0; } \
        if (counter++ > 0) emit_ch(','); else emit_instr(_ic ? "db " : "dw "); \
        if (_ic) emit_n(_v & 0xff); else emit_n(_v); \
        last_is_char = _ic; \
        if (counter == 16) { emit_nl(); counter = 0; } \
    } while (0)

    while (tok != tokRBrace && tok != tokEOS) {
        /* Determine the type expected for this element */
        uint8_t field_type_id = element_type_id;

        if (is_struct && field_idx < field_count) {
            FIELDINFO fi = get_struct_field(struct_id, field_idx++);
            field_type_id = fi.type_id;
        }

        if (type_is_struct(field_type_id)) {
            if (elementcount) emit_nl();
            counter = 0;
            expect_LBrace();
            far_parse_brace_initializer_elements(field_type_id);
            expect_RBrace();
        } else if (tok == tokString) {
            uint8_t field_elem = type_get_element_type_id(field_type_id);
            uint8_t field_is_ptr = type_is_pointer(field_type_id) &&
                (type_is_char(field_elem) || type_is_byte(field_elem));
            uint8_t field_is_arr = type_is_array(field_type_id) &&
                (type_is_char(field_elem) || type_is_byte(field_elem));
            uint8_t field_inferred = (element_type_id == 0);

            if (field_is_ptr || field_inferred) {
                /* char* field or inferred: store address of string literal */
                uint16_t sid = far_parse_concat_string_literal();
                if (counter > 0 && last_is_char) { emit_nl(); counter = 0; }
                if (counter++ > 0) emit_ch(','); else emit_instr("dw ");
                emit_strref(sid);
                last_is_char = 0;
            }
            else if (field_is_arr) {
                /* char[] field: emit string bytes inline, grouped with db */
                uint16_t arr_size = type_size(field_type_id);
                ARENA_MARKER _am = arena_get_marker();
                char* sbuf = NULL;
                size_t slen = 0;
                while (tok == tokString) {
                    sbuf = arena_strappend(sbuf, slen, token, token_length);
                    if (!sbuf) error(errArenaOutOfMemory);
                    slen += token_length;
                    get_token();
                }
                if (!sbuf) { sbuf = arena_strdup("", 0); slen = 0; }

                for (size_t ci = 0; ci < slen; ++ci) {
                    EMIT_VAL(1, (uint16_t)(uint8_t)sbuf[ci]);
                }
                for (uint16_t ci = (uint16_t)slen; ci < arr_size; ++ci) {
                    EMIT_VAL(1, 0);
                }

                arena_free_to_marker(_am);
            }
            else {
                error(errTypeError);
            }
        } else if (tok == tokLBrace) {
            get_token(); /* skip '{' */
            if (elementcount) emit_nl();
            counter = 0;
            if (is_struct) {
                far_parse_brace_initializer_elements(element_type_id);
            } else if (type_is_array(element_type_id)) {
                // Fixed-size array: emit elements and pad with zeros if needed
                uint8_t  fa_elem = type_get_element_type_id(element_type_id);
                uint16_t fa_len = type_get_array_length(element_type_id);
                uint8_t  fa_char = type_size(fa_elem) == 1;

                uint16_t count = far_parse_brace_initializer_elements(fa_elem);
                while (count++ < fa_len) EMIT_VAL(fa_char, 0);
            } else {
                if (type_is_pointer(element_type_id)) error(errTypeError);
                far_parse_brace_initializer_elements(element_type_id);
            }
            expect_RBrace();           
        } else {
            EXPR_RESULT element = parse_expr_delayconst(0, field_type_id);
            uint8_t is_func = element.has_sym && is_func_or_proto(&element.sym);
            if (!type_is_const(element.type_id) && !is_func) error(errConstExpected);

            if (is_delegate) {
                if (!is_func) error(errFunctionExpected);
                if (!signature_check(delegate_sig_id, element.sym.fn.signature_id)) {
                    error(errTypeError);
                }
            }
            else if (!type_check_compatible(element.type_id, field_type_id) && field_type_id != 0) {
                error(errTypeError);
            }
            
            uint8_t field_is_char = is_func ? 0 : (type_size(field_type_id) == 1);
            if (is_func) {
                /* Function references are always word-sized; flush if currently on a db line */
                if (counter > 0 && last_is_char) { emit_nl(); counter = 0; }
                if (counter++ > 0) emit_ch(','); else emit_instr("dw ");
                emit_sname_id(element.sym.name_id);
                last_is_char = 0;
                if (counter == 16) { emit_nl(); counter = 0; }
            } else if (field_is_char) {
                EMIT_VAL(1, element.value & 0xff);
            } else {
                uint16_t emit_val = element.value;
                /* Coerce int/char constants into Q4 when element type is fixed */
                if (type_is_fixed(field_type_id) && !type_is_fixed(element.type_id) &&
                    type_is_integral(element.type_id)) {
                    emit_val = (uint16_t)((int16_t)emit_val << 4);
                }
                EMIT_VAL(0, emit_val);
            }
            
        }
        ++elementcount; 
        if (tok == tokRBrace) break;
        if (tok != tokComma) { error(errExpected_c, ','); break; }        
        get_token(); // skip ','        
    }
    if (counter) emit_nl();

#undef EMIT_VAL
    return elementcount;
}
