// place stack

#pragma output REGISTER_SP = 0xbffd

// limit size of stdio
#pragma printf = %s %c %d

// room for one atexit function
#pragma output CLIB_EXIT_STACK_SIZE = 1
#pragma output CLIB_QUICKEXIT_STACK_SIZE = 1

// default = 0x30002  : 0x10001 = halt & loop, 0x30002 = return to basic
#pragma output CRT_ON_EXIT = 0x10001
#pragma output CRT_ORG_MAIN = 0x6000

// CONTROLS FOR DOT COMMANDS > 8K
// WE WANT TO ALLOCATE PAGES FOR 0x8000 AND HIGHER

// logical -> physical lookup table only needs to cover main 64k

// #pragma output DOTN_LAST_PAGE = 223

// allocate pages for 0x8000 and up

#pragma output DOTN_MAIN_ABSOLUTE_MASK = 0x07

// use a standard z80 memory map (ie all lumped at 0x2000)

//#pragma output __MMAP = 0

// append file mmap.inc to memory map

//#pragma output CRT_APPEND_APPEND_MMAP = 1

// default = 1  : non-zero closes open FILEs on exit
#pragma output CRT_ENABLE_CLOSE = 1   

// Dynamic Memory
#pragma output CLIB_MALLOC_HEAP_SIZE   = 0      // default =  -1  : size of malloc heap (0 = do not create a heap, -1 = place heap between bss and bottom of stack)
#pragma output CLIB_STDIO_HEAP_SIZE    = 256    // default = 256  : size of stdio heap (0 = do not allocate bytes for opening of files)
#pragma output CLIB_BALLOC_TABLE_SIZE  = 0      // default =   0  : number of queues created for block memory allocator