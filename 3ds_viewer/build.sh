#!/usr/bin/env bash
set -e

TARGET="viewer3ds"
SRC="3ds_loader.cpp"

# Pliki zrodlowe Dear ImGui oraz backendow
IMGUI_SRC="imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp"
IMGUI_BACKENDS="imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp"

echo "Kompilacja Gothic 3DS Viewer (z uzyciem epoxy + Dear ImGui)..."

g++ -std=c++17 $SRC $IMGUI_SRC $IMGUI_BACKENDS -o $TARGET \
    -I imgui \
    -I imgui/backends \
    -lglfw \
    -lepoxy \
    -lGL \
    -ldl \
    -lpthread

echo "Kompilacja zakonczona sukcesem!"
echo "Uruchomienie przykladowe: ./$TARGET sciezka/do/pliku.3ds"
#export GOTHIC2_DIR="/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II"
#./viewer3ds
# ./viewer3ds "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Meshes/Items/IT_Potions/ItPo_Health_02.3ds"
