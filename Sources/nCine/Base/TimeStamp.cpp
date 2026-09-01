#include "TimeStamp.h"
#include "../../Main.h"

namespace nCine
{
	#if defined(DEATH_TARGET_AMIGAOS)
	namespace
	{
		// GCC's Amiga hard-float driver can select a soft-float libgcc multilib for
		// __floatundisf. Avoid that ABI boundary in the frame clock altogether:
		// each 16-bit limb promotes to a signed int and is converted inline by the
		// FPU, while Horner's rule preserves the normal uint64_t-to-float result.
		float CounterToFloat(std::uint64_t value)
		{
			constexpr float LimbBase = 65536.0f;
			const std::uint32_t hi = std::uint32_t(value >> 32);
			const std::uint32_t lo = std::uint32_t(value);
			return ((float(std::uint16_t(hi >> 16)) * LimbBase + float(std::uint16_t(hi))) * LimbBase +
				float(std::uint16_t(lo >> 16))) * LimbBase + float(std::uint16_t(lo));
		}
	}
	#endif

	TimeStamp::TimeStamp()
		: _counter(0)
	{
	}

	TimeStamp::TimeStamp(std::uint64_t counter)
		:_counter(counter)
	{
	}

	bool TimeStamp::operator>(const TimeStamp& other) const
	{
		return _counter > other._counter;
	}

	bool TimeStamp::operator<(const TimeStamp& other) const
	{
		return _counter < other._counter;
	}

	TimeStamp& TimeStamp::operator+=(const TimeStamp& other)
	{
		_counter += other._counter;
		return *this;
	}

	TimeStamp& TimeStamp::operator-=(const TimeStamp& other)
	{
		DEATH_ASSERT(_counter >= other._counter);
		_counter -= other._counter;
		return *this;
	}

	TimeStamp TimeStamp::operator+(const TimeStamp& other) const
	{
		return TimeStamp(_counter + other._counter);
	}

	TimeStamp TimeStamp::operator-(const TimeStamp& other) const
	{
		DEATH_ASSERT(_counter >= other._counter);
		return TimeStamp(_counter - other._counter);
	}

	TimeStamp TimeStamp::timeSince() const
	{
		return TimeStamp(clock().now() - _counter);
	}

	float TimeStamp::secondsSince() const
	{
		return TimeStamp(clock().now() - _counter).seconds();
	}

	float TimeStamp::millisecondsSince() const
	{
		return TimeStamp(clock().now() - _counter).milliseconds();
	}

	float TimeStamp::microsecondsSince() const
	{
		return TimeStamp(clock().now() - _counter).microseconds();
	}

	float TimeStamp::nanosecondsSince() const
	{
		return TimeStamp(clock().now() - _counter).nanoseconds();
	}

	float TimeStamp::seconds() const
	{
	#if defined(DEATH_TARGET_AMIGAOS)
		return CounterToFloat(_counter) / float(clock().frequency());
	#else
		return static_cast<float>(_counter) / clock().frequency();
	#endif
	}

	float TimeStamp::milliseconds() const
	{
	#if defined(DEATH_TARGET_AMIGAOS)
		return (CounterToFloat(_counter) / float(clock().frequency())) * 1000.0f;
	#else
		return (static_cast<float>(_counter) / clock().frequency()) * 1000.0f;
	#endif
	}

	float TimeStamp::microseconds() const
	{
	#if defined(DEATH_TARGET_AMIGAOS)
		return (CounterToFloat(_counter) / float(clock().frequency())) * 1000000.0f;
	#else
		return (static_cast<float>(_counter) / clock().frequency()) * 1000000.0f;
	#endif
	}

	float TimeStamp::nanoseconds() const
	{
	#if defined(DEATH_TARGET_AMIGAOS)
		return (CounterToFloat(_counter) / float(clock().frequency())) * 1000000000.0f;
	#else
		return (static_cast<float>(_counter) / clock().frequency()) * 1000000000.0f;
	#endif
	}
}
