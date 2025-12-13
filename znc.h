#define MAX_FILENAME_LEN    64      // max filename length
#define MAX_IDENT_LEN       14      // max identifier length
#define MAX_NEST_DEPTH      5       // max nesting depth
#define MAX_SYMBOLS         500     // max number of symbols
#define MAX_READ_BUF        16      // max file read buffer size
#define MAX_WRITE_BUF       16      // max file write buffer size
#define MAX_STRTBL_SIZE     2048    // max string table size
#define MAX_CASE            128     // max case statements per switch

#ifdef __ZXNEXT
#define NL '\r'
#else
#define NL '\n'
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#ifdef __ZXNEXT
#include <z80.h>
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#endif

#ifdef __SDCC
#define MYCC __sdcccall(1)
#else //!__SDCC
#define MYCC
#endif

#include "dataarea.h"
#include "util.h"
#include "tokens.h"
#include "error.h"
#include "strtbl.h"
#include "sym.h"
#include "scanner.h"
#include "codegen.h"
#include "rtl.h"
#include "expr.h"
#include "compiler.h"
