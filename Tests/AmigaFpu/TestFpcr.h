#pragma once

#include <stdint.h>

inline uint32_t TestReadFpcr()
{
#if defined(__m68k__) && defined(TEST_HARD_FLOAT)
	uint32_t value;
	__asm__ volatile ("fmove.l %%fpcr,%0" : "=d" (value) : : "memory");
	return value;
#else
	return 0xffffffffUL;
#endif
}

inline void TestWriteFpcr(uint32_t value)
{
#if defined(__m68k__) && defined(TEST_HARD_FLOAT)
	__asm__ volatile ("fmove.l %0,%%fpcr" : : "d" (value) : "memory");
#else
	(void)value;
#endif
}

struct TestFpcrGuard
{
	uint32_t saved;
	TestFpcrGuard() : saved(TestReadFpcr()) {}
	~TestFpcrGuard() { TestWriteFpcr(saved); }
};
