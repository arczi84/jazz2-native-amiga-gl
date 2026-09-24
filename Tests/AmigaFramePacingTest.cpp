#include "../Sources/nCine/Backends/Amiga/AmigaFramePacing.h"
#include <cassert>

int main()
{
	using nCine::Backends::ShouldYieldCinematicFrame;
	assert(!ShouldYieldCinematicFrame(100, 0, 709379));
	assert(!ShouldYieldCinematicFrame(100, 100, 0));
	assert(!ShouldYieldCinematicFrame(99, 100, 709379));
	assert(ShouldYieldCinematicFrame(100, 100, 709379));
	assert(ShouldYieldCinematicFrame(14287, 100, 709379));
	assert(!ShouldYieldCinematicFrame(14288, 100, 709379));
	assert(!ShouldYieldCinematicFrame(100000, 100, 709379));
	assert(ShouldYieldCinematicFrame(14418, 100, 715909));
	assert(!ShouldYieldCinematicFrame(14419, 100, 715909));
	// Must work after the low 32-bit EClock word wraps, too.
	const std::uint64_t high = std::uint64_t(1) << 32;
	assert(ShouldYieldCinematicFrame(high + 100, high - 100, 709379));
}
