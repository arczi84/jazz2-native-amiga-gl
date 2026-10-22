#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_dir=$(cd "$script_dir/../.." && pwd)
output_dir=${1:-"$script_dir/build-early-inline-thresholds"}
cxx=/opt/amiga-gcc16-rc7/bin/m68k-amigaos-g++

mkdir -p "$output_dir"

common=(
	-std=gnu++14 -mcpu=68060 -mcrt=nix20 -mhard-float -O2
	-DTEST_HARD_FLOAT=1 -fomit-frame-pointer -fno-exceptions
	-fno-optimize-sibling-calls -include "$script_dir/TestCompat.h"
	-include "$repo_dir/cmake/toolchains/amiga-libstdc++-c99.h"
	-I"$repo_dir/Sources" -I"$repo_dir/Sources/Shared"
)

for threshold in 6 7 8 9 10 11 12 13 14; do
	name=$(printf 'D%02d' "$threshold")
	label="early_inline_$threshold"
	printf 'Building %s\n' "$name"
	"$cxx" "${common[@]}" "--param=early-inlining-insns=$threshold" \
		"-DTEST_VARIANT=\"$name:$label\"" \
		"$script_dir/CollisionTreeTest.cpp" \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTree.cpp" \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTreeBroadPhase.cpp" \
		"$repo_dir/Sources/Shared/Containers/SmallVector.cpp" \
		-lm -o "$output_dir/$name"
done

printf 'Done: %s\n' "$output_dir"
