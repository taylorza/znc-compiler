#ifndef TOKENS_H_
#define TOKENS_H_

typedef enum TOKEN_TYPE {
    ttError, ttDelimiter, ttIdent, ttNumber, ttKeyword, ttString, ttBlock
} TOKEN_TYPE;

typedef enum TOKEN {
    tokNone, tokConst,
    tokHashIf, tokHashIfDef, tokHashIfNDef, tokHashElse, tokHashEndif,
    tokArg, tokVoid, tokChar, tokInt, tokIf, tokElse, tokFor, tokWhile,
    tokReturn, tokBreak, tokContinue,
    tokIn, tokOut, tokNextReg, tokReadReg,
    tokPutc, tokPuts,
    tokAsm, tokInclude,
    tokIdent, 
    tokMake, tokDot, tokNex, tokRaw, tokSetStack, tokOrg, tokBank,
    
    tokNumber,      // number
    tokString,      // string

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

    tokEOS          // End of source
} TOKEN;

#endif //TOKENS_H_