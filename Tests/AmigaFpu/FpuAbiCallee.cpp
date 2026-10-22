#include "FpuAbiTest.h"

#include <math.h>
#include <stdarg.h>

FpuVirtualProbe::~FpuVirtualProbe() = default;

extern "C" FPU_NOINLINE float FpuReturnFloat(float value)
{
	return value;
}

extern "C" FPU_NOINLINE double FpuReturnDouble(double value)
{
	return value;
}

extern "C" FPU_NOINLINE float FpuAddFloat(float a, float b)
{
	return a + b;
}

extern "C" FPU_NOINLINE float FpuMulFloat(float a, float b)
{
	return a * b;
}

extern "C" FPU_NOINLINE float FpuDivFloat(float a, float b)
{
	return a / b;
}

extern "C" FPU_NOINLINE double FpuAddDouble(double a, double b)
{
	return a + b;
}

extern "C" FPU_NOINLINE double FpuMulDouble(double a, double b)
{
	return a * b;
}

extern "C" FPU_NOINLINE double FpuDivDouble(double a, double b)
{
	return a / b;
}

extern "C" FPU_NOINLINE float FpuUnsigned64ToFloat(uint64_t value)
{
	return static_cast<float>(value);
}

extern "C" FPU_NOINLINE double FpuUnsigned64ToDouble(uint64_t value)
{
	return static_cast<double>(value);
}

extern "C" FPU_NOINLINE float FpuSigned64ToFloat(int64_t value)
{
	return static_cast<float>(value);
}

extern "C" FPU_NOINLINE double FpuSigned64ToDouble(int64_t value)
{
	return static_cast<double>(value);
}

extern "C" FPU_NOINLINE uint64_t FpuFloatToUnsigned64(float value)
{
	return static_cast<uint64_t>(value);
}

extern "C" FPU_NOINLINE int64_t FpuDoubleToSigned64(double value)
{
	return static_cast<int64_t>(value);
}

extern "C" FPU_NOINLINE float FpuInvokeCallback(FpuFloatCallback callback, uint64_t value)
{
	return callback(value) + 0.5f;
}

extern "C" FPU_NOINLINE double FpuReadPromotedFloat(int marker, ...)
{
	va_list args;
	va_start(args, marker);
	double value = va_arg(args, double);
	va_end(args);
	return value + static_cast<double>(marker - marker);
}

extern "C" FPU_NOINLINE float FpuSqrtFloat(float value)
{
	return sqrtf(value);
}

extern "C" FPU_NOINLINE double FpuSqrtDouble(double value)
{
	return sqrt(value);
}

extern "C" FPU_NOINLINE double FpuFloorDouble(double value)
{
	return floor(value);
}

extern "C" FPU_NOINLINE int FpuCompareFloat(float a, float b)
{
	return (a < b ? 1 : 0) | (a <= b ? 2 : 0) | (a == b ? 4 : 0) |
		(a != b ? 8 : 0) | (a >= b ? 16 : 0) | (a > b ? 32 : 0);
}

extern "C" FPU_NOINLINE int FpuCompareDouble(double a, double b)
{
	return (a < b ? 1 : 0) | (a <= b ? 2 : 0) | (a == b ? 4 : 0) |
		(a != b ? 8 : 0) | (a >= b ? 16 : 0) | (a > b ? 32 : 0);
}

namespace
{
	class FpuVirtualProbeImpl : public FpuVirtualProbe
	{
	public:
		FPU_NOINLINE float AddFloat(float a, float b) override
		{
			return a + b;
		}

		FPU_NOINLINE double AddDouble(double a, double b) override
		{
			return a + b;
		}
	};
}

FPU_NOINLINE FpuVirtualProbe* CreateFpuVirtualProbe()
{
	return new FpuVirtualProbeImpl();
}

