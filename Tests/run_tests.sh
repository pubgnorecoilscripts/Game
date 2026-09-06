#!/usr/bin/env bash
# Builds and runs the PARASITE core tests. No Unreal installation needed:
# everything under Source/Parasite/Core is plain C++.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/parasite_coretests"

CXX="${CXX:-g++}"
"$CXX" -std=c++17 -Wall -Wextra -Werror -O1 \
	-o "$OUT" \
	"$ROOT/Tests/CoreTests.cpp" \
	"$ROOT/Source/Parasite/Core/MatchSim.cpp" \
	"$ROOT/Source/Parasite/Core/ParasiteRules.cpp"

"$OUT"
