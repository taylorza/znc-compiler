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
    // Sign extend A
    {"ccsxt",
        " LD L,A"NLS
        " RLCA"NLS
        " SBC A,A"NLS
        " LD H,A",
    NULL,
    FLAG_RTL_NONE
    },
    // Bitwise OR
    {"ccor",
        " LD A,L"NLS
        " OR E"NLS
        " LD L,A"NLS
        " LD A,H"NLS
        " OR D"NLS
        " LD H,A",
    NULL,
    FLAG_RTL_NONE
    },
    // Bitwise XOR
    {"ccxor",
        " LD A,L"NLS
        " XOR E"NLS
        " LD L,A"NLS
        " LD A,H"NLS
        " XOR D"NLS
        " LD H,A",
    NULL,
    FLAG_RTL_NONE
    },
    // Bitwise AND
{"ccand",
        " LD A,L"NLS
        " AND E"NLS
        " LD L,A"NLS
        " LD A,H"NLS
        " AND D"NLS
        " LD H,A",
    NULL,
    FLAG_RTL_NONE
    },
    // Equality test
    {"cceq",
        " CALL cccmp"NLS
        " RET Z"NLS
        " DEC HL",
    "cccmp",
    FLAG_RTL_NONE
    },
    // Not equal test
    {"ccne",
        " CALL cccmp"NLS
        " RET NZ"NLS
        " DEC HL",
    "cccmp",
     FLAG_RTL_NONE
    },
    // Greater than
    {"ccgt",
        " EX DE,HL"NLS
        " JP cclt",
    "cclt",
    FLAG_RTL_NONE
    },
    // Less than equal
    {"ccle",
        " CALL cccmp"NLS
        " RET Z"NLS
        " RET C"NLS
        " DEC HL",
    "cccmp",
    FLAG_RTL_NONE
    },
    // Greater than equal
    {"ccge",
        " CALL cccmp"NLS
        " RET NC"NLS
        " DEC HL",
    "cccmp",
    FLAG_RTL_NONE
    },

    // DE < HL
    {"cclt",
        " CALL cccmp"NLS
        " RET C"NLS
        " DEC HL",
    "cccmp",
    FLAG_RTL_NONE
    },

    // Compare HL, DE
    {"cccmp",
        " LD A,E"NLS
        " SUB L"NLS
        " LD E,A"NLS
        " LD A,D"NLS
        " SBC A,H"NLS
        " LD HL,1"NLS
        " JP M,cccmp1"NLS
        " OR E"NLS
        " RET"NLS
        "cccmp1"NLS
        " OR E"NLS
        " SCF",        
    NULL,
    FLAG_RTL_NONE
    },
    // Unsigned HL>=DE
    {"ccuge",
        " CALL ccucmp"NLS
        " RET NC"NLS
        " DEC HL",
    "ccucmp",
    FLAG_RTL_NONE
    },
    // Unsigned HL<DE
    {"ccult",
        " CALL ccucmp"NLS
        " RET C"NLS
        " DEC HL",
    "ccucmp",
    FLAG_RTL_NONE
    },
    // Unsigned HL>DE
    {"ccugt",
        " EX DE,HL"NLS
        " JP ccult"NLS
    "ccult",
    FLAG_RTL_NONE
    },    
    // Unsigned HL<=DE
    {"ccule",
        " CALL ccucmp"NLS
        " RET Z"NLS
        " RET C"NLS
        " DEC HL",
    "ccucmp",
    FLAG_RTL_NONE
    },
    // Unsigned compare HL, DE
    {"ccucmp",
        " LD A,D"NLS
        " CP H"NLS
        " JP NZ,ccucmp1"NLS
        " LD A,E"NLS
        " CP L"NLS
        "ccucmp1"NLS
        " LD HL,1",
    NULL,
    FLAG_RTL_NONE
    },
    // HL = -HL
    {"ccneg",
        " CALL cccom"NLS
        " INC HL",
    "cccom",
    FLAG_RTL_INLINE
    },
    // HL = ~HL
    {"cccom",
        " LD A,H"NLS
        " CPL"NLS
        " LD H,A"NLS
        " LD A,L"NLS
        " CPL"NLS
        " LD L,A",
    NULL,
    FLAG_RTL_NONE
    },
    // HL = !HL
    { "ccnot",
        " LD A,H"NLS
        " OR L"NLS
        " JR Z,.0"NLS
        " LD HL,0"NLS
        " RET"NLS
        ".0"NLS
        " INC L",
    NULL,
    FLAG_RTL_NONE
    },
    // HL = HL * DE
    {"ccmult",
        " LD C,E"NLS
        " LD E,L"NLS
        " MUL D,E"NLS
        " LD A,E"NLS
        " LD E,C"NLS
        " LD D,H"NLS
        " MUL D,E"NLS
        " ADD A,E"NLS
        " LD E,C"NLS
        " LD D,L"NLS
        " MUL D,E"NLS
        " ADD A,D"NLS
        " LD H,A"NLS
        " LD L,E",
    NULL,
    FLAG_RTL_NONE
    },
    // HL = HL / DE
    // DE = HL % DE
    {"ccdiv",
        " EX DE, HL"NLS
        " LD A, H"NLS
        " LD C, L"NLS
        " LD HL, 0"NLS
        " LD B, 16"NLS
        ".LOOP"NLS
        " RL C"NLS
        " RLA"NLS
        " ADC HL, HL"NLS
        " SBC HL, DE"NLS
        " JP NC, .NORESTR"NLS
        " ADD HL, DE"NLS
        ".NORESTR"NLS
        " CCF"NLS
        " DJNZ .LOOP"NLS
        " RL C"NLS
        " RLA"NLS
        " EX DE, HL"NLS
        " LD H, A"NLS
        " LD L, C",
        NULL,
        FLAG_RTL_NONE
    },
    { "putc",
        "  ld a,l"NLS
        "  rst 16",
        NULL,
        FLAG_RTL_INLINE
    },
    { "puts",
        " LD A, (HL)"NLS
        " OR A"NLS
        " JR Z, .DONE"NLS
        " RST $10"NLS
        " INC HL"NLS
        " JR PUTS"NLS
        ".DONE",
        NULL,
        FLAG_RTL_NONE
    },
    { "ccpstr",
        " PUSH HL"NLS
        " LD BC,0"NLS
        " XOR A"NLS
        " CPIR"NLS
        " JR NZ,.L2"NLS
        " DEC HL"NLS
        " DEC HL"NLS
        " LD A,(HL)"NLS
        " OR $80"NLS
        " LD (HL),A"NLS
        ".L2"NLS
        " POP HL",
        NULL,
        FLAG_RTL_NONE
    },
    { "ldcmdln",
        " LD DE,_ARGS"NLS
        " LD BC,79"NLS
        ".L1"NLS
        " LD A,(HL)"NLS
        " OR A"NLS
        " JR Z,.L2"NLS
        " CP \":\""NLS
        " JR Z,.L2"NLS
        " CP 13"NLS
        " JR Z,.L2"NLS
        " LD(DE),A"NLS
        " INC HL"NLS
        " INC DE"NLS
        " DEC BC"NLS
        " LD A,B"NLS
        " OR C"NLS
        " JR NZ,.L1"NLS
        ".L2"NLS
        " XOR A"NLS
        " LD(DE), A",
        NULL,
        FLAG_RTL_NONE
    }
};

void emit_code(const char* code) MYCC {
    const char* p = code;
    while (*p) {
        emit_ch(*p++);
    }
    emit_nl();
}

uint8_t inc_rtl(const char* fn) {
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
                    inc_rtl(depname);
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

void dump_rtl(void) {
#if __ZXNEXT
    //printf("\nGen RTL - %d", sizeof(rtltbl) / sizeof(RTLREC));
#endif
    for (uint8_t i = 0; i < sizeof(rtltbl) / sizeof(RTLREC); ++i) {
        RTLREC *rtl = &rtltbl[i];                
        if (rtl->flags & FLAG_RTL_INCLUDE) {
#if __ZXNEXT
            //printf("\n  %s", rtl->name);
#endif
            emit_nl();
            emit_str(rtl->name); emit_nl();
            emit_code(rtl->code);
            emit_ret();
        }     
    }
}




