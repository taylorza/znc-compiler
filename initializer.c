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
        if (!sbuf) error(errTooManySymbols);
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

    uint8_t is_char = type_is_char(element_type_id);
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
                if (counter++ > 0) emit_ch(','); else emit_instr("dw ");
                emit_strref(sid);
            } else if (field_is_arr) {
                /* char[] field: emit string bytes inline */
                uint16_t arr_size = type_size(field_type_id);
                ARENA_MARKER _am = arena_get_marker();
                char* sbuf = NULL;
                size_t slen = 0;
                while (tok == tokString) {
                    sbuf = arena_strappend(sbuf, slen, token, token_length);
                    if (!sbuf) error(errTooManySymbols);
                    slen += token_length;
                    get_token();
                }
                if (!sbuf) { sbuf = arena_strdup("", 0); slen = 0; }
                if (counter++ > 0) emit_ch(','); else emit_instr("db ");
                /* emit each character as a byte literal */
                for (size_t ci = 0; ci < slen; ++ci) {
                    if (ci > 0) emit_ch(',');
                    emit_n((uint16_t)(uint8_t)sbuf[ci]);
                }
                /* pad remainder of array with zeros */
                for (uint16_t ci = (uint16_t)slen; ci < arr_size; ++ci) {
                    emit_ch(',');
                    emit_n(0);
                }
                arena_free_to_marker(_am);
                ++elementcount;
            } else {
                error(errTypeError);
            }
        } else {
            EXPR_RESULT element = parse_expr_delayconst(0, field_type_id);
            uint8_t is_func = element.has_sym && is_func_or_proto(&element.sym);
            if (!type_is_const(element.type_id) && !is_func) error_expect_const();

            if (is_delegate) {
                if (!is_func) error(errTypeError);
                if (!signature_check(delegate_sig_id, element.sym.fn.signature_id)) {
                    error(errTypeError);
                }
            }
            else if (!type_check_compatible(element.type_id, field_type_id) && field_type_id != 0) {
                error(errTypeError);
            }

            ++elementcount;
            if (counter++ > 0)
                emit_ch(',');
            else
                emit_instr(is_char ? "db " : "dw ");

            if (is_func) {
                emit_sname_id(element.sym.name_id);
            }
            else if (is_char) {
                emit_n(element.value & 0xff);
            }
            else {
                uint16_t emit_val = element.value;
                /* Coerce int/char constants into Q4 when element type is fixed */
                if (type_is_fixed(field_type_id) && !type_is_fixed(element.type_id) &&
                    (type_is_int(element.type_id) || type_is_char(element.type_id))) {
                    emit_val = (uint16_t)((int16_t)emit_val << 4);
                }
                emit_n(emit_val);
            }
        }

        ++field_idx;
        if (tok == tokRBrace) break;
        if (tok != tokComma) { error(errSyntax); break; }
        get_token(); // skip ','
        if (tok == tokRBrace) break; // allow trailing comma
        if (counter == 8) {
            emit_nl();
            counter = 0;
        }
    }
    if (counter) emit_nl();

    return elementcount;
}
