## ZNC - A native compiler for the ZX Spectrum Next

ZNC is a language and compiler for the ZX Spectrum Next. The language is closely modeled after the C programming language, but does deviate in some cases where it either made the language easier to parse or in other cases where I preferred an alternate syntax.

The compiler is a non-optimizing single pass compiler, while the generated code is not the fastest or the smallest it should outperform non-compiled languages.

In this language:

**Build Directives.** A program may start with optional build directives:

* The `make` directive configures the build mode (with options "dot", "nex", or "raw") and optionally a string specifying the name an location of the resulting binary.
* The org directive sets the origin (memory address) of the code.

**Top‐Level Items.** Programs are a sequence of top‑level items (declarations and/or statements). Unlike traditional C, top‑level statements are allowed. However, if a top‑level statement calls a function, the function must have been declared before its use—either via a full definition or via a prototype.

**Declarations.** There are variable declarations, function definitions, and function prototypes. Variable types include scalar types (`char`, `byte`, `int`), arrays (e.g. `int[10]`), and pointers (e.g. `int*`).

**Control Structures.** You’ll find the usual control constructs: `if`–`else`, `while`, and `for` loops, as well as statements such as `break`, `continue`, and `return`.

**Inline Assembly.** A special __asm__ block can be used to embed raw Z80N assembly code inside functions.

**I/O Primitives.** For basic output, built‑in functions `putc()` and `puts()` are provided.

**Banks.** A bank construct allows you to group a statement block into a memory bank

## Installing the tools
The compiler includes a suite of tools:

