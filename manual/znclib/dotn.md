# dotn — Multi-bank DOT support

Description

Runtime support for DOT programs whose code or data spans multiple 8K banked pages. The module loads the pages, maps their bank IDs, and releases allocated pages at exit.

Types

- None

Constants

- None

Globals

- None public. The module uses internal `_page_map` and `_page_count` state.

Functions

- None public. Initialization and cleanup are performed automatically when the module is included.

Examples

```c
include "dotn.znc";
// DOTN initialization is performed by the library.
```

Notes

- Include `dot.znc`, `fileio.znc`, and `mmu.znc` dependencies are handled by the module.
- Loading failures terminate through the DOT error path.