# Patched GCC16 FPU/ABI and collision suite: 2026-09-05

## Result

All 50 controlled runs passed:

- 24 FpuAbi runs x 31 checks = 744 successful checks.
- 24 CollisionTree runs x 1000 frames, 256 bodies per run.
- 2 additional CollisionTree runs x 4000 frames, 256 bodies per run.
- Total collision frames: 32000. The tree's reported pairs are checked
  against an independent integer-coordinate reference.
- No FAIL/FAILED results; every program completed and retained its selected FPCR.

## Matrix and toolchain

Eight hard-float builds: CPU target 68040/68060 x O2/O3 x
no-fast-math/fast-math. Each ran with:

| FPCR | Arithmetic precision | Rounding |
| --- | --- | --- |
| 0x00 | Extended | Nearest |
| 0x80 | Double | Nearest |
| 0x90 | Double | Toward zero |

The extra 4000-frame runs used 040/O2/no-fast-math with FPCR 0x00 and 0x80.
The inherited startup FPCR was 0x90; each test explicitly sets the selected
value, checks it at the end, and restores the inherited value on return.

Driver/SDK: `/opt/amiga-gcc16-rc9/bin/m68k-amigaos-g++`.
Patched compiler: `-B/home/arczi/toolchains/amiga-gcc16-modefix/`.
The patched frontends/backend are the previously tested local GCC16 build,
not a newly rebuilt copy of all the latest upstream changes after git pull.

Flags: `-std=gnu++14 -mcpu=68040/68060 -mhard-float -mcrt=nix20`,
O2/O3, explicitly `-fno-fast-math` or `-ffast-math`,
`-fomit-frame-pointer -fno-exceptions -fno-optimize-sibling-calls`.
The test and libstdc++ C99 compatibility headers are force-included.

WinUAE configuration: `Tests-folder.uae`, CPU/FPU 68040, JIT enabled.
The 060-targeted binaries were run in this same 040 emulator configuration;
this is not a hardware-68060 or PiStorm verification.

## Earlier UINT64_MAX conversion failures

The old harness expected round-to-nearest results unconditionally.
For positive UINT64_MAX, the expected results differ when rounding toward
zero or minus infinity:

| Result type | Nearest | Toward zero |
| --- | --- | --- |
| float bits | 5f800000 | 5f7fffff |
| double bits | 43f0000000000000 | 43efffffffffffff |

The controlled runs pass with the appropriate independent integer-bit oracle
in both modes. The two old hard-float failures match the toward-zero values
exactly. They are consistent with an inherited FPCR of 0x90, rather than
evidence of another conversion miscompile. The old ABI logs did not record
FPCR themselves, so this does not retroactively prove the mode of every old run.

The earlier GCC6 soft-float failures have not been retested here. Nor does
this matrix establish why some older collision-tree runs failed and later
ones passed, or prove that the mode-change patch fixed those old failures.

Despite the suspicious standalone multilib-directory selection reported for
the RC9 060 driver, the actually linked hard-float binaries passed this ABI
suite. Directory selection alone must not be presented as proof that these
particular linked binaries have an ABI mismatch.

## Artifacts and rerun

Output directory:
`/mnt/d/dev/uae-tests/FpuTests/modefix-suite-SmFeDe/`.

It contains 16 executables, the build log, `RunSuite`, the 50 individual
result logs, and the completion marker. ABI logs are named, for example,
`040-O2-plain-p00-abi.log`; tree logs use `-tree.log` or `-long.log`.

Build:

```sh
bash Tests/AmigaFpu/build-modefix-suite.sh /mnt/d/dev/uae-tests/FpuTests/modefix-suite-SmFeDe
```

Run on Amiga (writes logs to the same directory):

```text
execute 455gb:dev/uae-tests/FpuTests/modefix-suite-SmFeDe/RunSuite
```

The ordinary `uae-tests/test` game-launch script was backed up to
`modefix-suite-startup-backup-4DYxq1/test` and restored after the run.
No game executable, MiniGL or QuarkTex library was changed by this task.

## Limits

This is the existing focused ABI/math and game collision test suite, not
exhaustive floating-point verification. It does not cover all inputs, NaNs,
infinities, denormals, all libm functions, every FPCR mode, all optimization
passes or hardware/emulator variants. It found no additional failure in
the tested configurations; it cannot establish that GCC has no other bugs.
