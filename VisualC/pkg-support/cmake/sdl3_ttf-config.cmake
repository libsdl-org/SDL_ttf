# SDL3_ttf CMake configuration file:
# This file is meant to be placed in a cmake subfolder of SDL3_ttf-devel-3.x.y-VC

include(FeatureSummary)
set_package_properties(SDL3_ttf PROPERTIES
    URL "https://www.libsdl.org/projects/SDL_ttf/"
    DESCRIPTION "Support for TrueType (.ttf) font files with Simple Directmedia Layer"
)

cmake_minimum_required(VERSION 3.0)

set(SDL3_ttf_FOUND TRUE)

set(SDLTTF_VENDORED TRUE)

set(SDLTTF_HARFBUZZ TRUE)
set(SDLTTF_FREETYPE TRUE)

if(CMAKE_SIZEOF_VOID_P STREQUAL "4")
    set(_sdl_arch_subdir "x86")
elseif(CMAKE_SIZEOF_VOID_P STREQUAL "8")
    set(_sdl_arch_subdir "x64")
else()
    unset(_sdl_arch_subdir)
    set(SDL3_ttf_FOUND FALSE)
    return()
endif()

set(_sdl3ttf_incdir       "${CMAKE_CURRENT_LIST_DIR}/../include")
set(_sdl3ttf_library      "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL3_ttf.lib")
set(_sdl3ttf_dll          "${CMAKE_CURRENT_LIST_DIR}/../lib/${_sdl_arch_subdir}/SDL3_ttf.dll")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL3_ttf-target.cmake files.

if(NOT TARGET SDL3_ttf::SDL3_ttf)
    add_library(SDL3_ttf::SDL3_ttf SHARED IMPORTED)
    set_target_properties(SDL3_ttf::SDL3_ttf
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl3ttf_incdir}"
            IMPORTED_IMPLIB "${_sdl3ttf_library}"
            IMPORTED_LOCATION "${_sdl3ttf_dll}"
            COMPATIBLE_INTERFACE_BOOL "SDL3_SHARED"
            INTERFACE_SDL3_SHARED "ON"
    )
endif()

unset(_sdl_arch_subdir)
unset(_sdl3ttf_incdir)
unset(_sdl3ttf_library)
unset(_sdl3ttf_dll)
