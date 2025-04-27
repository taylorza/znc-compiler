#ifndef ERROR_H_
#define ERROR_H_

typedef enum ERROR {
    errSyntax,
    errExpected,
    errTooLong,
    errTooManySymbols,
    errAlreadyDefined,
    errNotDefined_s,
    errNotlvalue,
    errTypeError,
    errFileError,
    errArgMismatch,
    errDeclMismatch,
} ERROR;

extern int errcnt;
void error(ERROR err, ...);

#endif //ERROR_H_