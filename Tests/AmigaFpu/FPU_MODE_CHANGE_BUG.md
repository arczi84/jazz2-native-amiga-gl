# m68k hard-float: late_combine2 misinterprets an XF spill as SF

## Reproduction

Confirmed with `/opt/amiga-gcc16-rc9/bin/m68k-amigaos-g++`, reporting
`16.2.0b 20260825082934`:

```sh
m68k-amigaos-g++ -mcpu=68040 -mhard-float -mcrt=nix20 \
  -O2 -fno-fast-math -fno-optimize-sibling-calls \
  -fdump-rtl-all -S FpuModeChangeRepro.cpp -o repro.s
```

The failing sequence is:

```asm
fmove.x fp0,(sp)
jsr __Z16change_animationv
ftst.s 8(sp)
fslt d0
```

For x = -1.0f, the low 32 bits of the extended-precision spill are zero.
The generated code tests that zero instead of the negative input, producing
false for `x < 0.0f`. This does not depend on fast-math or a math library call.

The same pattern was identified in the game's `Player::HandleHorizontalMovement`
after `SetAnimation` and before the facing-direction decision.

## Exact compiler path

The `326r.reload` and `327r.postreload` dumps still contain the correct pair:

```
(set (reg:XF 16 fp0) (mem:XF (reg:SI 15 sp)))
(set (...) (lt (reg:SF 16 fp0) (const_double:SF 0)))
```

`328r.late_combine2` deletes the reload and substitutes
`(mem:SF (plus (reg:SI 15 sp) (const_int 8)))` into the comparison.

In the inspected local GCC sources:

- `gcc/late-combine.cc`, `insn_combination::substitute_nondebug_use`, around
  line 210: invokes `insn_propagation`.
- `gcc/recog.cc`, `insn_propagation::apply_to_rvalue_1`, lines 1073-1094:
  checks `REG_CAN_CHANGE_MODE_P`, then calls `simplify_subreg` using
  `subreg_lowpart_offset`.
- `gcc/config/m68k/m68k.cc`: does not implement
  `TARGET_CAN_CHANGE_MODE_CLASS`; the default therefore permits the bitcast.
- `gcc/target.def`, around line 5825: the hook contract explicitly requires
  preservation of the common low-order bits, not numerical conversion.

An m68k FPU register does not hold the SF/DF memory bit pattern. Its
floating-point conversion on load/store makes that lowpart substitution invalid.

## Fix

Apply `m68k-fpu-mode-change.patch` to the GCC source root. It registers
`m68k_can_change_mode_class`, rejecting different-mode bitcasts for register
classes that intersect `FP_REGS`:

```cpp
return from == to || !reg_classes_intersect_p (FP_REGS, rclass);
```

This fixes the target's mode-change description instead of disabling
hard-float or changing the game's direction logic. It follows the same
principle as the existing x87 restriction in `ix86_can_change_mode_class`.

Temporary flag-only workaround, independently checked on the installed RC9:
`-fno-late-combine-instructions`. Hard-float remains enabled.

## Verification scope (2026-09-05)

The installed RC9 and the existing locally built `cc1plus` both reproduce
the invalid instruction sequence. A patched `m68k.cc` copy was compiled
and linked into a separate test `cc1plus`, using the existing local GCC
build objects. Local source HEAD: `9529cbe26`; this is not asserted to be
an exact source match for the RC9 distribution.

The patched compiler retains:

```asm
fmove.x fp0,(sp)
jsr __Z16change_animationv
fmove.x (sp),fp0
ftst.x fp0
```

Assembly checked for all eight combinations of 68040/68060, O2/O3,
and fast-math/no-fast-math, always with hard-float and with late combine
left enabled. Each retains the XF reload and register comparison.

The initial verification above is focused on code generation. An emulator
runtime test was subsequently completed below. Neither is a full GCC
regression suite or game test. Menu/font defects have not been established
to share this cause. Installed compilers, the game executable, MiniGL and
QuarkTex were not replaced.

Temporary build, assembly and RTL evidence: `/tmp/m68k-fp-spill-SuvpJ1`.
The original compiler source file was not modified; the patch is supplied
separately for review/application.

## WinUAE runtime verification (2026-09-05)

`FpuModeChangeRuntime.cpp` calls four separately compiled versions of the
reproducer: installed RC9, unmodified local GCC, patched local GCC, and
installed RC9 with `-fno-late-combine-instructions`. The harness itself is
compiled at O0 without fast-math. All four functions are linked into each
executable and tested on the same input in the same emulator process.
The animation stub explicitly clobbers fp0/fp1 to exercise caller saves.

Sixteen inputs cover both signs, signed zero, exact threshold values, and
adjacent floats around the threshold. Inputs are specified by their bit
patterns and expectations are integer constants, avoiding dependence on
the same floating-point comparison in the test oracle.

Built and ran all eight 040/060 x O2/O3 x fast/plain variants, always with
hard-float. Emulator configuration: `Tests-folder.uae`, CPU/FPU 68040.
The 060-targeted binaries also ran on this 040 emulator configuration;
this is not a separate verification of 060 CPU emulation.

Each of the eight logs reports:

```
SUMMARY cases=16 rc9_errors=5 local_errors=5 fixed_errors=0 workaround_errors=0
RESULT PASS
```

Total: original RC9 40/128 wrong, original local GCC 40/128 wrong,
patched GCC 0/128 wrong, flag-only workaround 0/128 wrong.
For input bits `bf800000` (-1.0f), expected=1, rc9=0, local=0,
fixed=1, workaround=1.

Build command:

```sh
bash build-mode-change.sh /mnt/d/dev/uae-tests/FpuTests/mode-change-20260905
```

Amiga command (writes each result to a separate on-disk log):

```
execute 455gb:dev/uae-tests/FpuTests/mode-change-20260905/RunModeChange
```

Logs and test executables are in
`/mnt/d/dev/uae-tests/FpuTests/mode-change-20260905/`.
The temporary startup substitution was reverted after the tests; the
`uae-tests/test` script again launches the unchanged `Jazz2.gcc16-rc9`.
