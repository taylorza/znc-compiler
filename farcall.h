#ifndef FARCALL_H__
#define FARCALL_H__
// Banking helper macros and far-call wrappers
#include "znc.h"

extern unsigned char _z_page_table[];

// Bank switch prolog/epilog for wrappers. Stubs should include znc.h first.
#define PROLOG(BANK) \
	{ \
		uint8_t page0 = ZXN_READ_MMU6(); \
		uint8_t page1 = ZXN_READ_MMU7(); \
		ZXN_WRITE_MMU6(_z_page_table[(BANK)<<1]); \
		ZXN_WRITE_MMU7(_z_page_table[((BANK)<<1)+1]);

#define EPILOG \
		ZXN_WRITE_MMU6(page0); \
		ZXN_WRITE_MMU7(page1); \
	}

#define EPILOG_RETURN(EXPR) \
		ZXN_WRITE_MMU6(page0); \
		ZXN_WRITE_MMU7(page1); \
		return (EXPR); \
	}

#endif //FARCALL_H__