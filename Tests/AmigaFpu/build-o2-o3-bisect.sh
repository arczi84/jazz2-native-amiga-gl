#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_dir=$(cd "$script_dir/../.." && pwd)
output_dir=${1:-"$script_dir/build-o2-o3-bisect"}
cxx=/opt/amiga-gcc16-rc7/bin/m68k-amigaos-g++

mkdir -p "$output_dir"

labels=(
	baseline_O2
	gcse_after_reload
	ipa_cp_clone
	loop_interchange
	loop_unroll_and_jam
	peel_loops
	predictive_commoning
	split_loops
	split_paths
	tree_loop_distribution
	tree_partial_pre
	unroll_completely_grow_size
	unswitch_loops
	vect_cost_dynamic
	version_loops_for_strides
	all_O3_additions
	baseline_O3
)

flags=(
	""
	"-fgcse-after-reload"
	"-fipa-cp-clone"
	"-floop-interchange"
	"-floop-unroll-and-jam"
	"-fpeel-loops"
	"-fpredictive-commoning"
	"-fsplit-loops"
	"-fsplit-paths"
	"-ftree-loop-distribution"
	"-ftree-partial-pre"
	"-funroll-completely-grow-size"
	"-funswitch-loops"
	"-fvect-cost-model=dynamic"
	"-fversion-loops-for-strides"
	"-fgcse-after-reload -fipa-cp-clone -floop-interchange -floop-unroll-and-jam -fpeel-loops -fpredictive-commoning -fsplit-loops -fsplit-paths -ftree-loop-distribution -ftree-partial-pre -funroll-completely-grow-size -funswitch-loops -fvect-cost-model=dynamic -fversion-loops-for-strides"
	"-O3"
)

common=(
	-std=gnu++14 -mcpu=68060 -mcrt=nix20 -mhard-float -O2
	-fomit-frame-pointer -fno-exceptions -fno-optimize-sibling-calls
	-include "$script_dir/TestCompat.h"
	-include "$repo_dir/cmake/toolchains/amiga-libstdc++-c99.h"
	-I"$repo_dir/Sources" -I"$repo_dir/Sources/Shared"
)

for index in "${!labels[@]}"; do
	label=${labels[$index]}
	read -r -a extra <<< "${flags[$index]}"
	name=$(printf 'B%02d' "$index")
	printf 'Building %s: %s\n' "$name" "$label"
	"$cxx" "${common[@]}" "${extra[@]}" "-DTEST_VARIANT=\"$name:$label\"" \
		"$script_dir/CollisionTreeTest.cpp" \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTree.cpp" \
		"$repo_dir/Sources/Jazz2/Collisions/DynamicTreeBroadPhase.cpp" \
		"$repo_dir/Sources/Shared/Containers/SmallVector.cpp" \
		-lm -o "$output_dir/$name"
done

printf 'Done: %s\n' "$output_dir"
