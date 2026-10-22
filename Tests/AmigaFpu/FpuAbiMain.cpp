#include "FpuAbiTest.h"
#include "TestFpcr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEST_VARIANT
#define TEST_VARIANT "default"
#endif

namespace
{
	int Failures = 0;
	int Checks = 0;

	uint32_t FloatBits(float value)
	{
		uint32_t bits;
		memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	uint64_t DoubleBits(double value)
	{
		uint64_t bits;
		memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	void Print64(uint64_t value)
	{
		printf("%08lx%08lx", static_cast<unsigned long>(value >> 32), static_cast<unsigned long>(value));
	}

	void Check32(const char* name, uint32_t actual, uint32_t expected)
	{
		++Checks;
		if (actual != expected) {
			++Failures;
			printf("FAIL %-28s got=%08lx expected=%08lx\n", name,
				static_cast<unsigned long>(actual), static_cast<unsigned long>(expected));
		}
	}

	void Check64(const char* name, uint64_t actual, uint64_t expected)
	{
		++Checks;
		if (actual != expected) {
			++Failures;
			printf("FAIL %-28s got=", name);
			Print64(actual);
			printf(" expected=");
			Print64(expected);
			printf("\n");
		}
	}

	void CheckInt(const char* name, int actual, int expected)
	{
		++Checks;
		if (actual != expected) {
			++Failures;
			printf("FAIL %-28s got=%ld expected=%ld\n", name,
				static_cast<long>(actual), static_cast<long>(expected));
		}

	}

	extern "C" FPU_NOINLINE float CallbackUnsigned64ToFloat(uint64_t value)
	{
		return static_cast<float>(value);
	}
}

int main(int argc, char** argv)
{
	TestFpcrGuard fpcrGuard;
	if (argc >= 2) {
		TestWriteFpcr(static_cast<uint32_t>(strtoul(argv[1], nullptr, 0)));
	}
	const uint32_t fpcr = TestReadFpcr();
	printf("variant=%s fpcr=%08lx inherited=%08lx\n", TEST_VARIANT,
		static_cast<unsigned long>(fpcr), static_cast<unsigned long>(fpcrGuard.saved));
	// Positive UINT64_MAX rounds down for round-to-zero / round-to-minus.
	// Keep the oracle as integer bit patterns, independent of tested conversions.
	const bool roundDown = fpcr != 0xffffffffUL &&
		((fpcr & 0x30) == 0x10 || (fpcr & 0x30) == 0x20);
	printf("FpuAbiTest compiler=%s hard=%d opt=%d fast=%d\n", __VERSION__,
#if defined(TEST_HARD_FLOAT)
		1,
#else
		0,
#endif
#if defined(__OPTIMIZE__)
		1,
#else
		0,
#endif
#if defined(__FAST_MATH__)
		1);
#else
		0);
#endif

	volatile float f15 = 1.5f;
	volatile float f225 = 2.25f;
	volatile float f7 = 7.0f;
	volatile float f2 = 2.0f;
	volatile double d15 = 1.5;
	volatile double d225 = 2.25;
	volatile double d7 = 7.0;
	volatile double d2 = 2.0;
	volatile uint64_t uOne = 1ULL;
	volatile uint64_t u32 = 0x0000000100000000ULL;
	volatile uint64_t uMax = 0xffffffffffffffffULL;
	volatile int64_t sMin = (-9223372036854775807LL - 1LL);

	Check32("return float", FloatBits(FpuReturnFloat(f225)), 0x40100000UL);
	Check64("return double", DoubleBits(FpuReturnDouble(d225)), 0x4002000000000000ULL);
	Check32("float add", FloatBits(FpuAddFloat(f15, f225)), 0x40700000UL);
	Check32("float multiply", FloatBits(FpuMulFloat(f15, f225)), 0x40580000UL);
	Check32("float divide", FloatBits(FpuDivFloat(f7, f2)), 0x40600000UL);
	Check64("double add", DoubleBits(FpuAddDouble(d15, d225)), 0x400e000000000000ULL);
	Check64("double multiply", DoubleBits(FpuMulDouble(d15, d225)), 0x400b000000000000ULL);
	Check64("double divide", DoubleBits(FpuDivDouble(d7, d2)), 0x400c000000000000ULL);

	Check32("u64 1 -> float", FloatBits(FpuUnsigned64ToFloat(uOne)), 0x3f800000UL);
	Check32("u64 2^32 -> float", FloatBits(FpuUnsigned64ToFloat(u32)), 0x4f800000UL);
	Check32("u64 max -> float", FloatBits(FpuUnsigned64ToFloat(uMax)),
		roundDown ? 0x5f7fffffUL : 0x5f800000UL);
	Check64("u64 2^32 -> double", DoubleBits(FpuUnsigned64ToDouble(u32)), 0x41f0000000000000ULL);
	Check64("u64 max -> double", DoubleBits(FpuUnsigned64ToDouble(uMax)),
		roundDown ? 0x43efffffffffffffULL : 0x43f0000000000000ULL);
	Check32("i64 min -> float", FloatBits(FpuSigned64ToFloat(sMin)), 0xdf000000UL);
	Check64("i64 min -> double", DoubleBits(FpuSigned64ToDouble(sMin)), 0xc3e0000000000000ULL);
	Check64("float 2^32 -> u64", FpuFloatToUnsigned64(4294967296.0f), 0x0000000100000000ULL);
	Check64("double -2^32 -> i64", static_cast<uint64_t>(FpuDoubleToSigned64(-4294967296.0)), 0xffffffff00000000ULL);

	Check32("callback float return", FloatBits(FpuInvokeCallback(CallbackUnsigned64ToFloat, uOne)), 0x3fc00000UL);
	Check64("varargs float promotion", DoubleBits(FpuReadPromotedFloat(7, static_cast<float>(f225))), 0x4002000000000000ULL);

	FpuVirtualProbe* probe = CreateFpuVirtualProbe();
	Check32("virtual float return", FloatBits(probe->AddFloat(f15, f225)), 0x40700000UL);
	Check64("virtual double return", DoubleBits(probe->AddDouble(d15, d225)), 0x400e000000000000ULL);
	delete probe;

	Check32("sqrtf", FloatBits(FpuSqrtFloat(9.0f)), 0x40400000UL);
	Check64("sqrt", DoubleBits(FpuSqrtDouble(9.0)), 0x4008000000000000ULL);
	Check64("floor", DoubleBits(FpuFloorDouble(-1.25)), 0xc000000000000000ULL);

	CheckInt("float compare less", FpuCompareFloat(f15, f225), 11);
	CheckInt("float compare equal", FpuCompareFloat(f225, f225), 22);
	CheckInt("float compare greater", FpuCompareFloat(f225, f15), 56);
	CheckInt("double compare less", FpuCompareDouble(d15, d225), 11);
	CheckInt("double compare equal", FpuCompareDouble(d225, d225), 22);
	CheckInt("double compare greater", FpuCompareDouble(d225, d15), 56);

	const uint32_t finalFpcr = TestReadFpcr();
	Check32("FPCR unchanged", finalFpcr, fpcr);
	printf("final_fpcr=%08lx\n", static_cast<unsigned long>(finalFpcr));
	if (Failures == 0) {
		printf("PASS checks=%ld\n", static_cast<long>(Checks));
		return 0;
	}

	printf("FAILED checks=%ld failures=%ld\n", static_cast<long>(Checks), static_cast<long>(Failures));
	return 1;
}
