# CMakeLists.txt dla pfxdump
#
# Uzycie (analogicznie do lighttest):
#   mkdir build && cd build
#   cmake .. -DZENKIT_INCLUDE_DIR=/home/mz/gothic/zen/OpenGothic/lib/ZenKit/include \
#             -DZENKIT_LIB=/home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/libzenkit.a
#   make
#
# Uruchomienie:
#   ./pfxdump "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Scripts/_compiled/ParticleFx.dat" --list
#   ./pfxdump "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Scripts/_compiled/ParticleFx.dat" PFX_IT_MAGICSPARKLE

cmake_minimum_required(VERSION 3.16)
project(pfxdump CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ZENKIT_INCLUDE_DIR)
  message(FATAL_ERROR "Podaj -DZENKIT_INCLUDE_DIR=/sciezka/do/ZenKit/include")
endif()
if(NOT ZENKIT_LIB)
  message(FATAL_ERROR "Podaj -DZENKIT_LIB=/sciezka/do/libzenkit.a")
endif()

add_executable(pfxdump pfxdump.cpp)
target_include_directories(pfxdump PRIVATE ${ZENKIT_INCLUDE_DIR})
target_link_libraries(pfxdump PRIVATE ${ZENKIT_LIB})

# ZenKit moze zalezec od squish (kompresja DXT) - jesli link sie nie powiedzie
# z brakujacymi symbolami squish::*, dolacz to samo co w lighttest:
# -DSQUISH_OBJECTS_DIR=/home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/vendor/libsquish/CMakeFiles/squish.dir/
