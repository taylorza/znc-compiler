#ifndef ZNC_H
#define ZNC_H

#define MAX_FILENAME_LEN    64      // max filename length
#define MAX_IDENT_LEN       14      // max identifier length
#define MAX_NEST_DEPTH      5       // max nesting depth
#define MAX_SYMBOLS         650     // max number of symbols
#define MAX_READ_BUF        16      // max file read buffer size
#define MAX_WRITE_BUF       16      // max file write buffer size
#define MAX_STRTBL_SIZE     15660   // max string table size
#define MAX_CASE            128     // max case statements per switch

#define MAX_TYPES 255               // Type table - max 255 entries (uint8_t limit)
#define MAX_FUNC_ARGS 8             // max function arguments (excluding varargs)
#define MAX_SIGNATURES 255          // max number of unique function signatures (uint8_t limit)

#define NO_LABEL            0xFFFF  // no label defined

#ifdef __ZXNEXT
#define NL '\r'
#else
#define NL '\n'
#endif

/* Debug tracing */
#define ZNC_DEBUG 0

#if ZNC_DEBUG
#define DPRINT(...) printf(__VA_ARGS__)
#else
#define DPRINT(...) ((void)0)
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
#else
#include "zxn.h"
#endif

#ifdef __SDCC
#define MYCC __sdcccall(1)
#else //!__SDCC
#define MYCC
#endif

#include "dataarea.h"
#include "shared.h"
#include "util.h"
#include "tokens.h"
#include "error.h"
#include "strtbl.h"
#include "identtbl.h"
#include "type.h"
#include "sym.h"
#include "farcall.h"
#include "scanner.h"
#include "codegen.h"
#include "rtl.h"
#include "expr.h"
#include "initializer.h"
#include "compiler.h"

#endif // ZNC_H