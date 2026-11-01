#include "znc.h"

void switch_bank67(uint8_t bank, uint8_t *old_page0, uint8_t *old_page1) MYCC {
    (void)bank;
    *old_page0 = 0;
    *old_page1 = 0;
}

void restore_bank67(uint8_t old_page0, uint8_t old_page1) MYCC {
    (void)old_page0;
    (void)old_page1;
}
