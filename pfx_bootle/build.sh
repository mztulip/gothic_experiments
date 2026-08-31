#!/usr/bin/env bash
#
# build.sh - kompiluje gothic_bottle_demo bez CMake, bezposrednio przez g++
# uzywajac pkg-config do znalezienia flag dla epoxy i glfw3.
#
# Uzycie:
#   ./build.sh          - kompiluje (Release, -O2)
#   ./build.sh debug    - kompiluje z symbolami debugowymi (-g -O0)
#   ./build.sh run       - kompiluje i od razu uruchamia program
#   ./build.sh clean     - usuwa skompilowany plik wynikowy
#
# Wymagane pakiety (Ubuntu/Debian):
#   sudo apt install build-essential pkg-config libglfw3-dev libepoxy-dev libglm-dev

set -euo pipefail

SRC="main.cpp"
OUT="gothic_bottle_demo"
CXX="${CXX:-g++}"

MODE="${1:-release}"

if [ "$MODE" = "clean" ]; then
    echo "Usuwam '${OUT}'..."
    rm -f "${OUT}"
    exit 0
fi

# --- sprawdzenie wymaganych narzedzi ---
command -v pkg-config >/dev/null 2>&1 || { echo "Brak pkg-config. Zainstaluj: sudo apt install pkg-config" >&2; exit 1; }
command -v "${CXX}"   >/dev/null 2>&1 || { echo "Brak kompilatora ${CXX}. Zainstaluj: sudo apt install build-essential" >&2; exit 1; }

pkg-config --exists epoxy || { echo "Brak biblioteki epoxy (dev). Zainstaluj: sudo apt install libepoxy-dev" >&2; exit 1; }
pkg-config --exists glfw3 || { echo "Brak biblioteki glfw3 (dev). Zainstaluj: sudo apt install libglfw3-dev" >&2; exit 1; }

# GLM jest header-only - sprawdzamy tylko czy naglowek jest widoczny.
if ! echo "#include <glm/glm.hpp>" | ${CXX} -E -x c++ - >/dev/null 2>&1; then
    echo "Ostrzezenie: nie widac naglowkow GLM w standardowej sciezce include." >&2
    echo "Zainstaluj: sudo apt install libglm-dev  (albo dograj sciezke recznie w tym skrypcie)" >&2
fi

CXXFLAGS=(-std=c++17 -Wall -Wextra)
if [ "$MODE" = "debug" ]; then
    CXXFLAGS+=(-g -O0)
else
    CXXFLAGS+=(-O2)
fi

# Flagi z pkg-config (include dirs, linker flags)
PKG_CFLAGS=$(pkg-config --cflags epoxy glfw3)
PKG_LIBS=$(pkg-config --libs epoxy glfw3)

echo "Kompiluje ${SRC} -> ${OUT} (tryb: ${MODE})..."
# shellcheck disable=SC2086
"${CXX}" "${CXXFLAGS[@]}" ${PKG_CFLAGS} "${SRC}" -o "${OUT}" ${PKG_LIBS}

echo "Gotowe: ./${OUT}"

if [ "$MODE" = "run" ]; then
    exec "./${OUT}"
fi
