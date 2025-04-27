#include "znc.h"

uint8_t old_speed = 255;
uint8_t old_border = 7;

void show_banner(void);
void show_help(const char *msg);
void cleanup(void);

int main(unsigned int argc, unsigned char **argv) { 
    show_banner();
    if (argc < 2 || argc > 3) {
        show_help("expected source and output\n");
        return 0;
    }    
#ifdef __ZXNEXT 
    old_speed = ZXN_READ_REG(0x07) & 0x03;
    old_border = ((*(uint8_t*)(0x5c48)) & 0b00111000) >> 3;
    ZXN_NEXTREG(0x07, 3);
#endif

    atexit(cleanup);
    const char *src_file = argv[1];
    const char *out_file;

    if (argc == 2) {
        strcpy(outfilename, src_file);
        set_file_ext(outfilename, "asm");
        out_file = outfilename;
    } else {
        out_file = argv[2];
    }
    compile(src_file, out_file);

    return 0;
}

void cleanup(void) {
    src_closeall();
    asm_close();

#ifdef __ZXNEXT
    zx_border(old_border);
    ZXN_WRITE_REG(0x07, old_speed);
#endif
}

void show_banner(void) {
    printf("ZNC Compiler v0.1 (c)2025\n\n");
}

void show_help(const char *msg) {    
    printf(
        "%s\n\n"
        "Usage:\n"
        "znc <src> [<out>]\n\n", 
        msg
    );
}