#include "znc.h"

void show_banner(void);
void show_help(const char *msg);

int main(unsigned int argc, unsigned char **argv) { 
    show_banner();
    if (argc < 3) {
        show_help("source and output file expected\n");
        return 0;
    }    
    
    compile(argv[1], argv[2]);

    return 0;
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