* [ZED - A text editor](https://github.com/taylorza/zed)
* [ZNC - The compiler](https://github.com/taylorza/znc-compiler)
* [ZOPT - A peephole optimizer](https://github.com/taylorza/zopt)
* [ASM - Assembler](https://taylorza.itch.io/nextbasic-inline-assembler)

[**Download: ZNC COMPILER**](https://taylorza.itch.io/znc-compiler)

Start by copying the downloaded zip file to the root directory of your NextZXOS SD Card. Then, from the NextZXOS command line, enter:

```
.unzip -o znc.zip
```

Once the extraction is complete, four binary components will be installed as DOT commands, and a new ZDEV directory will appear in the root. Navigate to this directory, where you will find a collection of sample programs.

To see how the samples are compiled, optimized, and assembled, open `zncdemo.bas` and run it. A menu will be displayed, allowing you to select a sample to execute. As each step unfolds on the screen, you will get a clear understanding of the process involved in compiling your own applications.

## Compiling code
Compilation is a two stage process.

1. Compile the source code to assembly language

The following will compile a source file called demo.znc to a corresponding assembly file called demo.asm.

```
.znc demo.znc
```

1. Assemble the file to the executable binary

Using the NextBASIC Inline Assembler, you assemble the generated assembly file to a binary.

```
.asm demo.asm
```

Now you should have file called `demo` in the DOT directory on your SD Card. This DOT command can be tested by executing the following at the command line.

```
.demo
```

## Examples
### Hello world

Create a file called `hello.znc` and add the following content
```
make dot "/dot/hello";

puts("Hello World");
```

Next, compile and assemble the code
```
.znc hello.znc
.asm hello.asm
```

If you did not get any errors with the above, you can excute the program and you should see `Hello World` printed to the screen.

### Bouncing ball
This is a more complex example that bounces a character on the screen.

``` C
make dot "/dot/ball";

void gotoxy(byte x, byte y) {
  putc(22);putc(y);putc(x);
}

int x=15, y=10;
int dx=1, dy=1;
byte n = 0;

while (1) {
  __asm__ {
    halt
    ld a, (_n)
    out ($fe), a
  }
  
  gotoxy(x, y);
  putc(' ');
  x = x + dx;
  y = y + dy;
  if (x == 0 || x == 31) { dx = -dx; n=(n+1) & 7; }
  if (y == 0 || y == 21) { dy = -dy; n=(n+1) & 7; }
  gotoxy(x, y);
  putc('*');
}
```

## Syntax (incomplete)
``` EBNF
(* Program directives and top‐level items, where declarations and statements may appear. *)
<program>         ::= [ <make> ]
                      [ <org> ]
                      { <top_level_item> }

<top_level_item>  ::= <decl>
                      | <statement>

<make>            ::= "make" [ "dot" | "nex" | "raw" ] "(" <string> ")" ";"

<org>             ::= "org" <number> ";"

<bank>            ::= "bank" "(" <number> [ "," <number> ] ")" <stmnt_block>

(* Statements *)
<statement>       ::= <stmnt_block>
                      | <decl>
                      | <assignment>                    
                      | <if>
                      | <while>
                      | <for>
                      | <break>
                      | <continue>
                      | <return>
                      | <putc>
                      | <puts>
                      | <asm>
                      | <include>
                      | <expr>
                      | <org>
                      | ";"

<stmnt_block>     ::= "{" { <statement> } "}"

(* Declarations: variable declarations, function definitions, and function prototypes *)
<decl>            ::= <vardecl>
                      | <funcdecl>
                      | <funcproto>

(* Function prototype: note the trailing semicolon instead of a body *)
<funcproto>       ::= <rettype> <ident> "(" <arglist> ")" ";"

(* Assignment expressions. Either a simple identifier or a more complex lvalue/expression on the left-hand side. *)
<assignment>      ::= <lvalue> "=" <expr> ";"
<lvalue>          ::= <ident>
                    | <ident> "[" <expr> "]"
                    | "*" <lvalue>
                    | "&" <lvalue>

(* Control structures *)
<if>              ::= "if" "(" <expr> ")" <statement> [ "else" <statement> ]

<while>           ::= "while" "(" <expr> ")" <statement>

<for>             ::= "for" "(" [ <decl> | <assignment> ] ";" [ <expr> ] ";" [ <expr> ] ")" <statement>

<break>           ::= "break" ";"

<continue>        ::= "continue" ";"

<return>          ::= "return" [ <expr> ] ";"

<putc>            ::= "putc" "(" <expr> ")" ";"

<puts>            ::= "puts" "(" <expr> ")" ";"

(* Inline assembly block (contents treated as raw Z80N Assembly). *)
<asm>             ::= "__asm__" "{" Z80N_Assembly "}"

<include>         ::= "include" <string>

(* String literal: content details omitted here *)
<string>          ::= "\"" { <character> } "\""

(* Variable declaration: type, identifier, and optional initializer.
   Multiple variables may be declared in one statement. *)
<vardecl>         ::= <type> <ident> [ "=" <expr> ] { "," <ident> [ "=" <expr> ] } ";"

(* Function definition: includes return type, name, parameter list,
   either an assembly block, or a function body. *)
<funcdecl>        ::= <rettype> <ident> "(" <arglist> ")"  <asm> | <stmnt_block>

(* Function call used as an expression; when used as a statement, a semicolon is appended. *)
<call_expr>       ::= <ident> "(" [ <exprlist> ] ")"
<funccall_stmt>   ::= <call_expr> ";"

<rettype>         ::= "void" | <type>

(* Type definitions include scalar types, arrays, and pointers. *)
<type>            ::= <scalartype>
                      | <arraytype>
                      | <pointertype>

<scalartype>      ::= "char" | "byte" | "int"

<arraytype>       ::= <scalartype> "[" <number> "]"

<pointertype>     ::= <scalartype> "*"

<arglist>         ::= <arg> { "," <arg> }

<arg>             ::= <type> <ident>

(* Expressions are parsed using a precedence-climbing algorithm.
   The production below is schematic. *)
<expr>            ::= "(" <expr> ")" 
                      | <factor> { <op> <factor> }

<factor>          ::= <number>
                      | <string>
                      | <call_expr>
                      | ( "*" | "&" ) <var> [ "[" <expr> "]" ]

<var>             ::= <ident>

(* Operators available (similar to C, but with ++/-- excluded) *)
<op>              ::= "+" | "-" | "*" | "/" | "%" 
                      | "==" | "!=" | "<" | ">" | "<=" | ">=" 
                      | "&&" | "||" | "^" | "|" | "&" | "<<" | ">>"

(* Identifiers, letters, and digits *)
<ident>           ::= <letter> { <letter> | <digit> | "_" }

<letter>          ::= "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J"
                      | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T"
                      | "U" | "V" | "W" | "X" | "Y" | "Z"
                      | "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j"
                      | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t"
                      | "u" | "v" | "w" | "x" | "y" | "z"

<digit>           ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"

<number>          ::= <digit> { <digit> }

<character>       ::= /* any character except the double quote " */  
                     

(* The nonterminal Z80N_Assembly is assumed to represent a block of valid Z80N assembly code. *)
               
```