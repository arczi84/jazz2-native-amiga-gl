#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_dir=$(cd "$script_dir/../.." && pwd)
output_dir=${1:-"$script_dir/build-controlled-thresholds"}
cxx=/opt/amiga-gcc16-rc7/bin/m68k-amigaos-g++

mkdir -p "$output_dir"

common=(
	-std=gnu++14 -mcpu=68060 -mcrt=nix20 -mhard-float
	-DTEST_HARD_FLOAT=1 -fomit-frame-pointer -fno-exceptions
	-fno-optimize-sibling-calls -include "$script_dir/TestCompat.h"
	-include "$repo_dir/cmake/toolchains/amiga-libstdc++-c99.h"
	-I"$repo_dir/Sources" -I"$repo_dir/Sources/Shared"
)

"$cxx" "${common[@]}" -O2 '-DTEST_VARIANT="controlled_tree_object"' -c \
	"$script_dir/CollisionTreeTest.cpp" -o "$output_dir/test.o"
"$cxx" "${common[@]}" -O2 -c \
	"$repo_dir/Sources/Jazz2/Collisions/DynamicTreeBroadPhase.cpp" -o "$output_dir/broad.o"
"$cxx" "${common[@]}" -O2 -c \
	"$repo_dir/Sources/Shared/Containers/SmallVector.cpp" -o "$output_dir/vector.o"

build_tree() {
	local name=$1
	shift
	printf 'Building %s: %s\n' "$name" "$*"
	"$cxx" "${common[@]}" "$@" -c \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTree.cpp" -o "$output_dir/$name.tree.o"
	"$cxx" -mcpu=68060 -mcrt=nix20 -mhard-float \
		"$output_dir/$name.tree.o" "$output_dir/test.o" "$output_dir/broad.o" "$output_dir/vector.o" \
		-lm -o "$output_dir/$name"
}

build_tree E00 -O2
for threshold in 6 7 8 9 10 11 12 13 14; do
	name=$(printf 'E%02d' "$threshold")
	build_tree "$name" -O2 "--param=early-inlining-insns=$threshold"
done
build_tree E99 -O3

rm -f "$output_dir"/*.o
printf 'Done: %s\n' "$output_dir"
