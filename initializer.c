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
uint16_t far_parse_struct_initializer_fields(uint8_t struct_type_id) MYCC;
uint16_t far_parse_brace_initializer_elements(uint8_t element_type_id, uint16_t expected_count) MYCC;

static void emit_initializer_value(uint16_t *counter, uint8_t *last_is_char, uint8_t is_char, uint16_t value) MYCC {
    if (*counter > 0 && is_char != *last_is_char) { emit_nl(); *counter = 0; }
    if ((*counter)++ > 0) emit_ch(','); else emit_instr(is_char ? "db " : "dw ");
    if (is_char) emit_n(value & 0xff); else emit_n(value);
    *last_is_char = is_char;
    if (*counter == 16) { emit_nl(); *counter = 0; }
}

/* Helper that ensures a pointer/string emission starts on its own line when requested */
static void emit_strref_line(uint16_t sid, uint16_t *counter, uint8_t *last_is_char) MYCC {
    if (*counter) emit_nl();
    *counter = 0;
    emit_instr("dw ");
    emit_strref(sid);
    emit_nl();
    *last_is_char = 0;
}

static void emit_zero_for_type(uint8_t type_id, uint16_t *counter, uint8_t *last_is_char) MYCC {
    if (type_is_struct(type_id)) {
        int struct_id = (int)type_get_struct_id(type_id) - 1;
        int field_count = get_field_count(struct_id);
        for (int i = 0; i < field_count; ++i) {
            FIELDINFO fi = get_struct_field(struct_id, i);
            emit_zero_for_type(fi.type_id, counter, last_is_char);
        }
        return;
    }

    if (type_is_array(type_id)) {
        uint8_t elem_type = type_get_element_type_id(type_id);
        uint16_t len = type_get_array_length(type_id);
        for (uint16_t i = 0; i < len; ++i) {
            emit_zero_for_type(elem_type, counter, last_is_char);
        }
        return;
    }

    emit_initializer_value(counter, last_is_char, type_size(type_id) == 1, 0);
}

static uint16_t parse_initializer_item(uint8_t field_type_id, uint16_t *counter, uint8_t *last_is_char, uint8_t force_line) MYCC {
    /* initializer parsing */
    if (type_is_struct(field_type_id)) {
        if (tok != tokLBrace) { error(errExpected_c, '{'); return 0; }
        get_token();
        if (*counter) emit_nl();
        *counter = 0;
        far_parse_struct_initializer_fields(field_type_id);
        expect_RBrace();
        return 1;
    }

    if (tok == tokString) {
        uint8_t field_elem = type_get_element_type_id(field_type_id);
        uint8_t field_is_ptr = type_is_pointer(field_type_id) &&
            (type_is_char(field_elem) || type_is_byte(field_elem));
        uint8_t field_is_arr = type_is_array(field_type_id) &&
            (type_is_char(field_elem) || type_is_byte(field_elem));
        uint8_t field_inferred = (field_type_id == 0);

        if (field_is_ptr || field_inferred) {
            uint16_t sid = far_parse_concat_string_literal();
            if (force_line) {
                emit_strref_line(sid, counter, last_is_char);
            } else {
                /* Emit pointer to string literal using emit_strref so assembler shows label+offset */
                if (*counter > 0 && *last_is_char) { emit_nl(); *counter = 0; }
                if ((*counter)++ > 0) emit_ch(','); else emit_instr("dw ");
                emit_strref(sid);
                *last_is_char = 0;
                if (*counter == 16) { emit_nl(); *counter = 0; }
            }
        }
        else if (field_is_arr) {
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
                emit_initializer_value(counter, last_is_char, 1, (uint16_t)(uint8_t)sbuf[ci]);
            }
            for (uint16_t ci = (uint16_t)slen; ci < arr_size; ++ci) {
                emit_initializer_value(counter, last_is_char, 1, 0);
            }

            arena_free_to_marker(_am);
        }
        else {
            error(errTypeError);
        }
        return 1;
    }

    if (tok == tokLBrace) {
        get_token();
        if (*counter) emit_nl();
        *counter = 0;
        if (type_is_array(field_type_id)) {
            uint8_t fa_elem = type_get_element_type_id(field_type_id);
            uint16_t fa_len = type_get_array_length(field_type_id);

            uint16_t count = far_parse_brace_initializer_elements(fa_elem, fa_len);
            if (count > fa_len) error(errTypeError);
        } else {
            if (type_is_pointer(field_type_id)) error(errTypeError);
            far_parse_brace_initializer_elements(field_type_id, 0);
        }
        expect_RBrace();
        return 1;
    }

    EXPR_RESULT element = parse_expr_delayconst(0, field_type_id);
    uint8_t is_func = element.has_sym && is_func_or_proto(&element.sym);
    if (!type_is_const(element.type_id) && !is_func) error(errConstExpected);

    if (type_is_delegate(field_type_id)) {
        if (!is_func) error(errFunctionExpected);
        if (!signature_check(type_get_function_sig(field_type_id), element.sym.fn.signature_id)) {
            error(errTypeError);
        }
    }
    else if (!type_check_compatible(element.type_id, field_type_id) && field_type_id != 0) {
        error(errTypeError);
    }

    if (is_func) {
        if (*counter > 0 && *last_is_char) { emit_nl(); *counter = 0; }
        if ((*counter)++ > 0) emit_ch(','); else emit_instr("dw ");
        emit_sname_id(element.sym.name_id);
        *last_is_char = 0;
        if (*counter == 16) { emit_nl(); *counter = 0; }
    } else {
        uint16_t emit_val = element.value;
        uint8_t field_is_char = (type_size(field_type_id) == 1);
        if (field_is_char) {
            emit_initializer_value(counter, last_is_char, 1, emit_val & 0xff);
        } else {
            if (type_is_fixed(field_type_id) && !type_is_fixed(element.type_id) &&
                type_is_integral(element.type_id)) {
                emit_val = (uint16_t)((int16_t)emit_val << 4);
            }
            emit_initializer_value(counter, last_is_char, 0, emit_val);
        }
    }
    return 1;
}

