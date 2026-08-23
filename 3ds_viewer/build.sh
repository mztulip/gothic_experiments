#!/usr/bin/env bash
set -e

TARGET="viewer3ds"
SRC="3ds_loader.cpp"

echo "Kompilacja Gothic 3DS Viewer (z uzyciem epoxy)..."

g++ -std=c++17 $SRC -o $TARGET \
    -lglfw \
    -lepoxy \
    -lGL \
    -lpthread

echo "Kompilacja zakonczenie sukcesem!"
echo "Uruchomienie przykladowe: ./$TARGET sciezka/do/pliku.3ds"

#export GOTHIC2_DIR="/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II"
#./viewer3ds
# ./viewer3ds "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Meshes/Items/IT_Potions/ItPo_Health_02.3ds"
