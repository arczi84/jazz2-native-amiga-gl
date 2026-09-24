#pragma once
#include <cstdint>

namespace nCine::Backends
{
	// AmigaDOS Delay uses 50 Hz ticks. A cinematic does not need an uncapped
	// presentation loop. Never add a wait when decoding/rendering already took
	// a full tick; never spin to fill a fractional tick.
	inline bool ShouldYieldCinematicFrame(std::uint64_t now, std::uint64_t previous,
		std::uint32_t frequency)
	{
		return previous != 0 && frequency != 0 && now >= previous &&
			(now - previous) < (std::uint64_t(frequency) + 49) / 50;
	}
}
