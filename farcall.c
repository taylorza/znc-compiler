#include "farcall.h"

extern unsigned char _z_page_table[];

void switch_bank67(uint8_t bank, uint8_t *old_page0, uint8_t *old_page1) MYCC {
    *old_page0 = ZXN_READ_MMU6();
    *old_page1 = ZXN_READ_MMU7();
    ZXN_WRITE_MMU6(_z_page_table[bank<<1]);
    ZXN_WRITE_MMU7(_z_page_table[(bank<<1)+1]);
}

void restore_bank67(uint8_t old_page0, uint8_t old_page1) MYCC {
    ZXN_WRITE_MMU6(old_page0);
    ZXN_WRITE_MMU7(old_page1);
}
