# SDL3_ttf CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL3_ttf framework

# INTERFACE_LINK_OPTIONS needs CMake 3.12
cmake_minimum_required(VERSION 3.12)

include(FeatureSummary)
set_package_properties(SDL3_ttf PROPERTIES
    URL "https://www.libsdl.org/projects/SDL_ttf/"
    DESCRIPTION "Support for TrueType (.ttf) font files with Simple Directmedia Layer"
)

set(SDL3_ttf_FOUND TRUE)

set(SDLTTF_VENDORED TRUE)

set(SDLTTF_HARFBUZZ TRUE)
set(SDLTTF_FREETYPE TRUE)

string(REGEX REPLACE "SDL3_ttf\\.framework.*" "SDL3_ttf.framework" _sdl3ttf_framework_path "${CMAKE_CURRENT_LIST_DIR}")
string(REGEX REPLACE "SDL3_ttf\\.framework.*" "" _sdl3ttf_framework_parent_path "${CMAKE_CURRENT_LIST_DIR}")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL3_ttf-target.cmake files.

if(NOT TARGET SDL3_ttf::SDL3_ttf)
    add_library(SDL3_ttf::SDL3_ttf INTERFACE IMPORTED)
    set_target_properties(SDL3_ttf::SDL3_ttf
        PROPERTIES
            INTERFACE_COMPILE_OPTIONS "SHELL:-F \"${_sdl3ttf_framework_parent_path}\""
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl3ttf_framework_path}/Headers"
            INTERFACE_LINK_OPTIONS "SHELL:-F \"${_sdl3ttf_framework_parent_path}\";SHELL:-framework SDL3_ttf"
            COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
            INTERFACE_SDL3_SHARED "ON"
    )
endif()

unset(_sdl3ttf_framework_path)
unset(_sdl3ttf_framework_parent_path)
