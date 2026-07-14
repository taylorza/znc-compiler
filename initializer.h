#ifndef INITIALIZER_H_
#define INITIALIZER_H_

uint16_t parse_concat_string_literal(void) MYCC;
uint16_t parse_brace_initializer_elements(uint8_t element_type_id, uint16_t expected_count) MYCC;
uint16_t parse_struct_initializer_fields(uint8_t struct_type_id) MYCC;

#endif //INITIALIZER_H_
