cmake_minimum_required(VERSION 3.22)
project(Mescraft)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/components
    ${PROJECT_SOURCE_DIR}/src/controller
    ${PROJECT_SOURCE_DIR}/src/shaders
    ${PROJECT_SOURCE_DIR}/src/systems
    ${PROJECT_SOURCE_DIR}/src/world
    ${PROJECT_SOURCE_DIR}/src/entities
    ${PROJECT_SOURCE_DIR}/src/utilities
)

# Recursively gather all .cpp files in src and include folders
file(GLOB_RECURSE SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/include/*.cpp
)

# Add executable
add_executable(Mescraft ${SOURCES})

# Find OpenGL
find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
    target_link_libraries(Mescraft PRIVATE ${OPENGL_gl_LIBRARY})
endif()

# Link GLFW (assuming you have prebuilt dll)
find_package(glfw3 3.3 REQUIRED)
if(glfw3_FOUND)
    target_link_libraries(Mescraft PRIVATE glfw)
endif()

# Link other system libs
target_link_libraries(Mescraft PRIVATE gdi32)

# Optional: library search path
link_directories(${PROJECT_SOURCE_DIR}/include/lib)

# Static linking for MinGW to make portable .exe
if(MINGW)
    set_target_properties(Mescraft PROPERTIES
        LINK_FLAGS "-static -static-libgcc -static-libstdc++"
    )
endif()

# Optional compiler warnings
if(MSVC)
    target_compile_options(Mescraft PRIVATE /W4 /permissive-)
else()
    target_compile_options(Mescraft PRIVATE -Wall -Wextra -Wpedantic)
endif()
