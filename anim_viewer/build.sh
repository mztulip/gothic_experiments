#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZENKIT_DIR="$(realpath "$SCRIPT_DIR/../ZenKit")"
ZENKIT_BUILD_DIR="$ZENKIT_DIR/build"
ZENKIT_LIB="$ZENKIT_BUILD_DIR/libzenkit.a"     # placeholder, doprecyzowane po buildzie
SQUISH_LIB="$ZENKIT_BUILD_DIR/vendor/libsquish/libsquish.a"  # placeholder

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

if [[ ! -f "$ZENKIT_DIR/CMakeLists.txt" ]]; then
  echo "ZenKit submodule nie jest zainicjalizowany - uruchamiam git submodule update --init --recursive"
  git -C "$ZENKIT_DIR/.." submodule update --init --recursive
fi

# ------------------------------------------------------------------
# ZenKit - buduj tylko jesli biblioteki jeszcze nie istnieja, albo
# jesli jawnie zazadano przebudowy przez --rebuild-zenkit
# ------------------------------------------------------------------
ZENKIT_FLAGS_FILE="$ZENKIT_BUILD_DIR/CMakeFiles/zenkit.dir/flags.make"

if [[ "$REBUILD_ZENKIT" == "1" || ! -f "$ZENKIT_LIB" || ! -f "$SQUISH_LIB" || ! -f "$ZENKIT_FLAGS_FILE" ]]; then
  echo "=========================================="
  echo " Building ZenKit"
  echo "=========================================="
  echo "Source: $ZENKIT_DIR"
  echo "Build : $ZENKIT_BUILD_DIR"
  echo

  cmake -S "$ZENKIT_DIR" \
    -B "$ZENKIT_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DZK_BUILD_TESTS=OFF \
    -DZK_BUILD_EXAMPLES=OFF \
    -DCMAKE_DEBUG_POSTFIX="" \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -g -fno-omit-frame-pointer" \
    -DCMAKE_C_FLAGS="-fsanitize=address -g -fno-omit-frame-pointer"

  cmake --build "$ZENKIT_BUILD_DIR" -j"$(nproc)"
else
  echo "=========================================="
  echo " ZenKit juz zbudowany - pomijam (uzyj --rebuild-zenkit aby wymusic)"
  echo "=========================================="
fi

if [[ -f "$ZENKIT_BUILD_DIR/libzenkitd.a" ]]; then
  ZENKIT_LIB="$ZENKIT_BUILD_DIR/libzenkitd.a"
else
  ZENKIT_LIB="$ZENKIT_BUILD_DIR/libzenkit.a"
fi


if [[ -f "$ZENKIT_BUILD_DIR/vendor/libsquish/libsquishd.a" ]]; then
  SQUISH_LIB="$ZENKIT_BUILD_DIR/vendor/libsquish/libsquishd.a"
else
  SQUISH_LIB="$ZENKIT_BUILD_DIR/vendor/libsquish/libsquish.a"
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
# Wyciagamy DOKLADNIE te same -D... co uzyl CMake do zbudowania
# libzenkit.a, zeby main.cpp mial identyczne ABI (ten sam layout
# klas jak w #ifdef _ZK_WITH_MMAP itp.) - zamiast recznie
# duplikowac flagi, ktore moga sie rozjechac przy zmianie configu.
# ------------------------------------------------------------------
ZENKIT_FLAGS_FILE="$ZENKIT_BUILD_DIR/CMakeFiles/zenkit.dir/flags.make"
ZENKIT_DEFINES=""

if [[ -f "$ZENKIT_FLAGS_FILE" ]]; then
  ZENKIT_DEFINES="$(grep '^CXX_DEFINES' "$ZENKIT_FLAGS_FILE" | sed 's/^CXX_DEFINES *= *//')"
  echo "Wykryte definicje z ZenKit: $ZENKIT_DEFINES"
else
  echo "OSTRZEZENIE: nie znaleziono $ZENKIT_FLAGS_FILE - definicje ABI moga sie nie zgadzac!"
fi

# ------------------------------------------------------------------
# ImGui - kompiluj do jednej statycznej biblioteki, tylko jesli trzeba
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
  echo " Building ImGui"
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
# samych, juz zbudowanych bibliotek (ZenKit, squish)
# ------------------------------------------------------------------
LIBS="-lglfw -lepoxy -lGL -ldl -lpthread"

build_program() {
  local src="$1"
  local out="$2"

  echo "=========================================="
  echo " Building $out"
  echo "=========================================="

  "$CXX" -std=c++20 -g -fsanitize=address -fno-omit-frame-pointer  $UTF8_FLAGS "$src" \
    $ZENKIT_DEFINES \
    -I"$ZENKIT_DIR/include" \
    -I"$ZENKIT_DIR/vendor/glm" \
    $IMGUI_INCLUDES \
    "$ZENKIT_LIB" \
    "$SQUISH_LIB" \
    "$IMGUI_LIB" \
    -o "$out" \
    $LIBS
}

build_program "main.cpp"    "animviewer"

echo
echo "=========================================="
echo " Build successful"
echo "=========================================="

#./animviewer "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Anims/_compiled/ALLIGATOR.MDM"