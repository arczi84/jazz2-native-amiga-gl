#pragma once

// Opt-in test instrumentation only; no tracing or timing in release builds.
#if defined(WITH_AMIGA) && defined(JAZZ2_PROFILE_INTRO)
#include "AmigaPlatform.h"
#include <cstdio>

namespace nCine::Backends::IntroProfile
{
	enum Stage { Decode, Prepare, Sounds, Bake, Upload, QueueDraw, DrawCall, PresentFlush, Swap, Limit, CinematicWait, Count };
	std::int64_t SystemTimeMs(); // AmigaDOS DateStamp, sampled only at film boundaries.
	struct Counter { std::uint64_t ticks = 0; unsigned long calls = 0; };
	inline Counter counters[Count];
	inline bool active = false;
	inline std::uint64_t start = 0;
	inline std::int64_t systemStart = 0;
	inline unsigned long frames = 0;
	inline unsigned long textureWidth = 0, textureHeight = 0;

	inline void Begin(unsigned long width, unsigned long height)
	{
		if (active) return;
		for (auto& counter : counters) counter = {};
		frames = 0;
		textureWidth = width;
		textureHeight = height;
		start = AmigaPlatform::TimerTicks();
		systemStart = SystemTimeMs();
		active = true;
	}

	struct Scope
	{
		Stage stage;
		bool enabled;
		std::uint64_t start;
		explicit Scope(Stage value) : stage(value), enabled(active),
			start(enabled ? AmigaPlatform::TimerTicks() : 0) {}
		~Scope()
		{
			if (enabled) {
				counters[stage].ticks += AmigaPlatform::TimerTicks() - start;
				++counters[stage].calls;
			}
		}
	};

	inline void End()
	{
		if (!active) return;
		const auto elapsed = AmigaPlatform::TimerTicks() - start;
		const auto systemElapsed = SystemTimeMs() - systemStart;
		active = false;
		const auto frequency = AmigaPlatform::TimerFrequency();
		if (!frequency) return;
		std::printf("INTRO_PROFILE texture=%lux%lu decoded=%lu elapsed_ms=%lu\n",
			textureWidth, textureHeight, frames, (unsigned long)(elapsed * 1000 / frequency));
		std::printf("INTRO_PROFILE version=3 eclock_hz=%lu dos_elapsed_ms=%ld\n",
			(unsigned long)frequency, (long)systemElapsed);
		const char* names[] = { "decode", "prepare_cpu", "sounds", "palette_bake", "gl_upload",
			"queue_draw_inclusive", "gl_draw", "present_flush_inclusive", "sdl_swap", "frame_limit", "cinematic_wait" };
		for (unsigned i = 0; i < Count; ++i) {
			const auto& c = counters[i];
			std::printf("INTRO_PROFILE %s calls=%lu total_ms=%lu mean_us=%lu\n", names[i],
				c.calls, (unsigned long)(c.ticks * 1000 / frequency),
				(unsigned long)(c.calls ? c.ticks * 1000000 / frequency / c.calls : 0));
		}
		std::fflush(stdout);
	}
}
#endif
