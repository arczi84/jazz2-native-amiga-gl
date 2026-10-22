#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$src/../.." && pwd)
out=${1:?Specify an output directory}
cxx=/opt/amiga-gcc16-rc9/bin/m68k-amigaos-g++
fixed=/home/arczi/toolchains/amiga-gcc16-modefix
mkdir -p "$out"
common=(-std=gnu++14 -B"$fixed/" -mcrt=nix20 -mhard-float
    -DTEST_HARD_FLOAT=1 -fomit-frame-pointer -fno-exceptions
    -fno-optimize-sibling-calls -include "$src/TestCompat.h"
    -include "$repo/cmake/toolchains/amiga-libstdc++-c99.h"
    -I"$repo/Sources" -I"$repo/Sources/Shared")
for cpu in 68040 68060; do
    for opt in O2 O3; do
        for math in plain fast; do
            tag="${cpu#68}-$opt-$math"
            flags=("${common[@]}" "-mcpu=$cpu" "-$opt" "-DTEST_VARIANT=\"modefix-$tag\"")
            if [[ $math == fast ]]; then flags+=(-ffast-math); else flags+=(-fno-fast-math); fi
            printf 'Building %s\n' "$tag"
            "$cxx" "${flags[@]}" -c "$src/FpuAbiMain.cpp" -o "$out/abi-main-$tag.o"
            "$cxx" "${flags[@]}" -c "$src/FpuAbiCallee.cpp" -o "$out/abi-callee-$tag.o"
            "$cxx" "${flags[@]}" "$out/abi-main-$tag.o" "$out/abi-callee-$tag.o" -lm -o "$out/Abi-$tag"
            "$cxx" "${flags[@]}" "$src/CollisionTreeTest.cpp" \
                "$repo/Sources/Jazz2/Collisions/DynamicTree.cpp" \
                "$repo/Sources/Jazz2/Collisions/DynamicTreeBroadPhase.cpp" \
                "$repo/Sources/Shared/Containers/SmallVector.cpp" \
                -lm -o "$out/Tree-$tag"
        done
    done
done
