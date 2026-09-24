#define WITH_AMIGA 1
#define JAZZ2_PROFILE_INTRO 1
#include "../Sources/nCine/Backends/Amiga/AmigaIntroProfile.h"
#include <cassert>

static std::uint64_t ticks;
std::int64_t nCine::Backends::IntroProfile::SystemTimeMs() { return ticks / 1000; }
std::uint64_t nCine::Backends::AmigaPlatform::TimerTicks() { return ticks; }
std::uint32_t nCine::Backends::AmigaPlatform::TimerFrequency() { return 1000000; }

int main()
{
	using namespace nCine::Backends::IntroProfile;
	{ Scope scope(Decode); ticks += 10; }
	assert(counters[Decode].calls == 0);
	Begin(160, 120);
	{ Scope scope(Decode); ticks += 3000; }
	{ Scope scope(Upload); ticks += 7000; }
	frames = 1;
	Begin(640, 480); // Repeated frame begins must not reset the film's counters.
	assert(textureWidth == 160 && counters[Decode].ticks == 3000);
	assert(counters[Upload].ticks == 7000);
	End();
	assert(!active);
	{ Scope scope(Decode); ticks += 10; }
	assert(counters[Decode].calls == 1);
	Begin(640, 480);
	assert(counters[Decode].calls == 0 && frames == 0 && textureWidth == 640);
	End();
}
