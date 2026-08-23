#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZENKIT_DIR="$(realpath "$SCRIPT_DIR/../ZenKit")"
ZENKIT_BUILD_DIR="$ZENKIT_DIR/build"

CXX="${CXX:-g++}"

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

echo
echo "=========================================="
echo " Building list_lights"
echo "=========================================="

ZENKIT_LIB="$ZENKIT_BUILD_DIR/libzenkit.a"
SQUISH_LIB="$ZENKIT_BUILD_DIR/vendor/libsquish/libsquish.a"

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


IMGUI_SRC="../imgui/imgui.cpp ../imgui/imgui_draw.cpp ../imgui/imgui_tables.cpp ../imgui/imgui_widgets.cpp"
IMGUI_BACKENDS="../imgui/backends/imgui_impl_glfw.cpp ../imgui/backends/imgui_impl_opengl3.cpp"
LIBS="-lglfw -lepoxy -lGL -ldl -lpthread"
INCLUDES="-I ../imgui -I ../imgui/backends"

"$CXX" -std=c++20 pfxview.cpp \
  -std=c++20 \
  -I"$ZENKIT_DIR/include" \
  -I"$ZENKIT_DIR/vendor/glm" \
  "$ZENKIT_LIB" \
  "$SQUISH_LIB" \
  $IMGUI_SRC $IMGUI_BACKENDS \
  -o pfxview \
  $INCLUDES $LIBS

"$CXX" -std=c++20 pfx_proto.cpp \
  -std=c++20 \
  -I"$ZENKIT_DIR/include" \
  -I"$ZENKIT_DIR/vendor/glm" \
  "$ZENKIT_LIB" \
  "$SQUISH_LIB" \
  $IMGUI_SRC $IMGUI_BACKENDS \
  -o pfxproto \
  $INCLUDES $LIBS

echo
echo "=========================================="
echo " Build successful"
echo "=========================================="
echo
echo "Run:"
echo "  ./pfxview path/to/PARTICLEFX.DAT"


#."/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Scripts/_compiled/PARTICLEFX.DAT"