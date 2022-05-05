# FIXME: CMAKE SUPPORT IN SDL2_ttf IS VERY INCOMPLETE YET !!!
#
# FIXME: make it able build against system libs or the included ones.
# FIXME: handle library versioning.
# FIXME: test accross different target platforms.

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the SDL_ttf source code and call cmake from there")
endif()

##### general #####
cmake_minimum_required(VERSION 3.0)
project(SDL_ttf C)

# See docs/release_checklist.md
set(MAJOR_VERSION 2)
set(MINOR_VERSION 19)
set(MICRO_VERSION 0)
set(SDL_TTF_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}")

# Calculate a libtool-like version number
math(EXPR BINARY_AGE "${MINOR_VERSION} * 100 + ${MICRO_VERSION}")
if(MINOR_VERSION MATCHES "[02468]$")
    # Stable branch, 2.6.1 -> libSDL2_ttf-2.0.so.0.600.1
    set(INTERFACE_AGE ${MICRO_VERSION})
else()
    # Development branch, 2.5.1 -> libSDL2_ttf-2.0.so.0.501.0
    set(INTERFACE_AGE 0)
endif()

# Increment this if there is an incompatible change - but if that happens,
# we should rename the library from SDL2 to SDL3, at which point this would
# reset to 0 anyway.
set(LT_MAJOR "0")

math(EXPR LT_AGE "${BINARY_AGE} - ${INTERFACE_AGE}")
math(EXPR LT_CURRENT "${LT_MAJOR} + ${LT_AGE}")
set(LT_REVISION "${INTERFACE_AGE}")
# For historical reasons, the library name redundantly includes the major
# version twice: libSDL2_ttf-2.0.so.0.
# TODO: in SDL 3, set the OUTPUT_NAME to plain SDL3_ttf, which will simplify
# it to libSDL3_ttf.so.0
set(LT_RELEASE "2.0")
set(LT_VERSION "${LT_MAJOR}.${LT_AGE}.${LT_REVISION}")

# The following should match the versions in the Xcode project file.
# Each version is 1 higher than you might expect, for compatibility
# with libtool: macOS ABI versioning is 1-based, unlike other platforms
# which are normally 0-based.
math(EXPR DYLIB_CURRENT_VERSION_MAJOR "${LT_MAJOR} + ${LT_AGE} + 1")
math(EXPR DYLIB_CURRENT_VERSION_MINOR "${LT_REVISION}")
math(EXPR DYLIB_COMPAT_VERSION_MAJOR "${LT_MAJOR} + 1")
set(DYLIB_CURRENT_VERSION "${DYLIB_CURRENT_VERSION_MAJOR}.${DYLIB_CURRENT_VERSION_MINOR}.0")
# For historical reasons this is 15.0.0 rather than the expected 1.0.0
set(DYLIB_COMPATIBILITY_VERSION "15.0.0")

# For the static assertions in SDL_ttf.c
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSDL_BUILD_MAJOR_VERSION=${MAJOR_VERSION}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSDL_BUILD_MINOR_VERSION=${MINOR_VERSION}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSDL_BUILD_MICRO_VERSION=${MICRO_VERSION}")

