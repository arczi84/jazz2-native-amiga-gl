#pragma once

#include <stdint.h>

#if defined(__GNUC__)
#	define FPU_NOINLINE __attribute__((noinline))
#else
#	define FPU_NOINLINE
#endif

class FpuVirtualProbe
{
public:
	virtual ~FpuVirtualProbe();
	virtual float AddFloat(float a, float b) = 0;
	virtual double AddDouble(double a, double b) = 0;
};

typedef float (*FpuFloatCallback)(uint64_t value);

extern "C" {
	FPU_NOINLINE float FpuReturnFloat(float value);
	FPU_NOINLINE double FpuReturnDouble(double value);
	FPU_NOINLINE float FpuAddFloat(float a, float b);
	FPU_NOINLINE float FpuMulFloat(float a, float b);
	FPU_NOINLINE float FpuDivFloat(float a, float b);
	FPU_NOINLINE double FpuAddDouble(double a, double b);
	FPU_NOINLINE double FpuMulDouble(double a, double b);
	FPU_NOINLINE double FpuDivDouble(double a, double b);

	FPU_NOINLINE float FpuUnsigned64ToFloat(uint64_t value);
	FPU_NOINLINE double FpuUnsigned64ToDouble(uint64_t value);
	FPU_NOINLINE float FpuSigned64ToFloat(int64_t value);
	FPU_NOINLINE double FpuSigned64ToDouble(int64_t value);
	FPU_NOINLINE uint64_t FpuFloatToUnsigned64(float value);
	FPU_NOINLINE int64_t FpuDoubleToSigned64(double value);

	FPU_NOINLINE float FpuInvokeCallback(FpuFloatCallback callback, uint64_t value);
	FPU_NOINLINE double FpuReadPromotedFloat(int marker, ...);
	FPU_NOINLINE float FpuSqrtFloat(float value);
	FPU_NOINLINE double FpuSqrtDouble(double value);
	FPU_NOINLINE double FpuFloorDouble(double value);

	FPU_NOINLINE int FpuCompareFloat(float a, float b);
	FPU_NOINLINE int FpuCompareDouble(double a, double b);
}

FPU_NOINLINE FpuVirtualProbe* CreateFpuVirtualProbe();

