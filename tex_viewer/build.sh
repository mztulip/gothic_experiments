#!/usr/bin/env bash
set -e

# Pliki zrodlowe ImGui
IMGUI_SRC="../imgui/imgui.cpp ../imgui/imgui_draw.cpp ../imgui/imgui_tables.cpp ../imgui/imgui_widgets.cpp"
IMGUI_BACKENDS="../imgui/backends/imgui_impl_glfw.cpp ../imgui/backends/imgui_impl_opengl3.cpp"

LIBS="-lglfw -lepoxy -lGL -ldl -lpthread"
INCLUDES="-I ../imgui -I ../imgui/backends"


echo "=== 2/2 Kompilacja Gothic TEX/Texture Viewer ==="
g++ -std=c++17 tex_viewer.cpp $IMGUI_SRC $IMGUI_BACKENDS -o texviewer $INCLUDES $LIBS

echo "Kompilacja zakonczona sukcesem!"
echo "Uruchomienie TEX: ./texviewer"

#export GOTHIC2_DIR="/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II"
