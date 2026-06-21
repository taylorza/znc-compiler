# dot — DOT error handler

Description

Register an error handler to be used by the DOT command.

Types

- `delegate void ErrorHandler()` —  Function pointer type for error handler functions.

Functions

- `void seterrh(ErrorHandler fn)` — Register an error handler function. 

Examples

```c
void myerr() {
  puts("DOT overlay error\r");
  exit(0);
}
seterrh(myerr);
```
