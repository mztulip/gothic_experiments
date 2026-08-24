#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZENKIT_DIR="$(realpath "$SCRIPT_DIR/../ZenKit")"
ZENKIT_BUILD_DIR="$ZENKIT_DIR/build"
IMGUI_DIR="$(realpath "$SCRIPT_DIR/../imgui")"
IMGUI_BUILD_DIR="$SCRIPT_DIR/.imgui_build"

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
# ImGui - kompiluj do jednej statycznej biblioteki RAZ, dziel miedzy
# pfxview i pfxproto. Przebudowuje sie automatycznie tylko gdy ktorys
# plik zrodlowy ImGui jest nowszy niz istniejace archiwum.
# ------------------------------------------------------------------
IMGUI_SRC=(
  "$IMGUI_DIR/imgui.cpp"
  "$IMGUI_DIR/imgui_draw.cpp"
  "$IMGUI_DIR/imgui_tables.cpp"
  "$IMGUI_DIR/imgui_widgets.cpp"
  "$IMGUI_DIR/backends/imgui_impl_glfw.cpp"
  "$IMGUI_DIR/backends/imgui_impl_opengl3.cpp"
)
IMGUI_LIB="$IMGUI_BUILD_DIR/libimgui.a"
IMGUI_INCLUDES="-I$IMGUI_DIR -I$IMGUI_DIR/backends"

needs_imgui_rebuild=0
if [[ ! -f "$IMGUI_LIB" ]]; then
  needs_imgui_rebuild=1
else
  for src in "${IMGUI_SRC[@]}"; do
    if [[ "$src" -nt "$IMGUI_LIB" ]]; then
      needs_imgui_rebuild=1
      break
    fi
  done
fi

if [[ "$needs_imgui_rebuild" == "1" ]]; then
  echo "=========================================="
  echo " Building ImGui (raz, dla obu programow)"
  echo "=========================================="
  mkdir -p "$IMGUI_BUILD_DIR"

  obj_files=()
  for src in "${IMGUI_SRC[@]}"; do
    obj="$IMGUI_BUILD_DIR/$(basename "${src%.cpp}").o"
    echo "  CXX $src"
    "$CXX" -std=c++20 $UTF8_FLAGS -O2 -c "$src" -o "$obj" $IMGUI_INCLUDES
    obj_files+=("$obj")
  done

  ar rcs "$IMGUI_LIB" "${obj_files[@]}"
  echo "  AR  $IMGUI_LIB"
else
  echo "=========================================="
  echo " ImGui juz zbudowany - pomijam"
  echo "=========================================="
fi
echo

# ------------------------------------------------------------------
# pfxview i pfxproto - dwa OSOBNE programy, oba linkowane do tych
# samych, juz zbudowanych bibliotek (ZenKit, squish, ImGui)
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
    $IMGUI_INCLUDES \
    "$ZENKIT_LIB" \
    "$SQUISH_LIB" \
    "$IMGUI_LIB" \
    -o "$out" \
    $LIBS
}

build_program "pfxview.cpp"    "pfxview"
build_program "pfx_proto.cpp"  "pfxproto"

echo
echo "=========================================="
echo " Build successful"
echo "=========================================="
echo
echo "Run:"
echo "  ./pfxview"
echo "  ./pfxproto"
echo