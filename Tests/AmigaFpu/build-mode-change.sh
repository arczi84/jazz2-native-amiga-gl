#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new output directory}
fixed_dir=${2:-/tmp/m68k-fp-spill-SuvpJ1}
local_dir=/home/arczi/build/amiga-gcc16-fixed/gcc/gcc
cxx=/opt/amiga-gcc16-rc9/bin/m68k-amigaos-g++
mkdir -p "$out"
common=(-mhard-float -mcrt=nix20 -fno-exceptions -fno-optimize-sibling-calls)
for cpu in 68040 68060; do
    for opt in O2 O3; do
        for math in fno-fast-math ffast-math; do
            tag="$cpu.$opt.$math"
            flags=("${common[@]}" "-mcpu=$cpu" "-$opt" "-$math")
            "$cxx" "${flags[@]}" -Dleft=left_bad -c "$src/FpuModeChangeRepro.cpp" -o "$out/bad.$tag.o"
            "$cxx" "${flags[@]}" -B"$local_dir/" -Dleft=left_local -c "$src/FpuModeChangeRepro.cpp" -o "$out/local.$tag.o"
            "$cxx" "${flags[@]}" -B"$fixed_dir/" -Dleft=left_fixed -c "$src/FpuModeChangeRepro.cpp" -o "$out/fixed.$tag.o"
            "$cxx" "${flags[@]}" -fno-late-combine-instructions -Dleft=left_workaround -c "$src/FpuModeChangeRepro.cpp" -o "$out/workaround.$tag.o"
            "$cxx" "${common[@]}" "-mcpu=$cpu" -O0 -fno-fast-math -c "$src/FpuModeChangeRuntime.cpp" -o "$out/runtime.$tag.o"
            "$cxx" "${common[@]}" "-mcpu=$cpu" "$out/runtime.$tag.o" "$out/bad.$tag.o" "$out/local.$tag.o" "$out/fixed.$tag.o" "$out/workaround.$tag.o" -o "$out/ModeChange.$tag"
            echo "Built $tag"
        done
    done
done
