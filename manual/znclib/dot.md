# dot — DOT error handler

Description

Register an error handler to be used by the DOT command.

Types

- `delegate void PFN_EXIT()` —  Function pointer type for error/exit handler functions.

Functions

- `void seterrh(PFN_EXIT fn)` — Register an error handler function. 
- `void exitat(PFN_EXIT fn)` — Register a function to be called at program exit.

Examples

```c
void myerr() {
  puts("DOT overlay error\r");
  exit(0);
}
seterrh(myerr);
```
