#ifndef TOKENS_H_
#define TOKENS_H_

typedef enum TOKEN_TYPE {
    ttError, ttDelimiter, ttIdent, ttNumber, ttKeyword, ttString, ttBlock
} TOKEN_TYPE;

typedef enum TOKEN {
    tokNone, tokConst,
    tokHashIf, tokHashIfDef, tokHashIfNDef, tokHashElif, tokHashElse, tokHashEndif,
    tokArg, tokVoid, tokChar, tokByte, tokInt, tokFixed, tokIf, tokElse, tokFor, tokWhile,
    tokSwitch, tokCase, tokDefault,
    tokReturn, tokBreak, tokContinue, tokExit,
    tokIn, tokOut, tokNextReg, tokReadReg,
    tokVaStart, tokVaArg, tokVaEnd,
    tokAbs, tokPutc, tokPuts,
    tokAsm, tokInclude,
    tokIdent, tokExtern, tokZncCall,
    tokMake, tokDot, tokNex, tokRaw, tokSetStack, tokOrg, tokBank,
    
    tokNumber,      // number
    tokFixedLit,    // fixed-point literal (e.g. 3.14) - value pre-computed in 12.4 format in intval
    tokString,      // string
    tokStruct,      // 'struct' keyword
    tokEnum,        // 'enum' keyword
    tokDelegate,    // 'delegate' keyword
    tokMember,      // '.' member operator

    tokSemi,        // ;
    tokLParen,      // (
    tokRParen,      // )
    tokLBrace,      // {
    tokRBrace,      // }
    tokLBrack,      // [
    tokRBrack,      // ]
    tokComma,       // ,
    tokQuote,       // "
    tokSingleQuote, // '

    tokAssign,      // =
    tokAddAssign,   // +=
    tokSubAssign,   // -=
    tokMulAssign,   // *=
    tokDivAssign,   // /=
    tokModAssign,   // %=
    tokOrAssign,    // |=
    tokXorAssign,   // ^=
    tokAndAssign,   // &=
    tokShlAssign,   // <<=
    tokShrAssign,   // >>=

    tokInc,         // ++
    tokDec,         // --

    tokPlus,        // +
    tokMinus,       // -
    tokStar,        // *
    tokDiv,         // /
    tokMod,         // %
    tokEq,          // ==
    tokNot,         // !
    tokNeq,         // !=
    tokLt,          // <
    tokLeq,         // <=
    tokGt,          // >
    tokGeq,         // >=

    tokOr,          // ||
    tokAnd,         // &&

    tokBitNot,      // ~
    tokBitOr,       // |
    tokBitXor,      // ^
    tokBitAnd,      // &
    tokAmp = tokBitAnd, // &
    
    tokShl,         // <<
    tokShr,         // >>

    tokCond,        // ?
    tokColon,       // :
    tokEllipsis,    // ...

    tokEOS          // End of source
} TOKEN;

#endif //TOKENS_H_