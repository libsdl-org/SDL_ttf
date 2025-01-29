# Create our own Findharfbuzz.cmake module because harfbuzz-config.cmake,
# distributed along e.g. msys2-mingw64 does not support anything other then APPLE and UNIX.

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_HARFBUZZ QUIET harfbuzz)

find_library(harfbuzz_LIBRARY
    NAMES harfbuzz
    HINTS ${PC_HARFBUZZ_LIBDIR}
)

find_path(harfbuzz_INCLUDE_PATH
    NAMES hb.h
    PATH_SUFFIXES harfbuzz
    HINTS ${PC_HARFBUZZ_INCLUDEDIR}
)

if(PC_HARFBUZZ_FOUND)
    get_flags_from_pkg_config("${harfbuzz_LIBRARY}" "PC_HARFBUZZ" "_harfbuzz")
endif()

set(harfbuzz_COMPILE_OPTIONS "${_harfbuzz_compile_options}" CACHE STRING "Extra compile flags of harfbuzz")

set(harfbuzz_LINK_LIBRARIES "${_harfbuzz_link_libraries}" CACHE STRING "Extra link libraries of harfbuzz")

set(harfbuzz_LINK_OPTIONS "${_harfbuzz_link_options}" CACHE STRING "Extra link flags of harfbuzz")

set(harfbuzz_LINK_DIRECTORIES "${_harfbuzz_link_directories}" CACHE PATH "Extra link directories of harfbuzz")

set(harfbuzz_VERSION "harfbuzz_VERSION-NOTFOUND")
foreach(_hb_incpath IN LISTS harfbuzz_INCLUDE_PATH)
    if(IS_DIRECTORY "${_hb_incpath}" AND EXISTS "${_hb_incpath}/hb-version.h")
        file(READ "${_hb_incpath}/hb-version.h" _hb_version_text)
        string(REGEX MATCH "#define[ \t]+HB_VERSION_MAJOR[ \t]+([0-9]+)" _hb_major_re "${_hb_version_text}")
        set(_hb_major "${CMAKE_MATCH_1}")
        string(REGEX MATCH "#define[ \t]+HB_VERSION_MINOR[ \t]+([0-9]+)" _hb_minor_re "${_hb_version_text}")
        set(_hb_minor "${CMAKE_MATCH_1}")
        string(REGEX MATCH "#define[ \t]+HB_VERSION_MICRO[ \t]+([0-9]+)" _hb_micro_re "${_hb_version_text}")
        set(_hb_micro "${CMAKE_MATCH_1}")
        if(_hb_major_re AND _hb_minor_re AND _hb_micro_re)
            set(harfbuzz_VERSION "${_hb_major}.${_hb_minor}.${_hb_micro}")
            break()
        endif()
    endif()
endforeach()

find_package_handle_standard_args(harfbuzz
    REQUIRED_VARS harfbuzz_LIBRARY harfbuzz_INCLUDE_PATH
    VERSION_VAR harfbuzz_VERSION
)

if (harfbuzz_FOUND)
    if (NOT TARGET harfbuzz::harfbuzz)
        add_library(harfbuzz::harfbuzz UNKNOWN IMPORTED)
        set_target_properties(harfbuzz::harfbuzz PROPERTIES
            IMPORTED_LOCATION "${harfbuzz_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${harfbuzz_INCLUDE_PATH}"
            INTERFACE_COMPILE_OPTIONS "${harfbuzz_COMPILE_OPTIONS}"
            INTERFACE_LINK_LIBRARIES "${harfbuzz_LINK_LIBRARIES}"
            INTERFACE_LINK_OPTIONS "${harfbuzz_LINK_OPTIONS}"
            INTERFACE_LINK_DIRECTORIES "${harfbuzz_LINK_DIRECTORIES}"
        )
    endif()
endif()
