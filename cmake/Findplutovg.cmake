
include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_PLUTOSG QUIET plutovg)

find_library(plutovg_LIBRARY
    NAMES plutovg
    HINTS ${PC_PLUTOVG_LIBDIR}
)

find_path(plutovg_INCLUDE_PATH
    NAMES plutovg.h
    PATH_SUFFIXES plutovg
    HINTS ${PC_PLUTOVG_INCLUDEDIR}
)

find_library(PLUTOVG_MATH_LIBRARY NAMES m)

if(PC_PLUTOVG_FOUND)
    get_flags_from_pkg_config("${plutovg_LIBRARY}" "PC_PLUTOVG" "_plutovg")
endif()

set(plutovg_COMPILE_OPTIONS "${_plutovg_compile_options}" CACHE STRING "Extra compile flags of plutovg")

set(plutovg_LINK_LIBRARIES "${_plutovg_link_libraries}" CACHE STRING "Extra link libraries of plutovg")

set(plutovg_LINK_OPTIONS "${_plutovg_link_options}" CACHE STRING "Extra link flags of plutovg")

set(plutovg_LINK_DIRECTORIES "${_plutovg_link_directories}" CACHE STRING "Extra link flags of plutovg")

find_package_handle_standard_args(plutovg
    REQUIRED_VARS plutovg_LIBRARY plutovg_INCLUDE_PATH
)

if(plutovg_FOUND)
  if(NOT TARGET plutovg::plutovg)
    add_library(plutovg::plutovg UNKNOWN IMPORTED)
    set_target_properties(plutovg::plutovg PROPERTIES
        IMPORTED_LOCATION "${plutovg_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${plutovg_INCLUDE_PATH}"
        INTERFACE_COMPILE_OPTIONS "${plutovg_COMPILE_OPTIONS}"
        INTERFACE_LINK_LIBRARIES "${plutovg_LINK_LIBRARIES}"
        INTERFACE_LINK_OPTIONS "${plutovg_LINK_OPTIONS}"
        INTERFACE_LINK_DIRECTORIES "${plutovg_LINK_DIRECTORIES}"
    )
    if(PLUTOVG_MATH_LIBRARY)
      set_property(TARGET plutovg::plutovg APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${PLUTOVG_MATH_LIBRARY}")
    endif()
  endif()
endif()
