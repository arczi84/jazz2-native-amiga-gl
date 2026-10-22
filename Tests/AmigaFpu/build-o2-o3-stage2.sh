#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_dir=$(cd "$script_dir/../.." && pwd)
output_dir=${1:-"$script_dir/build-o2-o3-stage2"}
cxx=/opt/amiga-gcc16-rc7/bin/m68k-amigaos-g++

mkdir -p "$output_dir"

base=(
	-std=gnu++14 -mcpu=68060 -mcrt=nix20 -mhard-float
	-DTEST_HARD_FLOAT=1 -fomit-frame-pointer -fno-exceptions
	-fno-optimize-sibling-calls -include "$script_dir/TestCompat.h"
	-include "$repo_dir/cmake/toolchains/amiga-libstdc++-c99.h"
	-I"$repo_dir/Sources" -I"$repo_dir/Sources/Shared"
)

build_mixed() {
	local name=$1
	local label=$2
	local test_opt=$3
	local tree_opt=$4
	local broad_opt=$5
	local vector_opt=$6
	local object_dir="$output_dir/$name.objects"
	mkdir -p "$object_dir"
	printf 'Building %s: %s\n' "$name" "$label"
	"$cxx" "${base[@]}" "$test_opt" "-DTEST_VARIANT=\"$name:$label\"" -c \
		"$script_dir/CollisionTreeTest.cpp" -o "$object_dir/test.o"
	"$cxx" "${base[@]}" "$tree_opt" -c \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTree.cpp" -o "$object_dir/tree.o"
	"$cxx" "${base[@]}" "$broad_opt" -c \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTreeBroadPhase.cpp" -o "$object_dir/broad.o"
	"$cxx" "${base[@]}" "$vector_opt" -c \
		"$repo_dir/Sources/Shared/Containers/SmallVector.cpp" -o "$object_dir/vector.o"
	"$cxx" -mcpu=68060 -mcrt=nix20 -mhard-float "$object_dir"/*.o -lm -o "$output_dir/$name"
}

build_with_flags() {
	local name=$1
	local label=$2
	shift 2
	printf 'Building %s: %s\n' "$name" "$label"
	"$cxx" "${base[@]}" -O2 "$@" "-DTEST_VARIANT=\"$name:$label\"" \
		"$script_dir/CollisionTreeTest.cpp" \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTree.cpp" \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTreeBroadPhase.cpp" \
		"$repo_dir/Sources/Shared/Containers/SmallVector.cpp" \
		-lm -o "$output_dir/$name"
}

build_mixed C00 all_O2                 -O2 -O2 -O2 -O2
build_mixed C01 test_only_O3           -O3 -O2 -O2 -O2
build_mixed C02 dynamic_tree_only_O3   -O2 -O3 -O2 -O2
build_mixed C03 broad_phase_only_O3    -O2 -O2 -O3 -O2
build_mixed C04 small_vector_only_O3   -O2 -O2 -O2 -O3
build_mixed C05 test_and_tree_O3       -O3 -O3 -O2 -O2
build_mixed C06 all_O3                 -O3 -O3 -O3 -O3

build_with_flags C07 early_inline_14 --param=early-inlining-insns=14
build_with_flags C08 inline_hint_600 --param=inline-heuristics-hint-percent=600
build_with_flags C09 inline_speedup_15 --param=inline-min-speedup=15
build_with_flags C10 inline_auto_30 --param=max-inline-insns-auto=30
build_with_flags C11 inline_single_200 --param=max-inline-insns-single=200
build_with_flags C12 all_O3_inline_params \
	--param=early-inlining-insns=14 \
	--param=inline-heuristics-hint-percent=600 \
	--param=inline-min-speedup=15 \
	--param=max-inline-insns-auto=30 \
	--param=max-inline-insns-single=200

rm -rf "$output_dir"/*.objects
printf 'Done: %s\n' "$output_dir"
