#ifndef ERROR_H_
#define ERROR_H_

typedef enum ERROR {
    errSyntax,
    errExpectQuote,
    errExpectLParen,
    errExpectRParen,
    errExpectLBrace,
    errExpectRBrace,
    errExpectLBrack,
    errExpectRBrack,
    errExpectSemi,
    errExpectComma,
    errTooLong,
    errTooManySymbols,
    errAlreadyDefined,
    errNotDefined,
    errNotlvalue,
    errTypeError,
    errFileError,
    errArgCountMismatch,
} ERROR;

extern int errcnt;
void error(ERROR err);

#endif //ERROR_H_