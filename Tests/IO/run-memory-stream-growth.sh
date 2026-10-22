#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
"${CXX:-c++}" -std=c++20 -O2 -DDEATH_NO_THREADS -I "$repo/Sources/Shared" \
  "$repo/Tests/IO/MemoryStreamGrowth.cpp" \
  "$repo/Sources/Shared/IO/MemoryStream.cpp" "$repo/Sources/Shared/IO/Stream.cpp" \
  "$repo/Sources/Shared/Containers/String.cpp" "$repo/Sources/Shared/Containers/StringView.cpp" \
  "$repo/Sources/Shared/Containers/SmallVector.cpp" "$repo/Sources/Shared/Base/Format.cpp" \
  "$repo/Sources/Shared/Utf8.cpp" -Wl,--wrap=realloc -o "$out/test"
"$out/test"