if (ANDROID)
    option(TTF_WITH_HARFBUZZ "use harfbuzz to improve text shaping" OFF)

    add_library(SDL2_ttf SHARED)
    target_sources(SDL2_ttf PRIVATE SDL_ttf.c)

    if (TTF_WITH_HARFBUZZ)
        set(HARFBUZZ_INCLUDE_DIRS ../../external/harfbuzz-2.8.0/src)
        set(HARFBUZZ_LIBRARIES harfbuzz)
        set(FREETYPE_INCLUDE_DIRS ../../external/freetype-2.11.1/include)
        set(FREETYPE_LIBRARY freetype)

        add_definitions("-DTTF_USE_HARFBUZZ=1")
        set(HB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
        set(FT_WITH_HARFBUZZ ON CACHE BOOL "" FORCE)

        add_subdirectory(external/harfbuzz-2.8.0)
        target_include_directories(SDL2_ttf PUBLIC external/harfbuzz-2.8.0/src)
        target_link_libraries(SDL2_ttf PRIVATE harfbuzz)

    else()
        set(FT_WITH_HARFBUZZ OFF CACHE BOOL "" FORCE)
    endif()

    add_subdirectory(external/freetype-2.11.1)
    include_directories(external/freetype-2.11.1/include)

    target_include_directories(SDL2_ttf PUBLIC .)
    target_link_libraries(SDL2_ttf PRIVATE freetype SDL2)

else()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(PkgConfig QUIET)
if (NOT (TARGET SDL2 OR TARGET SDL2-static))
  find_package(SDL2 REQUIRED)
endif()
if (NOT (TARGET Freetype::Freetype))
  pkg_check_modules(freetype2 IMPORTED_TARGET freetype2)
  if (freetype2_FOUND)
    set(PC_REQUIRES "freetype2 ${PC_REQUIRES}")
  else()
    find_package(Freetype REQUIRED)
  endif()
endif()

##### library generation #####
add_library(SDL2_ttf SDL_ttf.c SDL_ttf.h)

# Workaround for Ubuntu 20.04's SDL being older than
# https://github.com/libsdl-org/SDL/issues/3531
if (NOT TARGET SDL2::SDL2)
  add_library(SDL2::SDL2 SHARED IMPORTED)
  find_library(SDL2_LIBRARY
    NAMES SDL2
    HINTS "${SDL2_EXEC_PREFIX}"
    REQUIRED)
  set_target_properties(SDL2::SDL2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${SDL2_LIBRARY}")
endif()

if (APPLE)
  # TODO: Use DYLIB_COMPATIBILITY_VERSION, DYLIB_CURRENT_VERSION here
elseif (UNIX AND NOT ANDROID)
  # This is compatible with the libtool build
  set_target_properties(SDL2_ttf PROPERTIES
    VERSION ${LT_VERSION}
    SOVERSION ${LT_MAJOR}
    OUTPUT_NAME "SDL2_ttf-${LT_RELEASE}"
  )
endif()

if (BUILD_SHARED_LIBS)
  target_link_libraries(SDL2_ttf SDL2::SDL2)
  if (WIN32 OR CYGWIN)
    set_target_properties(SDL2_ttf PROPERTIES DEFINE_SYMBOL DLL_EXPORT)
    target_sources(SDL2_ttf PRIVATE version.rc)
  endif()
else()
  target_link_libraries(SDL2_ttf SDL2::SDL2-static)
  if (NOT PSP)
    set_target_properties(SDL2_ttf PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
endif()
if (TARGET Freetype::Freetype)
  target_link_libraries(SDL2_ttf Freetype::Freetype)
else()
  target_link_libraries(SDL2_ttf PkgConfig::freetype2)
endif()

target_include_directories(SDL2_ttf
  PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL2>
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

option(SDL2_TTF_DISABLE_INSTALL "Disable installing SDL2_ttf" OFF)
if (NOT SDL2_TTF_DISABLE_INSTALL)
  install(
    TARGETS SDL2_ttf
    EXPORT SDL2_ttfTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
  install(
    FILES SDL_ttf.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/SDL2
  )

  ##### export files #####
  if (APPLE)
    set(PKG_PREFIX "SDL2_ttf.framework/Resources")
  elseif (WIN32)
    set(PKG_PREFIX "cmake")
  else ()
    set(PKG_PREFIX "lib/cmake/SDL2")
  endif ()

  write_basic_package_version_file("${CMAKE_BINARY_DIR}/SDL2_ttfConfigVersion.cmake"
    VERSION ${SDL_TTF_VERSION}
    COMPATIBILITY AnyNewerVersion
  )

  install(
    EXPORT SDL2_ttfTargets
    FILE SDL2_ttfTargets.cmake
    NAMESPACE SDL2_ttf::
    DESTINATION ${PKG_PREFIX}
  )
  install(
    FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttfConfig.cmake
      ${CMAKE_BINARY_DIR}/SDL2_ttfConfigVersion.cmake
    DESTINATION ${PKG_PREFIX}
    COMPONENT Devel
  )

  ##### pkg-config #####
  if (PKG_CONFIG_FOUND)
    set(SDL_VERSION 2.0.0)
    set(prefix ${CMAKE_INSTALL_PREFIX})
    set(exec_prefix "\${prefix}")
    set(libdir "\${exec_prefix}/lib${LIB_SUFFIX}")
    set(bindir "\${exec_prefix}/bin")
    set(includedir "\${prefix}/include")

    configure_file("${SDL_ttf_SOURCE_DIR}/SDL2_ttf.pc.in"
      "${SDL_ttf_BINARY_DIR}/SDL2_ttf.pc" @ONLY)

    if (CMAKE_SYSTEM_NAME MATCHES FreeBSD)
      # FreeBSD uses ${PREFIX}/libdata/pkgconfig
      install(FILES ${SDL_ttf_BINARY_DIR}/SDL2_ttf.pc DESTINATION "libdata/pkgconfig")
    else ()
      install(FILES ${SDL_ttf_BINARY_DIR}/SDL2_ttf.pc
        DESTINATION "lib${LIB_SUFFIX}/pkgconfig")
    endif ()
  endif ()
endif ()

endif()
