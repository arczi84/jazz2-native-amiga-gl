#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_dir=$(cd "$script_dir/../.." && pwd)
output_dir=${1:-"$script_dir/build-amiga"}

mkdir -p "$output_dir"

build_compiler() {
	local compiler_name=$1
	local compiler_root=$2
	local cxx="$compiler_root/bin/m68k-amigaos-g++"

	if [[ ! -x "$cxx" ]]; then
		printf 'Missing compiler: %s\n' "$cxx" >&2
		return 1
	fi

	local cxx_compat=()
	if [[ "$compiler_name" == "gcc16-rc7" ]]; then
		cxx_compat=(-include "$repo_dir/cmake/toolchains/amiga-libstdc++-c99.h")
	fi

	local modes=(soft hard)
	local variants=(O0 O2 O3 O2-fast)
	for mode in "${modes[@]}"; do
		local float_flags=(-msoft-float)
		local mode_define=()
		if [[ "$mode" == "hard" ]]; then
			float_flags=(-mhard-float)
			mode_define=(-DTEST_HARD_FLOAT=1)
		fi

		for variant in "${variants[@]}"; do
			local opt_flags=(-O0)
			case "$variant" in
				O2) opt_flags=(-O2) ;;
				O3) opt_flags=(-O3) ;;
				O2-fast) opt_flags=(-O2 -ffast-math) ;;
			esac

			local tag="$compiler_name.$mode.$variant"
			local common=(-std=gnu++14 -mcpu=68060 -mcrt=nix20 -fomit-frame-pointer -fno-exceptions
				-fno-optimize-sibling-calls -include "$script_dir/TestCompat.h"
				"${float_flags[@]}" "${opt_flags[@]}" "${mode_define[@]}" "${cxx_compat[@]}")

			printf 'Building %s\n' "$tag"
			"$cxx" "${common[@]}" -I"$script_dir" -c "$script_dir/FpuAbiMain.cpp" -o "$output_dir/FpuAbiMain.$tag.o"
			"$cxx" "${common[@]}" -I"$script_dir" -c "$script_dir/FpuAbiCallee.cpp" -o "$output_dir/FpuAbiCallee.$tag.o"
			"$cxx" -mcpu=68060 -mcrt=nix20 "${float_flags[@]}" \
				"$output_dir/FpuAbiMain.$tag.o" "$output_dir/FpuAbiCallee.$tag.o" -lm \
				-o "$output_dir/FpuAbi.$tag"

			"$cxx" "${common[@]}" -I"$repo_dir/Sources" -I"$repo_dir/Sources/Shared" \
				"$script_dir/CollisionTreeTest.cpp" \
				"$repo_dir/Sources/Jazz2/Collisions/DynamicTree.cpp" \
				"$repo_dir/Sources/Jazz2/Collisions/DynamicTreeBroadPhase.cpp" \
				"$repo_dir/Sources/Shared/Containers/SmallVector.cpp" \
				-lm -o "$output_dir/CollisionTree.$tag"
		done
	done
}

build_compiler gcc16-rc7 /opt/amiga-gcc16-rc7
build_compiler gcc6.5 /opt/amiga-gcc6-latest

rm -f "$output_dir"/*.o
printf 'Done. Amiga executables: %s\n' "$output_dir"
