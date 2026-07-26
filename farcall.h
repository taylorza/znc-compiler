#ifndef FARCALL_H__
#define FARCALL_H__
// Banking helper macros and far-call wrappers
#include "znc.h"

extern unsigned char _z_page_table[];

void switch_bank67(uint8_t bank, uint8_t *old_page0, uint8_t *old_page1) MYCC;
void restore_bank67(uint8_t old_page0, uint8_t old_page1) MYCC;

// Bank switch prolog/epilog for wrappers. Stubs should include znc.h first.
#define PROLOG(BANK) \
	{ \
		uint8_t page0; \
		uint8_t page1; \
		switch_bank67(BANK, &page0, &page1);

#define EPILOG \
		restore_bank67(page0, page1); \
	}

#define EPILOG_RETURN(EXPR) \
		restore_bank67(page0, page1); \
		return (EXPR); \
	}
#endif //FARCALL_H__