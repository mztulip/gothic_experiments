#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZENKIT_DIR="$(realpath "$SCRIPT_DIR/../ZenKit")"
ZENKIT_BUILD_DIR="$ZENKIT_DIR/build"

CXX="${CXX:-g++}"

# Flagi UTF-8 dla kompilatora (GCC / Clang)
UTF8_FLAGS="-finput-charset=UTF-8 -fexec-charset=UTF-8"

# --rebuild-zenkit wymusza pelna przebudowe ZenKit, nawet jesli biblioteki
# juz istnieja (przydatne po zmianie samego ZenKit, np. git pull).
REBUILD_ZENKIT=0
if [[ "${1:-}" == "--rebuild-zenkit" ]]; then
  REBUILD_ZENKIT=1
fi

ZENKIT_LIB="$ZENKIT_BUILD_DIR/libzenkit.a"
SQUISH_LIB="$ZENKIT_BUILD_DIR/vendor/libsquish/libsquish.a"

# ------------------------------------------------------------------
# ZenKit - buduj tylko jesli biblioteki jeszcze nie istnieja, albo
# jesli jawnie zazadano przebudowy przez --rebuild-zenkit
# ------------------------------------------------------------------
if [[ "$REBUILD_ZENKIT" == "1" || ! -f "$ZENKIT_LIB" || ! -f "$SQUISH_LIB" ]]; then
  echo "=========================================="
  echo " Building ZenKit"
  echo "=========================================="
  echo "Source: $ZENKIT_DIR"
  echo "Build : $ZENKIT_BUILD_DIR"
  echo

  cmake -S "$ZENKIT_DIR" \
    -B "$ZENKIT_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DZK_BUILD_TESTS=OFF

  cmake --build "$ZENKIT_BUILD_DIR" -j"$(nproc)"
else
  echo "=========================================="
  echo " ZenKit juz zbudowany - pomijam (uzyj --rebuild-zenkit aby wymusic)"
  echo "=========================================="
fi
echo

if [[ ! -f "$ZENKIT_LIB" ]]; then
  echo "ERROR: Nie znaleziono:"
  echo "  $ZENKIT_LIB"
  exit 1
fi

if [[ ! -f "$SQUISH_LIB" ]]; then
  echo "ERROR: Nie znaleziono:"
  echo "  $SQUISH_LIB"
  exit 1
fi


# ------------------------------------------------------------------
# pfxview i pfxproto - dwa OSOBNE programy, oba linkowane do tych
# samych, juz zbudowanych bibliotek (ZenKit, squish)
# ------------------------------------------------------------------
LIBS="-lglfw -lepoxy -lGL -ldl -lpthread"

build_program() {
  local src="$1"
  local out="$2"

  echo "=========================================="
  echo " Building $out"
  echo "=========================================="

  "$CXX" -std=c++20 $UTF8_FLAGS "$src" \
    -I"$ZENKIT_DIR/include" \
    -I"$ZENKIT_DIR/vendor/glm" \
    "$ZENKIT_LIB" \
    "$SQUISH_LIB" \
    -o "$out" \
    $LIBS
}

build_program "main.cpp"    "lighttest"

echo
echo "=========================================="
echo " Build successful"
echo "=========================================="
echo
echo "Run:"
echo "  ./lighttest"
echo