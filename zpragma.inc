// place stack

//#pragma output REGISTER_SP = xxxx

// limit size of stdio
#pragma printf = %s %c %d

// room for one atexit function
#pragma output CLIB_EXIT_STACK_SIZE = 1
#pragma output CLIB_QUICKEXIT_STACK_SIZE = 1

// default = 0x30002  : 0x10001 = halt & loop, 0x30002 = return to basic
#pragma output CRT_ON_EXIT = 0x10001

// CONTROLS FOR DOT COMMANDS > 8K
// WE WANT TO ALLOCATE PAGES FOR 0x8000 AND HIGHER

// logical -> physical lookup table only needs to cover main 64k

// #pragma output DOTN_LAST_PAGE = 11

// allocate pages for 0x8000 and up

//#pragma output DOTN_MAIN_ABSOLUTE_MASK = 0x0F

// use a standard z80 memory map (ie all lumped at 0x2000)

//#pragma output __MMAP = 0

// append file mmap.inc to memory map

//#pragma output CRT_APPEND_APPEND_MMAP = 1

// default = 1  : non-zero closes open FILEs on exit
#pragma output CRT_ENABLE_CLOSE = 1   
