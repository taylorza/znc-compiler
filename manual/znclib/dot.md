# dot — DOT error handler

Description

Register an error handler to be used by the DOT command.

Types

- `delegate void PFN_EXIT()` —  Function pointer type for error/exit handler functions.

Functions

- `void seterrh(PFN_EXIT fn)` — Register an error handler function. 
- `int atexit(PFN_EXIT fn)` — Register a function to be called at program exit. Returns 0 on failure (e.g., if the stack of exit functions is full), or 1 on success.

Examples

```c
void myerr() {
  puts("DOT overlay error\r");
  exit(0);
}
seterrh(myerr);
```
