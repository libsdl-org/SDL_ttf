# SDL3_ttf CMake configuration file:
# This file is meant to be placed in a cmake subfolder of SDL3_ttf-devel-3.x.y-mingw

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(sdl3_ttf_config_path "${CMAKE_CURRENT_LIST_DIR}/../i686-w64-mingw32/lib/cmake/SDL3_ttf/SDL3_ttfConfig.cmake")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(sdl3_ttf_config_path "${CMAKE_CURRENT_LIST_DIR}/../x86_64-w64-mingw32/lib/cmake/SDL3_ttf/SDL3_ttfConfig.cmake")
else("${CMAKE_SIZEOF_VOID_P}" STREQUAL "")
    set(SDL3_ttf_FOUND FALSE)
    return()
endif()

if(NOT EXISTS "${sdl3_ttf_config_path}")
    message(WARNING "${sdl3_ttf_config_path} does not exist: MinGW development package is corrupted")
    set(SDL3_ttf_FOUND FALSE)
    return()
endif()

include("${sdl3_ttf_config_path}")
