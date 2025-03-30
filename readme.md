## ZNC - A native compiler for the ZX Spectrum Next

ZNC is a language and compiler for the ZX Spectrum Next. The language is closely modeled after the C programming language, but does deviate in some cases where it either made the language easier to parse or in other cases where I preferred an alternate syntax.

The compiler is a non-optimizing single pass compiler, while the generated code is not the fastest or the smallest it should outperform non-compiled languages.

## Language Features
* Data Types
    - char/byte
    - int
    - arrays
    - pointers
* Flow control
    - if/else
    - while/for loops
* Expressions
    - Evaluated in accordance with operator precedence
    - Parenthesised expressions
    - Bitwise and logical operators
* Inline assembly
* Include files
* User functions
    
## Compiling code
Currently the compiler will generate a DOT Command. Compilation is a two stage process.

1. Compile the source code to assembly language

The following will compile a source file called demo.znc to a corresponding assembly file called demo.asm. These names do not need to match, but the assembly filename will be the base for the name of the final binary file.

```
.znc demo.znc demo.asm
```

2. Assemble the file to the executable binary

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
puts("Hello World");
```

Next, compile and assemble the code
```
.znc hello.znc hello.asm
.asm hello.asm
```

And if you did not get any errors with the above, you can excute the program and you should see `Hello World` printed to the screen.

### Bouncing ball
This is a more complex example that bounces a character on the screen.

``` C
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
``` BNF
<program>       => <statement>*
<statement>     => <stmnt block>
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
                    | ';'
<stmnt block>   => '{' <statement>* '}'
<decl>          => <vardecl>
                    | <funcdecl>
<assignment>    => [<ident>|<expr>] '=' <expr>
<if>            => 'if' '(' <expr> ')' <statemment>
                    ('else' <statement>)
<while>         => 'while' '(' <expr> ')' <statement>
<for>           => 'for' '('
                        <decl>|<assignment> ';' 
                        <expr> ';' 
                        <expr>')' <statement>
<break>         => 'break' ';'
<continue>      => 'continue' ';'
<return>        => 'return' (<expr>);
<putc>          => 'putc' '('<expr>')'
<puts>          => 'puts' '('<expr>')'
<asm>           => '__asm__' '{' Z80 Assembly '}'
<include>       => 'include' <string>
<string>        => '"'...'"'
<vardecl>       => <type> <ident> ('=' <expr>) (',' <vardecl>) ';'
<funcdecl>      => <rettype> <ident> '('<arglist>')' 
<expr>          => '('<expr>')' 
                    | <expr> <op> <expr> 
                    | <funccall>
<funccall>      => <ident>'('<exprlist>')'

```