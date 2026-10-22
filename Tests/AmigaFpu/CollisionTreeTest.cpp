#include "../../Sources/Jazz2/Collisions/DynamicTreeBroadPhase.h"
#include "TestFpcr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using Jazz2::Collisions::DynamicTreeBroadPhase;
using nCine::AABBf;
using nCine::Vector2f;

#ifndef TEST_VARIANT
#define TEST_VARIANT "default"
#endif

namespace
{
	constexpr int BodyCount = 256;
	constexpr int DefaultFrameCount = 4000;
	constexpr int QuarterScale = 4;

	struct Body
	{
		int Id;
		int Proxy;
		int X;
		int Y;
		int W;
		int H;
		int Vx;
		int Vy;
		AABBf Box;
	};

	Body Bodies[BodyCount];
	unsigned char Seen[BodyCount][BodyCount];
	uint32_t RandomState = 0x6a09e667UL;

	uint32_t NextRandom()
	{
		RandomState ^= RandomState << 13;
		RandomState ^= RandomState >> 17;
		RandomState ^= RandomState << 5;
		return RandomState;
	}

	uint32_t FloatBits(float value)
	{
		uint32_t bits;
		memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	void SyncBox(Body& body)
	{
		body.Box = AABBf(
			static_cast<float>(body.X) * 0.25f,
			static_cast<float>(body.Y) * 0.25f,
			static_cast<float>(body.X + body.W) * 0.25f,
			static_cast<float>(body.Y + body.H) * 0.25f);
	}

	bool OverlapsFixed(const Body& a, const Body& b)
	{
		return a.X <= b.X + b.W && a.Y <= b.Y + b.H &&
			a.X + a.W >= b.X && a.Y + a.H >= b.Y;
	}

	struct PairCollector
	{
		void OnPairAdded(void* userDataA, void* userDataB)
		{
			Body* a = static_cast<Body*>(userDataA);
			Body* b = static_cast<Body*>(userDataB);
			if (OverlapsFixed(*a, *b)) {
				int low = (a->Id < b->Id ? a->Id : b->Id);
				int high = (a->Id < b->Id ? b->Id : a->Id);
				Seen[low][high] = 1;
			}
		}
	};

	bool VerifyFrame(int frame, DynamicTreeBroadPhase& broadPhase)
	{
		for (int i = 0; i < BodyCount; ++i) {
			for (int j = i + 1; j < BodyCount; ++j) {
				bool expected = OverlapsFixed(Bodies[i], Bodies[j]);
				bool actual = (Seen[i][j] != 0);
				if (actual != expected) {
					printf("FAIL frame=%ld pair=%ld,%ld got=%ld expected=%ld seed=%08lx\n",
						static_cast<long>(frame), static_cast<long>(i), static_cast<long>(j),
						static_cast<long>(actual), static_cast<long>(expected),
						static_cast<unsigned long>(RandomState));
					printf(" A fixed=%ld,%ld,%ld,%ld float=%08lx,%08lx,%08lx,%08lx\n",
						static_cast<long>(Bodies[i].X), static_cast<long>(Bodies[i].Y),
						static_cast<long>(Bodies[i].W), static_cast<long>(Bodies[i].H),
						static_cast<unsigned long>(FloatBits(Bodies[i].Box.L)),
						static_cast<unsigned long>(FloatBits(Bodies[i].Box.T)),
						static_cast<unsigned long>(FloatBits(Bodies[i].Box.R)),
						static_cast<unsigned long>(FloatBits(Bodies[i].Box.B)));
					printf(" B fixed=%ld,%ld,%ld,%ld float=%08lx,%08lx,%08lx,%08lx\n",
						static_cast<long>(Bodies[j].X), static_cast<long>(Bodies[j].Y),
						static_cast<long>(Bodies[j].W), static_cast<long>(Bodies[j].H),
						static_cast<unsigned long>(FloatBits(Bodies[j].Box.L)),
						static_cast<unsigned long>(FloatBits(Bodies[j].Box.T)),
						static_cast<unsigned long>(FloatBits(Bodies[j].Box.R)),
						static_cast<unsigned long>(FloatBits(Bodies[j].Box.B)));
					printf(" tree height=%ld balance=%ld qualityBits=%08lx\n",
						static_cast<long>(broadPhase.GetTreeHeight()),
						static_cast<long>(broadPhase.GetTreeBalance()),
						static_cast<unsigned long>(FloatBits(broadPhase.GetTreeQuality())));
					return false;
				}
			}
		}
		return true;
	}
}

int main(int argc, char** argv)
{
	TestFpcrGuard fpcrGuard;
	int frameCount = DefaultFrameCount;
	if (argc >= 2) {
		long requestedFrames = strtol(argv[1], nullptr, 10);
		if (requestedFrames > 0 && requestedFrames <= 1000000L) {
			frameCount = static_cast<int>(requestedFrames);
		}
	}

#if defined(__m68k__) && defined(TEST_HARD_FLOAT)
	if (argc >= 3) {
		TestWriteFpcr(static_cast<uint32_t>(strtoul(argv[2], nullptr, 0)));
	}
	const uint32_t fpcr = TestReadFpcr();
#else
	const uint32_t fpcr = 0xffffffffUL;
#endif

	printf("CollisionTreeTest compiler=%s hard=%d variant=%s fpcr=%08lx bodies=%ld frames=%ld scale=%ld\n", __VERSION__,
#if defined(TEST_HARD_FLOAT)
		1,
#else
		0,
#endif
		TEST_VARIANT,
		static_cast<unsigned long>(fpcr),
		static_cast<long>(BodyCount), static_cast<long>(frameCount), static_cast<long>(QuarterScale));

	DynamicTreeBroadPhase broadPhase;
	for (int i = 0; i < BodyCount; ++i) {
		Body& body = Bodies[i];
		body.Id = i;
		body.X = static_cast<int>(NextRandom() % 2048U) - 1024;
		body.Y = static_cast<int>(NextRandom() % 1024U) - 512;
		body.W = 32 + static_cast<int>(NextRandom() % 161U);
		body.H = 32 + static_cast<int>(NextRandom() % 161U);
		body.Vx = static_cast<int>(NextRandom() % 15U) - 7;
		body.Vy = static_cast<int>(NextRandom() % 15U) - 7;
		if (body.Vx == 0) body.Vx = 1;
		if (body.Vy == 0) body.Vy = -1;
		SyncBox(body);
		body.Proxy = broadPhase.CreateProxy(body.Box, &body);
	}

	PairCollector collector;
	memset(Seen, 0, sizeof(Seen));
	broadPhase.UpdatePairs(&collector);
	if (!VerifyFrame(0, broadPhase)) {
		return 1;
	}

	for (int frame = 1; frame <= frameCount; ++frame) {
		if ((frame % 97) == 0) {
			int index = static_cast<int>(NextRandom() % BodyCount);
			Body& body = Bodies[index];
			broadPhase.DestroyProxy(body.Proxy);
			body.X = static_cast<int>(NextRandom() % 2048U) - 1024;
			body.Y = static_cast<int>(NextRandom() % 1024U) - 512;
			SyncBox(body);
			body.Proxy = broadPhase.CreateProxy(body.Box, &body);
		}

		for (int i = 0; i < BodyCount; ++i) {
			Body& body = Bodies[i];
			if (body.X < -1280 || body.X + body.W > 1280) body.Vx = -body.Vx;
			if (body.Y < -768 || body.Y + body.H > 768) body.Vy = -body.Vy;
			body.X += body.Vx;
			body.Y += body.Vy;
			SyncBox(body);
			broadPhase.MoveProxy(body.Proxy, body.Box,
				Vector2f(static_cast<float>(body.Vx) * 0.25f, static_cast<float>(body.Vy) * 0.25f));
		}

		memset(Seen, 0, sizeof(Seen));
		broadPhase.UpdatePairs(&collector);
		if (!VerifyFrame(frame, broadPhase)) {
			return 1;
		}
		if ((frame % 500) == 0) {
			printf(" progress frame=%ld height=%ld balance=%ld qualityBits=%08lx\n",
				static_cast<long>(frame), static_cast<long>(broadPhase.GetTreeHeight()),
				static_cast<long>(broadPhase.GetTreeBalance()),
				static_cast<unsigned long>(FloatBits(broadPhase.GetTreeQuality())));
		}
	}

	const uint32_t finalFpcr = TestReadFpcr();
	if (finalFpcr != fpcr) {
		printf("FAIL FPCR changed got=%08lx expected=%08lx\n",
			static_cast<unsigned long>(finalFpcr), static_cast<unsigned long>(fpcr));
		return 1;
	}
	printf("final_fpcr=%08lx\n", static_cast<unsigned long>(finalFpcr));
	printf("PASS frames=%ld bodies=%ld finalSeed=%08lx\n",
		static_cast<long>(frameCount), static_cast<long>(BodyCount),
		static_cast<unsigned long>(RandomState));
	return 0;
}
