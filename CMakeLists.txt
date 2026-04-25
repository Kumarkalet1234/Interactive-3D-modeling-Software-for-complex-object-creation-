cmake_minimum_required(VERSION 3.16)
project(AeroMash VERSION 1.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Source files ──────────────────────────────
add_executable(AeroMash
    src/main.cpp
    src/geometry.cpp
    src/shaders.cpp
    src/io.cpp
    src/sculpt.cpp
    src/ui.cpp

    # Vendor — include these in your project or as submodules
    vendor/glad/src/glad.c
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

# ── Include dirs ──────────────────────────────
target_include_directories(AeroMash PRIVATE
    src/
    vendor/glad/include
    vendor/imgui
    vendor/imgui/backends
    vendor/glm
)

# ── Find packages ─────────────────────────────
find_package(OpenGL REQUIRED)

# GLFW — prefer system install, fall back to submodule
find_package(glfw3 QUIET)
if (NOT glfw3_FOUND)
    message(STATUS "GLFW3 not found via find_package — trying vendor/glfw3")
    add_subdirectory(vendor/glfw3 EXCLUDE_FROM_ALL)
endif()

# ── Link ──────────────────────────────────────
target_link_libraries(AeroMash PRIVATE
    OpenGL::GL
    glfw
    ${CMAKE_DL_LIBS}   # -ldl on Linux
)

# ── Platform specifics ────────────────────────
if (WIN32)
    target_link_libraries(AeroMash PRIVATE opengl32)
    target_compile_definitions(AeroMash PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
elseif (APPLE)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(IOKIT_LIB IOKit REQUIRED)
    target_link_libraries(AeroMash PRIVATE ${COCOA_LIB} ${IOKIT_LIB})
endif()

# ── Optimisation flags ────────────────────────
if (MSVC)
    target_compile_options(AeroMash PRIVATE /W3 /O2)
else()
    target_compile_options(AeroMash PRIVATE -Wall -O2)
endif()

message(STATUS "AeroMash build configured. Run:  cmake --build . --config Release")
