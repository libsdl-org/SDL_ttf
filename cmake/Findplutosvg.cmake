
include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_PLUTOSVG QUIET plutosvg)

find_library(plutosvg_LIBRARY
    NAMES plutosvg
    HINTS ${PC_PLUTOSVG_LIBDIR}
)

find_path(plutosvg_INCLUDE_PATH
    NAMES plutosvg.h
    PATH_SUFFIXES plutosvg
    HINTS ${PC_PLUTOSVG_INCLUDEDIR}
)

if(PC_PLUTOSVG_FOUND)
    get_flags_from_pkg_config("${plutosvg_LIBRARY}" "PC_PLUTOSVG" "_plutosvg")
endif()

set(plutosvg_COMPILE_OPTIONS "${_plutosvg_compile_options}" CACHE STRING "Extra compile flags of plutosvg")

set(plutosvg_LINK_LIBRARIES "${_plutosvg_link_libraries}" CACHE STRING "Extra link libraries of plutosvg")

set(plutosvg_LINK_OPTIONS "${_plutosvg_link_options}" CACHE STRING "Extra link flags of plutosvg")

set(plutosvg_LINK_DIRECTORIES "${_plutosvg_link_directories}" CACHE STRING "Extra link flags of plutosvg")

find_package(plutovg)

find_package_handle_standard_args(plutosvg
    REQUIRED_VARS plutosvg_LIBRARY plutosvg_INCLUDE_PATH plutovg_FOUND
)

if(plutosvg_FOUND)
  if(NOT TARGET plutosvg::plutosvg)
    add_library(plutosvg::plutosvg UNKNOWN IMPORTED)
    set_target_properties(plutosvg::plutosvg PROPERTIES
        IMPORTED_LOCATION "${plutosvg_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${plutosvg_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${plutosvg_COMPILE_OPTIONS}"
        INTERFACE_LINK_LIBRARIES "${plutosvg_LINK_LIBRARIES};plutovg::plutovg"
        INTERFACE_LINK_OPTIONS "${plutosvg_LINK_OPTIONS}"
        INTERFACE_LINK_DIRECTORIES "${plutosvg_LINK_DIRECTORIES}"
    )
  endif()
endif()
