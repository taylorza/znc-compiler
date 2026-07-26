#include "znc.h"

uint8_t old_speed = 255;
uint8_t old_border = 7;

void show_banner(void) MYCC;
void show_help(const char *msg) MYCC;
void cleanup(void) MYCC;

#include "compiler.h"

int main(unsigned int argc, unsigned char **argv) { 
    show_banner();

    /* Very lightweight option parsing: accept any number of flags before
     * the positional arguments. Supported flags:
     *   -dfe    enable dead-function-elimination marker emission
     */
    const char *src_file = NULL;
    char *out_file_arg = NULL;

    for (unsigned int i = 1; i < argc; ++i) {
        const char *a = (const char*)argv[i];
        if (a[0] == '-') {
            if (strcmp(a, "-dfe") == 0) {
                dfe_enabled = 1;
                continue;
            }
            show_help("unknown option\n");
            return 0;
        }
        if (!src_file) src_file = a;
        else if (!out_file_arg) out_file_arg = (char*)a;
        else {
            show_help("too many arguments\n");
            return 0;
        }
    }

    if (!src_file) {
        show_help("expected source and output\n");
        return 0;
    }
#ifdef __ZXNEXT 
    old_speed = ZXN_READ_REG(0x07) & 0x03;
    old_border = ((*(uint8_t*)(0x5c48)) & 0b00111000) >> 3;
    ZXN_NEXTREG(0x07, 3);
#endif

    atexit(cleanup);

    if (!out_file_arg) {
        strcpy(outfilename, src_file);
        set_file_ext(outfilename, "asm");
    } else {
        strncpy(outfilename, out_file_arg, MAX_FILENAME_LEN);
        outfilename[MAX_FILENAME_LEN - 1] = '\0';
    }

    compile(src_file, outfilename);

    return 0;
}

void cleanup(void) MYCC {
    src_closeall();
    asm_close();
#ifdef __ZXNEXT
    zx_border(old_border);
    ZXN_WRITE_REG(0x07, old_speed);
#endif
}

void show_banner(void) MYCC {
    printf("ZNC Compiler v0.6d-beta (c)2026\n%s %s\n",__DATE__, __TIME__);
}

void show_help(const char *msg) MYCC {    
    printf(
        "%s\n\n"
        "Usage:\n"
        "znc <src> [<out>]\n\n", 
        msg
    );
}