cmake_minimum_required(VERSION 3.10)
project(gothic_bottle_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EPOXY REQUIRED epoxy)
pkg_check_modules(GLFW REQUIRED glfw3)

# GLM jest header-only. Jesli nie masz go zainstalowanego systemowo,
# zainstaluj: sudo apt install libglm-dev  (Debian/Ubuntu)
find_path(GLM_INCLUDE_DIR glm/glm.hpp)

add_executable(gothic_bottle_demo main.cpp)

target_include_directories(gothic_bottle_demo PRIVATE
    ${EPOXY_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIR}
)

target_link_libraries(gothic_bottle_demo PRIVATE
    ${EPOXY_LIBRARIES}
    ${GLFW_LIBRARIES}
)

target_compile_options(gothic_bottle_demo PRIVATE
    ${EPOXY_CFLAGS_OTHER}
    ${GLFW_CFLAGS_OTHER}
)
