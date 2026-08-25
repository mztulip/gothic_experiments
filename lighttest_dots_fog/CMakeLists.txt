cmake_minimum_required(VERSION 3.10)
project(lighttest CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(EPOXY REQUIRED epoxy)

find_package(glm REQUIRED)

# Sciezki do ZenKit - podaj przy wywolaniu cmake, np.:
#   cmake .. -DZENKIT_INCLUDE_DIR=/home/mz/gothic/zen/OpenGothic/lib/ZenKit/include \
#             -DZENKIT_LIB=/home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/libzenkit.a \
#             -SQUISH_OBJECTS_DIR=/home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/vendor/libsquish/CMakeFiles/squish.dir/
set(ZENKIT_INCLUDE_DIR "" CACHE PATH "Katalog include/ ZenKit")
set(ZENKIT_GLM_INCLUDE_DIR "" CACHE PATH "Katalog vendor/glm ZenKit (jesli inny niz systemowy glm)")
set(ZENKIT_LIB "" CACHE FILEPATH "Sciezka do libzenkit.a")

#set(SQUISH_LIB "" CACHE FILEPATH "Sciezka do libsquish.a")
set(SQUISH_OBJECTS_DIR
    "/home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/vendor/libsquish/CMakeFiles/squish.dir"
    CACHE PATH "Katalog z plikami .o Squisha")

file(GLOB SQUISH_OBJECTS
    "${SQUISH_OBJECTS_DIR}/*.o")

if(NOT ZENKIT_INCLUDE_DIR OR NOT ZENKIT_LIB OR NOT SQUISH_OBJECTS)
  message(FATAL_ERROR
    "Podaj sciezki do ZenKit, np.:\n"
    "  cmake .. -DZENKIT_INCLUDE_DIR=.../lib/ZenKit/include "
    "-DZENKIT_LIB=.../build/lib/ZenKit/libzenkit.a "
    "-DSQUISH_LIB=.../build/lib/ZenKit/vendor/libsquish/libsquish.a")
endif()

add_executable(lighttest main.cpp)
target_include_directories(lighttest PRIVATE
  ${GLFW_INCLUDE_DIRS} ${EPOXY_INCLUDE_DIRS}
  ${ZENKIT_INCLUDE_DIR} ${ZENKIT_GLM_INCLUDE_DIR})

target_link_libraries(lighttest PRIVATE
  ${GLFW_LIBRARIES}
  ${EPOXY_LIBRARIES}
  glm::glm
  ${ZENKIT_LIB}
  ${SQUISH_OBJECTS}
)

target_compile_options(lighttest PRIVATE ${GLFW_CFLAGS_OTHER} ${EPOXY_CFLAGS_OTHER})
