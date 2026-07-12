#include "znc.h"

#ifdef __ZXNEXT
#define NLS "\r"
#else
#define NLS "\n"
#endif

typedef struct RTLREC {
    const char *name;
    const char *code;
    const char *deps;
    uint8_t flags;
} RTLREC;

RTLREC rtltbl[] = {
    // Far call to function in another bank
    {"ccfarcall",
#include "RTL/generated/ccfarcall.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // Call exit function at __exitfn, if set.  Otherwise, just return.
    {"ccexit",
#include "RTL/generated/ccexit.inc"
    NULL,
    FLAG_RTL_INLINE
    },
    // Sign extend A
    {"ccsxt",
#include "RTL/generated/ccsxt.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // Absolute value of HL (HL = abs(HL))
    {"ccabs",
#include "RTL/generated/ccabs.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // Bitwise OR
    {"ccor",
#include "RTL/generated/ccor.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // Bitwise XOR
    {"ccxor",
#include "RTL/generated/ccxor.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // Bitwise AND
{"ccand",
#include "RTL/generated/ccand.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // Equality test
    {"cceq",
#include "RTL/generated/cceq.inc"
    "cccmp",
    FLAG_RTL_NONE
    },
    // Not equal test
    {"ccne",
#include "RTL/generated/ccne.inc"
    "cccmp",
     FLAG_RTL_NONE
    },
    // Greater than
    {"ccgt",
#include "RTL/generated/ccgt.inc"
    "cclt",
    FLAG_RTL_NONE
    },
    // Less than equal
    {"ccle",
#include "RTL/generated/ccle.inc"
    "cccmp",
    FLAG_RTL_NONE
    },
    // Greater than equal
    {"ccge",
#include "RTL/generated/ccge.inc"
    "cccmp",
    FLAG_RTL_NONE
    },

    // DE < HL
    {"cclt",
#include "RTL/generated/cclt.inc"
    "cccmp",
    FLAG_RTL_NONE
    },

    // Compare HL, DE
    {"cccmp",
#include "RTL/generated/cccmp.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // Unsigned HL>=DE
    {"ccuge",
#include "RTL/generated/ccuge.inc"
    "ccucmp",
    FLAG_RTL_NONE
    },
    // Unsigned HL<DE
    {"ccult",
#include "RTL/generated/ccult.inc"
    "ccucmp",
    FLAG_RTL_NONE
    },
    // Unsigned HL>DE
    {"ccugt",
#include "RTL/generated/ccugt.inc"
    "ccult",
    FLAG_RTL_NONE
    },    
    // Unsigned HL<=DE
    {"ccule",
#include "RTL/generated/ccule.inc"
    "ccucmp",
    FLAG_RTL_NONE
    },
    // Unsigned compare HL, DE
    {"ccucmp",
#include "RTL/generated/ccucmp.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // HL = -HL
    {"ccneg",
#include "RTL/generated/ccneg.inc"
    "cccom",
    FLAG_RTL_INLINE
    },
    // HL = ~HL
    {"cccom",
#include "RTL/generated/cccom.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // HL = !HL
    { "ccnot",
#include "RTL/generated/ccnot.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // HL = HL * DE
    {"ccmult",
#include "RTL/generated/ccmult.inc"
    NULL,
    FLAG_RTL_NONE
    },
    // HL = DE / HL
    // DE = DE % HL
    {"ccdiv",
#include "RTL/generated/ccdiv.inc"
        NULL,
        FLAG_RTL_NONE
    },
    // int/char 2 Fixed-point HL = HL << 4
    {"ccint2fx",    
#include "RTL/generated/ccint2fx.inc"
        NULL,
        FLAG_RTL_NONE
    },
    // Fixed-point to int/char: HL = HL >> 4
    {"ccfx2int",    
#include "RTL/generated/ccfx2int.inc"
        NULL,
        FLAG_RTL_NONE
    },
    // Fixed-point multiply: DE * HL -> HL  (12.4 format, uses ccmult32)
    {"ccfxmul",
#include "RTL/generated/ccfxmul.inc"
        NULL,
        FLAG_RTL_NONE
    },
    // Fixed-point divide: DE / HL -> HL  (12.4 format, uses ccdiv)
    {"ccfxdiv",
#include "RTL/generated/ccfxdiv.inc"
        NULL,
        FLAG_RTL_NONE
    },
    { "putc",
#include "RTL/generated/putc.inc"
        NULL,
        FLAG_RTL_INLINE
    },
    { "puts",
#include "RTL/generated/puts.inc"
        NULL,
        FLAG_RTL_NONE
    },
    { "ccpstr",
#include "RTL/generated/ccpstr.inc"
        NULL,
        FLAG_RTL_NONE
    },
    { "ldcmdln",
#include "RTL/generated/ldcmdln.inc"
        NULL,
        FLAG_RTL_NONE
    },
    { "ccvaarg",
#include "RTL/generated/ccvaarg.inc"
        NULL,
        FLAG_RTL_NONE
    },
    { "ccvafirst",
#include "RTL/generated/ccvafirst.inc"
        NULL,
        FLAG_RTL_NONE
    },
    { "ccvafixed",
#include "RTL/generated/ccvafixed.inc"
        NULL,
        FLAG_RTL_NONE
    },
    { "ccvafixedaddr",
#include "RTL/generated/ccvafixedaddr.inc"
        NULL,
        FLAG_RTL_NONE
    },
        {
            "ccswitch",
#include "RTL/generated/ccswitch.inc"
                NULL,
                FLAG_RTL_NONE
        }
};

void emit_code(const char* code) {
    const char* p = code;
    while (*p) {
        if (*p == '\n') { ++p; emit_nl(); }
        else emit_ch(*p++);
    }
    emit_nl();
}

uint8_t far_inc_rtl(const char* fn) MYCC {
    char depname[16] = { 0 };
    for (uint8_t i = 0; i < sizeof(rtltbl) / sizeof(RTLREC); ++i) {
        RTLREC *const rtl = &rtltbl[i];
        if (strcmp(fn, rtl->name) == 0) {
            if (rtl->flags & FLAG_RTL_INCLUDE) return 1;
            if (rtl->deps) {
                const char* curr = rtl->deps;
                while (*curr) {
                    char* p = &depname[0];
                    while (*curr != ',' && *curr != '\0') {
                        *p++ = *curr++;
                    }
                    if (*curr == ',') ++curr;
                    *p = '\0';
                    far_inc_rtl(depname);
                }
            }
            if (rtl->flags & FLAG_RTL_INLINE) {
                emit_code(rtl->code);
            } else {
                rtl->flags |= FLAG_RTL_INCLUDE;                
            }              
            return rtl->flags;
        }
    }
    error(errNotDefined_s, fn);
    return 0;
}

#if __ZXNEXT
#  define F_OPEN(name) esx_f_open(name, ESX_MODE_OPEN_CREAT_TRUNC | ESX_MODE_W)
#  define F_WRITE(f, buf, len) esx_f_write(f, buf, len)
#  define F_CLOSE(f) esx_f_close(f)
#else
#  define F_OPEN(name) fopen(name, "wb")
#  define F_WRITE(f, buf, len) fwrite(buf, 1, len, f)
#  define F_CLOSE(f) do { if ((f) && (f) != stdout) fclose(f); } while(0)
#endif

void far_dump_rtl(char *outfilename) MYCC {
    errno = 0;
#if __ZXNEXT
    uint8_t fh;
    fh = F_OPEN(outfilename);
    if (errno) {
        printf("can't create '%s'", outfilename);
        return;
    }
#else
    FILE* fh;
    fh = F_OPEN(outfilename);
    if (errno) {
        printf("can't create '%s'", outfilename);
        return;
    }
#endif
    F_WRITE(fh, ";#OPT_OFF\n", 10);
    for (uint8_t i = 0; i < sizeof(rtltbl) / sizeof(RTLREC); ++i) {
        RTLREC *rtl = &rtltbl[i];    
        //printf("\n  %s (%s) (%d)", rtl->name, rtl->deps, rtl->flags);            
        if (rtl->flags & FLAG_RTL_INCLUDE) {
#if __ZXNEXT
            zx_border(i & 1);
#endif            
            F_WRITE(fh, rtl->name, strlen(rtl->name));
            F_WRITE(fh, "\n", 1);
            F_WRITE(fh, rtl->code, strlen(rtl->code));
            F_WRITE(fh, " ret\n", 5);
        }     
    }
    F_WRITE(fh, ";#OPT_ON\n", 9);
    F_CLOSE(fh);
}




