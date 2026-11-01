#include "znc.h"

int main(void) {
    asm_open(NULL);
    emit_instrln("org $8000");
    emit_instrln("ld sp,$ff00");
    emit_ld_immed_n(1);
    emit_add_hl_small(-32768);
    emit_instrln("ld ($e000),hl");
    emit_ld_immed_n(0x5432);
    emit_ldde_immed_n(0x1234);
    emit_mulDE2();
    emit_instrln("ld ($e002),de");
    emit_clean_stack(1);
    emit_instrln("ld ($e004),sp");
    emit_mul_const_optimized(13);
    emit_instrln("ld ($e006),hl");
    emit_instrln("ld ($e008),de");
    emit_mul_const_optimized(256);
    emit_instrln("ld ($e00a),hl");
    emit_instrln("ld ($e00c),de");
    emit_instrln("jp $ffff");

    emit_strln("mul3:");
    emit_mul_const_optimized(3);
    emit_strln("mul3_end:");
    emit_strln("mul5:");
    emit_mul_const_optimized(5);
    emit_strln("mul5_end:");
    emit_strln("mul256:");
    emit_mul_const_optimized(256);
    emit_strln("mul256_end:");
    emit_strln("include \"test.rtl\"");
    asm_close();
    asm_close();
    dump_rtl("test.rtl");
    return 0;
}
