# ZNC - A native compiler for the ZX Spectrum Next

ZNC is a language and compiler for the ZX Spectrum Next. The language is closely modeled after the C programming language, but does deviate in some cases where it either made the language easier to parse or in other cases where I preferred an alternate syntax.

The compiler is a non-optimizing single pass compiler, while the generated code is not the fastest or the smallest it should outperform non-compiled languages.

In this language:

**Build Directives.** A program may start with optional build directives:

* The `make` directive configures the build mode (with options "dot", "nex", or "raw") and optionally a string specifying the name and location of the resulting binary.
* The `bank` directive places code in a specific bank using `bank(<bank>[,<offset>]) { ... }`.
* The `org` directive sets the origin (memory address) of the code.

**Top‐Level Items.** Programs are a sequence of top‑level items (declarations and/or statements). Unlike traditional C, top‑level statements are allowed. However, if a top‑level statement calls a function, the function must have been declared before its use—either via a full definition or via a prototype.

**Declarations.** There are variable declarations, function definitions (including variadic functions), and function prototypes. Variable types include scalar types (`char`, `byte`, `int`), arrays (declared in C#/Java style, e.g. `int[10] items;`), pointers (e.g. `int*`), and structs defined with `struct Name { ... }`.

**Control Structures.** You’ll find the usual control constructs: `if`–`else`, `while`, and `for` loops, as well as statements such as `break`, `continue`, `return`, and `switch`/`case`/`default`.

**Inline Assembly.** A special __asm__ block can be used to embed raw Z80N assembly code inside functions.

**I/O Primitives.** For basic output, built‑in functions `putc()` and `puts()` are provided. Port and register helpers are also available: `in(port)`, `out(port, value)`, `nextreg(reg, value)`, and `readreg(reg)`.

**Banks.** A bank construct allows you to group a statement block into a memory bank using `bank(<bank>[,<offset>]) { ... }`.

## Installing the tools
The compiler includes a suite of tools:

* [ZED - A text editor](https://github.com/taylorza/zed)
* [ZNC - The compiler](https://github.com/taylorza/znc-compiler)
* [ZOPT - A peephole optimizer](https://github.com/taylorza/zopt)
* [ASM - Assembler](https://taylorza.itch.io/nextbasic-inline-assembler)
* [ZIDE - Front-end driver](https://github.com/taylorza/znc-compiler)

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

## Using ZIDE

As an alternative to using the command line, you can use ZIDE which provides a more user friendly development environment.
ZIDE is a NextBASIC program that can be executed from the Browser or from the command line using the following commands.

```
load "zide.bas"
run
```

Once the IDE is launched, you can use the highlighted hotkeys to set the various options and execute commands. 

**Options:**

`W` - Sets the working file, this is file that will be opened when you launch the editor with `E`

`M` - Sets the main file for your project. This is the root file that the compiler will start with

**Commands:**

`E` - Edit the current working file

`C` - Compile the main file, any files included by the main file will also be compiled

`R` - Run the executable file. The first time you run, you will be prompted to select the executable file

`N` - Start editing a new file

`O` - Set compiler options

`Q` - Exit the IDE

**Compiler Options**

`O` from the main screen on the IDE brings you to the Compiler options screen. From this screen you can set the following options


`O` - Toggle compiler optimization on/off. When off the resulting code is larger and slower, but compile times are much faster

`C` - Set the command line arguments passed to your application when it is run from the main screen using the run command `R`

`B` - Takes you back to the previous screen in the IDE

**File Manager:**
The file manager enables you to create shortcuts to files you use frequently, this can be handy in a multi file project.

`F` - Takes you to the file manager where you can add/remove files from the slots provided.

`0`..`9` - Opens the editor and loads the corresponding file for editing

`B` - Takes you back to the previous screen in the IDE

# ZNC Language Reference
The ZNC language is designed to offer a familiar C-like structure while seamlessly integrating inline Z80N assembly.

## Basic Syntax
A typical `ZNC` source file has two major parts, **directives** and **code**

### Directives
Directives generally control the code generation process

#### make
An optional directive placed at the beginning of your source file. This directive configures the specific build options. For example, what type of binary to generate at assembly time and what the binary name should be.

If make is not specified, the compiled code will be assembled directly into memory. This can be useful if you want to compile ZNC code for use as inline code in your NextBASIC applications.

**Syntax**
`make <type> ["<binary name>"];`

`<type>` - Specified the type of binary to create
* dot - Creates a DOT command binary
* nex - Creates a NEX binary
* raw - Raw creates a raw binary with the default code address set to 0xc000 hex or 49152 decimal

`["<binary name>"]` - Optionally specifies the name of the resulting binary. If not provided, the source file name is used as the base of the binary name.

**Example:**
Create a DOT command named hello in the DOT folder a the root of the current SD card.

`make dot "/dot/hello"`

#### org
This directive can occur anywhere in the code, but it is a rather advanced directive. It is used to control the base address used for code generation. If the code is compiled and assembled directly to memory ie. no `make` option was specified then it also controls the memory address the code will be assembled to.

**Syntax**
`org <address>;`

`<address>` - Specifies the target address for the subsequent code

**Example:**
Target code for address 0x8000

`org 0x8000;`

#### bank
The `bank` directive is used to target code for specific banks. This is useful when compiling `NEX` or direct to memory binaries. Banks contain blocks of code and data.

**Syntax**
`bank <bankno> '{' <statements> '}'`

`<bankno>` - The bank that the block of code will be compiled and assembled for.
`<statements>` - The code that will be included in this bank

**Example:**
Compile code for bank 40

```c
bank 40 {
  for(char ch='A'; ch<='Z'; ch=ch+1) {
    putc(ch)
  }
}
```

### Code
The main component of your application is the code, the instructions that the compiler will transform into executable code and data.

## Data types
The ZNC compiler supports the following data types

char  - Signed 8-bit integer. Range: -128..127
byte  - Unsigned 8-bit integer. Range: 0..255
int   - Signed 16-bit integer. Range: -32768..32767
fixed - Signed 16-bit fixed-point in Q4 (12.4) format. Range: -2048.0 to +2047.9375, precision 0.0625 (1/16). Literals use a decimal point, e.g. `3.14`.

You can also create pointers to or arrays for each of the data types.

### Pointers
Pointers enable to you access data in memory at the address pointed to by the pointer. And integer pointer provides access to a 2 byte integer value at the target address, while a byte pointer accesses a single byte at the address pointed to by the pointer.

The follow will declare a pointer that points to the ZX Spectrum ULA screen memory and writes a byte to the screen.

```
byte *screen = 0x4000; // ZX Spectrum ULA Screen address (16384 in decimal)
*screen = 255;         // Write the value 255 to the memory address pointed to by "screen"
```

#### Pointer arithmetic 
Adding/subtracting an integer scales by the pointed element size (byte → +1 byte, int/fixed → +2 bytes). Indexing `p[i]` is equivalent to `*(p + i)`. Pointer + pointer is not supported; subtracting two pointers is not supported.

`void*` is supported as a generic pointer; it is compatible with any other pointer type in assignments and function calls. Arithmetic on `void*` is not defined since element size is unknown.

### Arrays
You can also create arrays of the base data types. For example if you wanted to store ten integers you could declare an array of integers as follows

```C
int[10] numbers;
```

#### Array access and initialization
Arrays are declared with the size on the type, e.g. `int[10] numbers;`. If you omit the size *and* provide an initializer, the compiler infers the length:

```c
int[] nums = {1, 2};   // length = 2
char[] s = "Hi";       // length = 3 (includes trailing 0)
```

If you omit the size and provide **no** initializer, the declaration is treated as a pointer (`int[] p;` → `int* p`). Arrays with an explicit size do **not** allow an initializer; initialize them at runtime instead.

Array indexing uses `[]` as usual: `numbers[0] = 42;`.

#### Const values
`const` is only allowed on scalar types (`char`, `byte`, `int`, `fixed`). It creates a compile-time constant with no storage allocated; the value is folded into code. `const` on pointers or arrays is rejected.

### Declaring variables
Variables can be declared at any point in the code

### Functions
Functions must be declared before use, either via a prototype or a full definition. Variadic functions use `...` after the last named parameter and the built-ins `va_start(list, lastNamedParam);`, `va_arg(list, type);`, and `va_end(list);` inside the body. Use `va_list` as the argument cursor.

```c
int sum(int count, ...) {
  va_list args;
  int total = 0;
  va_start(args, count);
  while (count--) total = total + va_arg(args, int);
  va_end(args);
  return total;
}
```

### Structs
Struct types are defined with `struct Name { /* fields */ };` and can be used for variables, pointers, and arrays. Access fields with `.`; this works for both struct variables and pointers to structs (there is no `->` operator).

```c
struct Point { byte x; byte y; };

struct Point p;
p.x = 10; p.y = 5;

struct Point* pp = &p;
pp.x = 3;  // pointer-to-struct also uses '.'
```

## Syntax (incomplete)
``` EBNF
(* ----------------------------------------------------------------
   Program structure
   ---------------------------------------------------------------- *)

<program>         ::= [ <make> ] [ <org> ] { <top_level_item> }

<top_level_item>  ::= <top_decl>
                    | <statement>
                    | <hashif_top>

(* ----------------------------------------------------------------
   Directives
   ---------------------------------------------------------------- *)

<make>            ::= "make" ( "dot" | "nex" | "raw" ) [ <string> ] ";"
<org>             ::= "org" <number> ";"
<bank>            ::= "bank" "(" <number> [ "," <number> ] ")" <stmnt_block>

(* ----------------------------------------------------------------
   Declarations — separated by context
   ---------------------------------------------------------------- *)

(* Top-level: function definitions, prototypes, delegates, enums allowed *)
<top_decl>        ::= <vardecl>
                    | <funcdecl>
                    | <funcproto>
                    | <structdef>
                    | <enumdef>
                    | <delegatedef>

(* Block-level: only variable and struct declarations *)
<local_decl>      ::= <vardecl>
                    | <structdef>

<structdef>       ::= "struct" <ident> "{" { <vardecl> } "}" [ ";" ]

<enumdef>         ::= "enum" <ident> "{" <enum_member_decl> { "," <enum_member_decl> } [ "," ] "}" [ ";" ]
<enum_member_decl> ::= <ident> [ "=" <const_expr> ]

<delegatedef>     ::= "delegate" <rettype> <ident> "(" [ <arglist> ] ")" ";"

<funcproto>       ::= <rettype> <ident> "(" [ <arglist> ] [ "..." ] ")" ";"

<funcdecl>        ::= <rettype> <ident> "(" [ <arglist> ] [ "..." ] ")" ( <asm> | <stmnt_block> )

<vardecl>         ::= [ "const" ] <type> <var_declarator> { "," <var_declarator> } ";"

<var_declarator>  ::= <ident> [ "=" <initializer_expr> ]

<rettype>         ::= "void" | <type>

(* ----------------------------------------------------------------
   Types
   ---------------------------------------------------------------- *)

(* { "*" } supports multi-level pointers (e.g. int**, char*).
   The optional "[...]" adds one array dimension.
   The language type system encodes indirection in a 4-bit field;
   a semantic check may restrict maximum pointer depth if required. *)
<type>            ::= <basetype> { "*" } [ "[" [ <const_expr> ] "]" ]

(* "struct" <ident> is unambiguous. Bare <ident> resolves a named delegate,
   struct, or enum type via the symbol table (context-sensitive; semantic restriction). *)
<basetype>        ::= "char"
                    | "byte"
                    | "int"
                    | "fixed"
                    | "struct" <ident>
                    | <ident>

<arglist>         ::= <arg> { "," <arg> }
<arg>             ::= <type> <ident>

(* ----------------------------------------------------------------
   Statements
   ---------------------------------------------------------------- *)

<statement>       ::= <stmnt_block>
                    | <local_decl>
                    | <if>
                    | <switch>
                    | <while>
                    | <for>
                    | <break>
                    | <continue>
                    | <return>
                    | <exit>
                    | <putc>
                    | <puts>
                    | <out>
                    | <nextreg>
                    | <asm>
                    | <include>
                    | <org>
                    | <bank>
                    | <hashif_block>
                    | <expr> ";"
                    | ";"

<stmnt_block>     ::= "{" { <statement> } "}"

(* ----------------------------------------------------------------
   Expressions
   ---------------------------------------------------------------- *)

(* Every expression is an assignment; the "=" clause is optional.
   This eliminates the precedence hole from the original grammar.
   Lvalue-ness of the left-hand side is enforced semantically. *)
<expr>            ::= <assignment>

(* Right-associative: a = b = c parses as a = (b = c) *)
<assignment>      ::= <ternary> [ ( "=" | <assign_op> ) <assignment> ]

<ternary>         ::= <binary> [ "?" <expr> ":" <assignment> ]

<binary>          ::= <unary> { <binop> <unary> }

(* Dereference "*" and address-of "&" are prefix unary operators.
   Dereference is removed from <primary> (was duplicated there).
   Address-of applies to any syntactically valid lvalue. *)
<unary>           ::= <postfix>
                    | "++" <unary>
                    | "--" <unary>
                    | "+"  <unary>
                    | "-"  <unary>
                    | "~"  <unary>
                    | "!"  <unary>
                    | "*"  <unary>        (* dereference *)
                    | "&"  <lvalue_expr>  (* address-of any lvalue *)

<postfix>         ::= <primary> { <postfix_op> }
<postfix_op>      ::= "++"
                    | "--"
                    | "(" [ <exprlist> ] ")"
                    | "[" <expr> "]"
                    | "." <ident>

(* Dereference and address-of removed; brace-init moved to <brace_init> *)
<primary>         ::= <number>
                    | <string>
                    | <enum_member_ref>
                    | <ident>
                    | "(" <expr> ")"
                    | "in" "(" <expr> ")"
                    | "readreg" "(" <expr> ")"
                    | <brace_init>

(* Enum member reference uses type-qualified syntax, e.g. Color.GREEN.
   The first identifier must resolve to an enum type name. *)
<enum_member_ref> ::= <ident> "." <ident>

(* lvalue_expr: syntactically valid assignment targets and operands for "&".
   Left-recursive via indexing and field access (natural for a postfix grammar). *)
<lvalue_expr>     ::= <ident>
                    | "*" <unary>
                    | <lvalue_expr> "[" <expr> "]"
                    | <lvalue_expr> "." <ident>
                    | "(" <lvalue_expr> ")"

<exprlist>        ::= <expr> { "," <expr> }

(* ----------------------------------------------------------------
   Constant expressions
   Syntactically a restricted subset of <expr>; semantically limited to
   compile-time-evaluable values: numeric/character/fixed-point literals
   and const-qualified identifiers, combined with operators below.
   ---------------------------------------------------------------- *)

<const_expr>      ::= <const_ternary>

<const_ternary>   ::= <const_binary> [ "?" <const_expr> ":" <const_expr> ]

<const_binary>    ::= <const_unary> { <binop> <const_unary> }

<const_unary>     ::= { "+" | "-" | "~" | "!" } <const_primary>

<const_primary>   ::= <number>
                    | <enum_member_ref>
                    | <ident>               (* must be a const-qualified variable *)
                    | "(" <const_expr> ")"

(* ----------------------------------------------------------------
   Initializers
   ---------------------------------------------------------------- *)

(* Used in <var_declarator>: scalar expression or brace-enclosed aggregate *)
<initializer_expr> ::= <expr>
                     | <brace_init>

(* Brace initializer for arrays and structs.
   Elements are constant expressions; nesting supports arrays of arrays
   and structs containing structs.
   A trailing comma is permitted for convenience. *)
<brace_init>      ::= "{" <init_element> { "," <init_element> } [ "," ] "}"
<init_element>    ::= <brace_init>    (* nested aggregate *)
                    | <const_expr>    (* scalar constant element *)

(* ----------------------------------------------------------------
   Operators
   ---------------------------------------------------------------- *)

<binop>           ::= "+" | "-" | "*" | "/" | "%"
                    | "==" | "!=" | "<" | ">" | "<=" | ">="
                    | "&&" | "||"
                    | "&"  | "|"  | "^" | "<<" | ">>"

<assign_op>       ::= "+=" | "-=" | "*=" | "/=" | "%="
                    | "<<=" | ">>=" | "&=" | "|=" | "^="

(* ----------------------------------------------------------------
   Control structures
   ---------------------------------------------------------------- *)

<if>              ::= "if" "(" <expr> ")" <statement> [ "else" <statement> ]

<switch>          ::= "switch" "(" <expr> ")" "{" { <case> | <default> } "}"
<case>            ::= "case" <const_expr> ":" { <statement> }
<default>         ::= "default" ":" { <statement> }

<while>           ::= "while" "(" <expr> ")" <statement>

(* for-init inlines the variable declaration syntax without a trailing ";".
   The ";" separators in "for(...;...;...)" are explicit in this rule. *)
<for>             ::= "for" "(" <for_init> ";" [ <expr> ] ";" [ <expr> ] ")" <statement>
<for_init>        ::= [ "const" ] <type> <var_declarator> { "," <var_declarator> }
                    | <expr>
                    | (* empty *)

<break>           ::= "break" ";"
<continue>        ::= "continue" ";"
<return>          ::= "return" [ <expr> ] ";"
<exit>            ::= "exit" "(" <expr> ")" ";"
<putc>            ::= "putc" "(" <expr> ")" ";"
<puts>            ::= "puts" "(" <expr> ")" ";"
<out>             ::= "out" "(" <expr> "," <expr> ")" ";"
<nextreg>         ::= "nextreg" "(" <expr> "," <expr> ")" ";"
<asm>             ::= "__asm__" "{" <Z80N_Assembly> "}"
<include>         ::= "include" <string>

(* ----------------------------------------------------------------
   Conditional compilation
   Two variants to cleanly handle context:
   - <hashif_top>   appears at top level; branches are <top_level_item>s
   - <hashif_block> appears inside blocks; branches are <statement>s
   ---------------------------------------------------------------- *)

<hashif_top>      ::= ( "#if"    <const_expr>
                      | "#ifdef"  <ident>
                      | "#ifndef" <ident>
                      )
                      { <top_level_item> }
                      { "#elif" <const_expr> { <top_level_item> } }
                      [ "#else" { <top_level_item> } ]
                      "#endif"

<hashif_block>    ::= ( "#if"    <const_expr>
                      | "#ifdef"  <ident>
                      | "#ifndef" <ident>
                      )
                      { <statement> }
                      { "#elif" <const_expr> { <statement> } }
                      [ "#else" { <statement> } ]
                      "#endif"

(* ----------------------------------------------------------------
   Lexical rules
   ---------------------------------------------------------------- *)

(* Maximum identifier length is 14 characters; longer names are a compile error. *)
<ident>           ::= <letter> { <letter> | <digit> | "_" }

<number>          ::= <decimal_literal>
                    | <hex_literal>
                    | <binary_literal>
                    | <char_literal>
                    | <fixed_literal>

<decimal_literal> ::= <digit> { <digit> }
<hex_literal>     ::= "0x" <hex_digit> { <hex_digit> }
<binary_literal>  ::= "0b" ( "0" | "1" ) { "0" | "1" }
<char_literal>    ::= "'" <character> "'"
<fixed_literal>   ::= <decimal_literal> "." <decimal_literal>
                      (* 16-bit signed Q4 fixed-point; stored in 12.4 format *)
<hex_digit>       ::= <digit> | "a".."f" | "A".."F"

(* Adjacent string literals are concatenated: "Hel" "lo" → "Hello" *)
<string>          ::= '"' { <str_char> } '"' { '"' { <str_char> } '"' }
<str_char>        ::= <character> | <escape_seq>
<escape_seq>      ::= "\" ( "n" | "r" | "t" | "\" | '"' | "0"
                          | "x" <hex_digit> <hex_digit> )

(* Block comments may be nested *)
<comment>         ::= "//" { <any_char> } <newline>
                    | "/*" { <any_char> | <comment> } "*/"

(* <Z80N_Assembly> represents a valid block of Z80N assembly mnemonics
   and directives as accepted by the target assembler. *)
```