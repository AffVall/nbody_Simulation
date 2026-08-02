cmake_minimum_required(VERSION 3.16)
project(nbody-sim LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# nlohmann/json (header-only)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

# GLM (header-only)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1)
FetchContent_MakeAvailable(glm)

# GLFW - use pre-built from trash
set(GLFW_PREBUILT_DIR "/home/valeria/.local/share/Trash/files/build.2/_deps/glfw-build/src")
set(GLFW_SRC_DIR "/home/valeria/.local/share/Trash/files/build.2/_deps/glfw-src")

add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${GLFW_PREBUILT_DIR}/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${GLFW_SRC_DIR}/include"
)

# GLAD
add_library(glad third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include/)

# ImGui
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui/ third_party/imgui/backends/)
target_link_libraries(imgui PUBLIC glfw glad)

# Executavel
add_executable(nbody
    src/main.cpp
    src/Body.cpp
    src/Physics.cpp
    src/PN.cpp
    src/BarnesHut.cpp
    src/Integrators.cpp
    src/IAS15.cpp
    src/AhmadCohen.cpp
    src/Collisions.cpp
    src/Diagnostics.cpp
    src/BinaryIO.cpp
    src/Renderer.cpp
    src/IO.cpp
)

target_include_directories(nbody PRIVATE src/)
target_link_libraries(nbody PRIVATE
    nlohmann_json::nlohmann_json
    glfw glad glm imgui
    GL dl pthread
)

find_package(X11 QUIET)
if(X11_FOUND)
    target_link_libraries(nbody PRIVATE X11::X11)
endif()
find_package(Threads REQUIRED)
target_link_libraries(nbody PRIVATE Threads::Threads)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(nbody PRIVATE -O3 -march=native -flto)
endif()