uint16_t far_parse_struct_initializer_fields(uint8_t struct_type_id) MYCC {
    uint16_t counter = 0;
    uint16_t elementcount = 0;
    int struct_id = (int)type_get_struct_id(struct_type_id) - 1;
    int field_count = get_field_count(struct_id);
    int field_idx = 0;
    uint8_t last_is_char = 0;

    while (field_idx < field_count && tok != tokRBrace && tok != tokEOS) {
        FIELDINFO fi = get_struct_field(struct_id, field_idx++);
        uint8_t field_type_id = fi.type_id;

        /* struct field parsing: struct=%d field=%d type=%d token='%s' */

        if (type_is_struct(field_type_id) && tok == tokLBrace) {
            get_token();
            if (counter) emit_nl();
            counter = 0;
            far_parse_struct_initializer_fields(field_type_id);
            expect_RBrace();
            ++elementcount;
        } else if (parse_initializer_item(field_type_id, &counter, &last_is_char, 1)) {
            ++elementcount;
        }

        if (tok == tokRBrace) break;
        if (tok != tokComma) { error(errExpected_c, ','); break; }
        get_token();
    }

    while (field_idx < field_count) {
        FIELDINFO fi = get_struct_field(struct_id, field_idx++);
        if (counter) emit_nl();
        counter = 0;
        emit_zero_for_type(fi.type_id, &counter, &last_is_char);
        ++elementcount;
    }

    if (counter) emit_nl();
    if (tok != tokRBrace && tok != tokEOS) error(errTypeError);
    return elementcount;
}

uint16_t far_parse_brace_initializer_elements(uint8_t element_type_id, uint16_t expected_count) MYCC {
    uint16_t counter = 0;
    uint16_t elementcount = 0;
    uint8_t last_is_char = type_size(element_type_id) == 1;

    if (type_is_struct(element_type_id)) {
        /* Array-of-structs: each element is a brace-enclosed struct { ... } */
        while (tok != tokRBrace && tok != tokEOS) {
            if (tok != tokLBrace) { error(errExpected_c, '{'); break; }
            get_token();
            if (elementcount) emit_nl();
            counter = 0;
            far_parse_struct_initializer_fields(element_type_id);
            expect_RBrace();
            ++elementcount;
            if (tok == tokRBrace) break;
            if (tok != tokComma) { error(errExpected_c, ','); break; }
            get_token();
        }
        if (counter) emit_nl();
        while (expected_count > 0 && elementcount < expected_count) {
            emit_zero_for_type(element_type_id, &counter, &last_is_char);
            ++elementcount;
        }
        if (expected_count > 0 && elementcount > expected_count) error(errTypeError);
        return elementcount;
    }

    while (tok != tokRBrace && tok != tokEOS) {
        if (expected_count > 0 && elementcount >= expected_count) {
            error(errTypeError);
            break;
        }
        if (parse_initializer_item(element_type_id, &counter, &last_is_char, 0)) {
            ++elementcount;
        }
        if (tok == tokRBrace) break;
        if (tok != tokComma) { error(errExpected_c, ','); break; }
        get_token(); // skip ','
    }
    while (expected_count > 0 && elementcount < expected_count) {
        emit_zero_for_type(element_type_id, &counter, &last_is_char);
        ++elementcount;
    }
    if (expected_count > 0 && elementcount > expected_count) error(errTypeError);
    if (counter) emit_nl();
    return elementcount;
}
