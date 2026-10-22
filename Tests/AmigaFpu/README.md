# Amiga m68k floating-point compiler tests

These tests distinguish floating-point arithmetic and ABI failures from the dynamic-tree collision failure seen in the Amiga hard-float build.

## Programs

- `FpuAbi.*` tests float/double returns across translation units, callbacks, virtual calls, varargs, 64-bit conversions, comparisons and selected libm calls. Results are compared and printed as integer IEEE-754 bit patterns; the test never uses `%f`.
- `CollisionTree.*` compiles the real Jazz2 `DynamicTree` and `DynamicTreeBroadPhase`. Its independent reference uses quarter-pixel integers, so a floating-point miscompile cannot make the tree and the reference agree accidentally.

## Build matrix

Run from Linux/WSL:

```sh
Tests/AmigaFpu/build-tests.sh
```

The script uses exactly:

- `/opt/amiga-gcc16-rc7/bin/m68k-amigaos-g++` (GCC 16.2 RC7)
- `/opt/amiga-gcc6-latest/bin/m68k-amigaos-g++` (GCC 6.5)

Each compiler is tested with `-mcpu=68060`, without an explicit `-m68881`, in these variants:

- `-msoft-float`: `-O0`, `-O2`, `-O3`, `-O2 -ffast-math`
- `-mhard-float`: `-O0`, `-O2`, `-O3`, `-O2 -ffast-math`

`TestCompat.h` supplies the public `std::uninitialized_move` wrapper missing
from GCC 6.5's C++ library. It does not alter any floating-point operation.

Copy the resulting executables from `Tests/AmigaFpu/build-amiga` to the Amiga and run them from Shell. A good run ends with `PASS`. A failure includes the first operation or collision pair that disagreed with the integer reference.

`CollisionTree.*` runs 4000 frames by default. An optional argument changes the
count, for example `CollisionTree.gcc16-rc7.hard.O2 500` for a quick run.

## Controlled FPCR and mode-change compiler fix

`FpuAbi` now accepts an optional FPCR value as its first argument.
`CollisionTree` accepts it after the frame count. For example:

```text
Abi-040-O2-plain 0x00
Tree-040-O2-plain 1000 0x00
```

Both programs save and restore the inherited FPCR and check that its value
does not change during the tests. `FpuAbi` therefore has 31 checks (the
original 30 plus FPCR preservation). Its UINT64_MAX conversion expectations
use integer bit patterns appropriate for the selected rounding direction;
rounding toward zero must not be judged against a nearest-rounding oracle.

Build the patched-compiler hard-float matrix with:

```sh
bash Tests/AmigaFpu/build-modefix-suite.sh /path/to/output
```

This uses RC9's driver/SDK with the patched `cc1`/`cc1plus` in
`/home/arczi/toolchains/amiga-gcc16-modefix`, for 040/060, O2/O3 and
fast-math/no-fast-math. It does not replace installed compilers or the game.

See [MODEFIX_SUITE_RESULTS.md](MODEFIX_SUITE_RESULTS.md) for the controlled
WinUAE run and its limitations.
