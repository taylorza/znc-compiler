#ifndef ERROR_H_
#define ERROR_H_

typedef enum ERROR {
    errSyntax,
    errExpected_c,
    errTooLong,
    errTooManySymbols,
    errAlreadyDefined_s,
    errNotDefined_s,
    errNotlvalue,
    errTypeError,
    errFileError,
    errArgMismatch,
    errDeclMismatch,
    errInvalid_s,
    errExpected_s,
} ERROR;

extern int errcnt;
void error(ERROR err, ...);
void error_expect_const(void);

#endif //ERROR_H_