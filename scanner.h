#ifndef SCANNER_H_
#define SCANNER_H_

#define MAX_NEST_FILE 4
#define MAX_STR_LEN 80

typedef struct SOURCEPOS {
    char filename[MAX_FILENAME_LEN];
    char buf[MAX_READ_BUF];
    uint16_t line;
    uint8_t col;
    uint8_t ofs;
    uint8_t len;
#ifdef __ZXNEXT
    uint8_t handle;
#else
    FILE* handle;
#endif
    
} SOURCEPOS;

extern uint8_t fileid;
extern SOURCEPOS loc[MAX_NEST_FILE];

extern char token[MAX_STR_LEN+1];
extern uint16_t token_line;
extern uint8_t token_col;
extern TOKEN_TYPE token_type;
extern uint8_t token_length;
extern TOKEN tok;
extern int16_t intval;

uint8_t src_open(const char *filename) MYCC;
void src_close(void) MYCC;
void src_closeall(void) MYCC; 

TOKEN_TYPE get_token(void) MYCC;
void expect(TOKEN t, char ch);

void expect_colon(void) MYCC;
void expect_semi(void) MYCC;
void expect_comma(void) MYCC;
void expect_LParen(void) MYCC;
void expect_RParen(void) MYCC;
void expect_LBrace(void) MYCC;
void expect_RBrace(void) MYCC;

#endif //SCANNER_H_