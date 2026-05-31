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
            FIELDINFO fi = get_struct_field(struct_id, field_idx);
            field_type_id = fi.type_id;
        }

        if (tok == tokString) {
            uint8_t field_is_ptr   = type_is_pointer(field_type_id) && type_is_char(type_get_element_type_id(field_type_id));
            uint8_t field_is_arr   = type_is_array(field_type_id)   && type_is_char(type_get_element_type_id(field_type_id));
            uint8_t field_inferred = (element_type_id == 0);

            if (field_is_ptr || field_inferred) {
                /* char* field or inferred: store address of string literal */
                uint16_t sid = far_parse_concat_string_literal();
                ++elementcount;
                if (counter > 0 && last_is_char) { emit_nl(); counter = 0; }
                if (counter++ > 0) emit_ch(','); else emit_instr("dw ");
                emit_strref(sid);
                last_is_char = 0;
            } else if (field_is_arr) {
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
                ++elementcount;
            } else {
                error(errTypeError);
            }
        } else if (tok == tokLBrace) {
            get_token(); /* skip '{' */

            if (type_is_array(field_type_id)) {
                /* Fixed-size array field: e.g. int[4] coords = {1,2,3,4} */
                uint8_t  arr_elem = type_get_element_type_id(field_type_id);
                uint16_t arr_len  = type_get_array_length(field_type_id);
                uint8_t  arr_char = type_size(arr_elem) == 1;
                for (uint16_t ai = 0; ai < arr_len; ai++) {
                    EXPR_RESULT aelem = parse_expr_delayconst(0, arr_elem);
                    if (!type_is_const(aelem.type_id)) error(errConstExpected);
                    if (!type_check_compatible(aelem.type_id, arr_elem) && arr_elem != 0) error(errTypeError);
                    uint16_t emit_val = aelem.value;
                    if (type_is_fixed(arr_elem) && !type_is_fixed(aelem.type_id) &&
                        (type_is_int(aelem.type_id) || type_is_char(aelem.type_id) || type_is_byte(aelem.type_id)))
                        emit_val = (uint16_t)((int16_t)emit_val << 4);
                    EMIT_VAL(arr_char, emit_val);
                    if (ai < arr_len - 1) {
                        if (tok != tokComma) { error(errExpected_c, ','); break; }
                        get_token(); /* skip ',' */
                    }
                }
            } else if (is_struct) {
                /* Nested struct sub-initializer: { field1, field2, ... }
                 * Each nested brace is one struct instance in an array of structs. */
                for (int sfi_idx = 0; sfi_idx < field_count; sfi_idx++) {
                    FIELDINFO sfi     = get_struct_field(struct_id, sfi_idx);
                    uint8_t ftype     = sfi.type_id;
                    uint8_t f_is_char = type_size(ftype) == 1;

                    if (type_is_array(ftype) && tok == tokLBrace) {
                        /* Array member inside nested struct */
                        get_token(); /* skip '{' */
                        uint8_t  fa_elem = type_get_element_type_id(ftype);
                        uint16_t fa_len  = type_get_array_length(ftype);
                        uint8_t  fa_char = type_size(fa_elem) == 1;
                        for (uint16_t ai = 0; ai < fa_len; ai++) {
                            EXPR_RESULT aelem = parse_expr_delayconst(0, fa_elem);
                            if (!type_is_const(aelem.type_id)) error(errConstExpected);
                            if (!type_check_compatible(aelem.type_id, fa_elem) && fa_elem != 0) error(errTypeError);
                            uint16_t emit_val = aelem.value;
                            if (type_is_fixed(fa_elem) && !type_is_fixed(aelem.type_id) &&
                                (type_is_int(aelem.type_id) || type_is_char(aelem.type_id) || type_is_byte(aelem.type_id)))
                                emit_val = (uint16_t)((int16_t)emit_val << 4);
                            EMIT_VAL(fa_char, emit_val);
                            if (ai < fa_len - 1) {
                                if (tok != tokComma) { error(errExpected_c, ','); break; }
                                get_token(); /* skip ',' */
                            }
                        }
                        expect(tokRBrace, '}');
                    } else {
                        EXPR_RESULT selem = parse_expr_delayconst(0, ftype);
                        uint8_t selem_is_func = selem.has_sym && is_func_or_proto(&selem.sym);
                        if (!type_is_const(selem.type_id) && !selem_is_func) error(errConstExpected);
                        if (!type_check_compatible(selem.type_id, ftype) && ftype != 0) error(errTypeError);
                        uint16_t emit_val = selem.value;
                        if (type_is_fixed(ftype) && !type_is_fixed(selem.type_id) &&
                            (type_is_int(selem.type_id) || type_is_char(selem.type_id) || type_is_byte(selem.type_id)))
                            emit_val = (uint16_t)((int16_t)emit_val << 4);
                        EMIT_VAL(f_is_char, emit_val);
                    }

                    if (sfi_idx < field_count - 1) {
                        if (tok != tokComma) { error(errExpected_c, ','); break; }
                        get_token(); /* skip ',' */
                    }
                }
            } else {
                error(errTypeError);
            }

            expect(tokRBrace, '}');
            ++elementcount;
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

            ++elementcount;
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
                    (type_is_int(element.type_id) || type_is_char(element.type_id) || type_is_byte(element.type_id))) {
                    emit_val = (uint16_t)((int16_t)emit_val << 4);
                }
                EMIT_VAL(0, emit_val);
            }
        }

        ++field_idx;
        if (tok == tokRBrace) break;
        if (tok != tokComma) { error(errExpected_c, ','); break; }
        get_token(); // skip ','
        if (tok == tokRBrace) break; // allow trailing comma
    }
    if (counter) emit_nl();

#undef EMIT_VAL
    return elementcount;
}
