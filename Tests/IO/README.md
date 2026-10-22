# MemoryStream growth regression

Run `Tests/IO/run-memory-stream-growth.sh` from any directory with a C++20 compiler and a linker supporting `--wrap`.

The test writes 128 KiB one byte at a time, matching the QOI sprite encoder used during first-launch cache conversion. It counts actual reallocations and verifies every byte, overwriting without extending, truncation followed by append, and a fixed-size stream. The original implementation performs 114,688 reallocations; the corrected implementation performs six.

The same test was compiled with Amiga GCC 16.2 rc15 and run on an emulated m68k/68040 with libnix. A reduced real GOG TSF animation set also reproduces the slowdown with the original implementation and completes with geometric growth. No copyrighted game data is stored in these tests.
