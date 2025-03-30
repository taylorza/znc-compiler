#include "znc.h"

uint8_t oldspeed = 255;

void show_banner(void);
void show_help(const char *msg);
void cleanup(void);

int main(unsigned int argc, unsigned char **argv) { 
    show_banner();
    if (argc < 3) {
        show_help("source and output file expected\n");
        return 0;
    }    
#ifdef __ZXNEXT 
    oldspeed = ZXN_READ_REG(0x07) & 0x03;
    ZXN_NEXTREG(0x07, 3);
#endif

    atexit(cleanup);
    compile(argv[1], argv[2]);

    return 0;
}

void cleanup(void) {
    src_closeall();
    asm_close();

#ifdef __ZXNEXT
    ZXN_WRITE_REG(0x07, oldspeed);
#endif
}

void show_banner(void) {
    printf("ZNC Compiler v0.1 (c)2025\n\n");
}

void show_help(const char *msg) {    
    printf(
        "%s\n\n"
        "Usage:\n"
        "znc <srcfile> <outfile>\n\n", 
        msg
    );
}