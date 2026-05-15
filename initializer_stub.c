#include "znc.h"
#include "farcall.h"
#include "initializer.h"
#include "shared.h"

uint16_t far_parse_concat_string_literal(void) MYCC;
uint16_t far_parse_brace_initializer_elements(uint8_t element_type_id) MYCC;

uint16_t parse_concat_string_literal(void) MYCC {
    PROLOG(42)
    uint16_t r = far_parse_concat_string_literal();
    EPILOG_RETURN(r)
}

uint16_t parse_brace_initializer_elements(uint8_t element_type_id) MYCC {
    PROLOG(42)
    uint16_t r = far_parse_brace_initializer_elements(element_type_id);
    EPILOG_RETURN(r)
}
