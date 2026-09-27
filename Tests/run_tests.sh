#!/usr/bin/env bash
# Builds and runs the PARASITE core tests. No Unreal installation needed:
# everything under Source/Parasite/Core is plain C++.
#
# -Wshadow matters: UnrealBuildTool compiles Core/ into the game module with
# variable shadowing treated as an error, so it has to be clean here too.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/parasite_coretests"

CXX="${CXX:-g++}"
"$CXX" -std=c++17 -Wall -Wextra -Wshadow -Werror -O1 \
	-o "$OUT" \
	"$ROOT/Tests/CoreTests.cpp" \
	"$ROOT/Source/Parasite/Core/MatchSim.cpp" \
	"$ROOT/Source/Parasite/Core/ParasiteRules.cpp"

"$OUT"
