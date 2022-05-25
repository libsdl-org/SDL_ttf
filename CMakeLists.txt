cmake_minimum_required(VERSION 3.14)

# See docs/release_checklist.md
set(MAJOR_VERSION 2)
set(MINOR_VERSION 19)
set(MICRO_VERSION 1)
set(SDL_TTF_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}")
set(SDL_REQUIRED_VERSION 2.0.10)

# Set defaults preventing destination file conflicts
set(SDL_CMAKE_DEBUG_POSTFIX "d"
    CACHE STRING "Name suffix for debug builds")

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

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the SDL_ttf source code and call cmake from there")
endif()

project(SDL2_ttf LANGUAGES C
    VERSION "${SDL_TTF_VERSION}")

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

if (NOT (TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static))
    find_package(SDL2 REQUIRED)
endif()

# Workaround for Ubuntu 20.04's SDL being older than
# https://github.com/libsdl-org/SDL/issues/3531
if (NOT (TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static))
    find_library(SDL2_LIBRARY
        NAMES SDL2
        HINTS "${SDL2_EXEC_PREFIX}"
        REQUIRED
    )
    if (NOT SDL2_LIBRARY)
        message(FATAL_ERROR "Could not find SDL2 library. Please define SDL2_EXEC_PREFIX and/or SLD2_LIBRARY")
    endif()
    add_library(SDL2::SDL2 SHARED IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
    )
endif()

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(VENDORED_DEFAULT "Default value for *_VENDORED options. Can be overridden for each library. Is only used in the first configure run." ON)

# For style consistency, create a TTF_WITH_FREETYPE CMake variable. This variable is NOT configurable.
set(TTF_WITH_FREETYPE ON)
cmake_dependent_option(TTF_WITH_FREETYPE_VENDORED "Use vendored freetype library" ${VENDORED_DEFAULT} TTF_WITH_FREETYPE OFF)

option(TTF_WITH_HARFBUZZ "Use harfbuzz to improve text shaping" OFF)
cmake_dependent_option(TTF_WITH_HARFBUZZ_VENDORED "Use vendored harfbuzz library" ${VENDORED_DEFAULT} TTF_WITH_HARFBUZZ OFF)

option(BUILD_SAMPLES "Build the SDL2_ttf sample program(s)" ON)
option(BUILD_SHARED_LIBS "Build the library as a shared library" ON)
option(TTF_DISABLE_INSTALL "Disable installing SDL2_ttf" OFF)

if(BUILD_SHARED_LIBS)
    set(sdl2_ttf_export_name SDL2_ttf)
    set(sdl2_ttf_install_name_infix shared)
else()
    set(sdl2_ttf_export_name SDL2_ttf-static)
    set(sdl2_ttf_install_name_infix static)
endif()

add_library(SDL2_ttf
    SDL_ttf.c
    SDL_ttf.h
)
add_library(SDL2_ttf::${sdl2_ttf_export_name} ALIAS SDL2_ttf)

target_compile_definitions(SDL2_ttf PRIVATE
    SDL_BUILD_MAJOR_VERSION=${MAJOR_VERSION}
    SDL_BUILD_MINOR_VERSION=${MINOR_VERSION}
    SDL_BUILD_MICRO_VERSION=${MICRO_VERSION}
)

if (BUILD_SHARED_LIBS)
    target_link_libraries(SDL2_ttf PRIVATE SDL2::SDL2)
else()
    target_link_libraries(SDL2_ttf PRIVATE SDL2::SDL2-static)
endif()

set(INSTALL_EXTRA_TARGETS)
set(PC_LIBS)
set(PC_REQUIRES)

# Save BUILD_SHARED_LIBS variable
set(SDL2TTF_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Build freetype and harfbuzz as a static library (avoid cyclic dependency between shared libraries)
set(BUILD_SHARED_LIBS OFF)

if (TTF_WITH_HARFBUZZ)
    target_compile_definitions(SDL2_ttf PRIVATE TTF_USE_HARFBUZZ=1)
    if (TTF_WITH_HARFBUZZ_VENDORED)
        set(HB_BUILD_UTILS OFF CACHE BOOL "harfbuzz build utils" FORCE)
        set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "harfbuzz install option" FORCE)
        set(HB_HAVE_FREETYPE ${TTF_WITH_FREETYPE} CACHE BOOL "harfbuzz freetype helpers" FORCE)
        add_subdirectory(external/harfbuzz EXCLUDE_FROM_ALL)
        list(APPEND INSTALL_EXTRA_TARGETS harfbuzz)
        if (NOT TARGET harfbuzz::harfbuzz)
            add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
        endif()
        list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:harfbuzz>)
    else()
        find_package(harfbuzz REQUIRED)
        list(APPEND PC_REQUIRES harfbuzz)
    endif()
    target_link_libraries(SDL2_ttf PRIVATE harfbuzz::harfbuzz)
endif()

if (TTF_WITH_FREETYPE)
    if (TTF_WITH_FREETYPE_VENDORED)
        set(FT_DISABLE_ZLIB ON CACHE BOOL "freetype zlib option")
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "freetype bzip2 option")
        set(FT_DISABLE_PNG ON CACHE BOOL "freetype png option")
        set(FT_DISABLE_BROTLI ON CACHE BOOL "freetype option")
        if (TTF_WITH_HARFBUZZ)
            set(FT_DISABLE_HARFBUZZ OFF CACHE BOOL "freetype harfbuzz option" FORCE)
            set(FT_REQUIRE_HARFBUZZ ON CACHE BOOL "freetype harfbuzz option" FORCE)
            #set(HarfBuzz_INCLUDE_DIR "$<TARGET_PROPERTY:harfbuzz::harfbuzz,INCLUDE_DIRECTORIES>" CACHE STRING "freetype harfbuzz include path" FORCE)
            #set(HarfBuzz_LIBRARY harfbuzz::harfbuzz CACHE STRING "freetype harfbuzz library path" FORCE)
        else()
            set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "freetype harfbuzz option" FORCE)
            set(FT_REQUIRE_HARFBUZZ OFF CACHE BOOL "freetype harfbuzz option" FORCE)
        endif()
        add_subdirectory(external/freetype EXCLUDE_FROM_ALL)
        list(APPEND INSTALL_EXTRA_TARGETS freetype)
        if (NOT TARGET Freetype::Freetype)
            add_library(Freetype::Freetype ALIAS freetype)
        endif()
        list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:freetype>)
    else()
        find_package(Freetype REQUIRED)
        list(APPEND PC_REQUIRES freetype2)
    endif()
    target_link_libraries(SDL2_ttf PRIVATE Freetype::Freetype)
endif()

# Restore BUILD_SHARED_LIBS variable
set(BUILD_SHARED_LIBS ${SDL2TTF_BUILD_SHARED_LIBS})

set_target_properties(SDL2_ttf PROPERTIES
    DEFINE_SYMBOL DLL_EXPORT
    PUBLIC_HEADER SDL_ttf.h
    EXPORT_NAME ${sdl2_ttf_export_name}
    C_VISIBILITY_PRESET "hidden"
)
if (NOT ANDROID)
    set_target_properties(SDL2_ttf PROPERTIES
        DEBUG_POSTFIX "${SDL_CMAKE_DEBUG_POSTFIX}")
endif()

if (APPLE)
    # TODO: Use DYLIB_COMPATIBILITY_VERSION, DYLIB_CURRENT_VERSION here
elseif (UNIX AND NOT APPLE AND NOT ANDROID)
    set_target_properties(SDL2_ttf PROPERTIES
        SOVERSION "${LT_MAJOR}"
        VERSION "${LT_VERSION}"
    )
endif()
if (BUILD_SHARED_LIBS)
    if (WIN32 OR OS2)
        set_target_properties(SDL2_ttf PROPERTIES
            PREFIX ""
        )
    endif()
    if (OS2)
        # OS/2 doesn't support a DLL name longer than 8 characters.
        set_target_properties(SDL2_ttf PROPERTIES
           OUTPUT_NAME "SDL2ttf"
        )
    elseif (UNIX AND NOT APPLE AND NOT ANDROID)
        set_target_properties(SDL2_ttf PROPERTIES
            OUTPUT_NAME "SDL2_ttf-${LT_RELEASE}"
        )
    endif()
endif()

if (WIN32 AND BUILD_SHARED_LIBS)
    target_sources(SDL2_ttf PRIVATE version.rc)
endif()

target_include_directories(SDL2_ttf
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL2>
    )

if (NOT TTF_DISABLE_INSTALL)
    install(
        TARGETS SDL2_ttf
        EXPORT SDL2_ttfTargets
        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL2"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    if (INSTALL_EXTRA_TARGETS)
        install(TARGETS ${INSTALL_EXTRA_TARGETS} EXPORT SDL2_ttfTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            PUBLIC_HEADER DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/external_include"
        )
    endif()

    ##### export files #####
    if (APPLE)
        set(PKG_PREFIX "SDL2_ttf.framework/Resources")
    elseif (WIN32)
        set(PKG_PREFIX "cmake")
    else ()
        set(PKG_PREFIX "${CMAKE_INSTALL_LIBDIR}/cmake/SDL2_ttf")
    endif ()

    configure_package_config_file(SDL2_ttfConfig.cmake.in SDL2_ttfConfig.cmake
        INSTALL_DESTINATION "${PKG_PREFIX}"
    )
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/SDL2_ttfConfigVersion.cmake"
        VERSION ${SDL_TTF_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    install(EXPORT SDL2_ttfTargets
        FILE SDL2_ttf-${sdl2_ttf_install_name_infix}-targets.cmake
        NAMESPACE SDL2_ttf::
        DESTINATION "${PKG_PREFIX}"
    )

    set(VERSION ${SDL_TTF_VERSION})
    set(SDL_VERSION ${SDL_REQUIRED_VERSION})
    set(prefix "${CMAKE_INSTALL_PREFIX}")
    set(exec_prefix "\${prefix}")
    set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
    set(bindir "\${exec_prefix}/bin")
    set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    string(JOIN " " PC_REQUIRES ${PC_REQUIRES})
    string(JOIN " " PC_LIBS ${PC_LIBS})
    configure_file(SDL2_ttf.pc.in ${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttf.pc.intermediate @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttf.pc" INPUT "${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttf.pc.intermediate")

    if (CMAKE_SYSTEM_NAME MATCHES FreeBSD)
        # FreeBSD uses ${PREFIX}/libdata/pkgconfig
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttf.pc DESTINATION "libdata/pkgconfig")
    else ()
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttf.pc
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    endif ()
    install(FILES "LICENSE.txt" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME}")
    if (NOT (WIN32 OR CYGWIN OR MINGW))
        if(BUILD_SHARED_LIBS)
            if(NOT ANDROID)
                install(CODE "
                    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
                        \"$<TARGET_SONAME_FILE_NAME:SDL2_ttf>\" \"libSDL2_ttf$<$<CONFIG:Debug>:${SDL_CMAKE_DEBUG_POSTFIX}>$<TARGET_FILE_SUFFIX:SDL2_ttf>\"
                        WORKING_DIRECTORY \"${PROJECT_BINARY_DIR}\")")
                install(FILES "${PROJECT_BINARY_DIR}/libSDL2_ttf$<$<CONFIG:Debug>:${SDL_CMAKE_DEBUG_POSTFIX}>$<TARGET_FILE_SUFFIX:SDL2_ttf>" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
            endif()
        endif()
    endif()
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttfConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttfConfigVersion.cmake"
        DESTINATION ${PKG_PREFIX}
        COMPONENT devel
    )

    if (BUILD_SAMPLES)
        add_executable(glfont glfont.c)
        add_executable(showfont showfont.c)

        set(OpenGL_GL_PREFERENCE GLVND)
        find_package(OpenGL)
        if (TARGET OpenGL::OpenGL)
            target_compile_definitions(glfont PRIVATE HAVE_OPENGL)
            target_link_libraries(glfont PRIVATE OpenGL::OpenGL)
        elseif (TARGET OpenGL::GL)
            target_compile_definitions(glfont PRIVATE HAVE_OPENGL)
            target_link_libraries(glfont PRIVATE OpenGL::GL)
        endif()

        foreach(prog glfont showfont)
            if (MINGW)
                target_link_libraries(${prog} PRIVATE mingw32)
                target_link_options(${prog} PRIVATE -mwindows)
            endif()
            target_link_libraries(${prog} PRIVATE SDL2_ttf::${sdl2_ttf_export_name})
            if (TARGET SDL2::SDL2main)
                target_link_libraries(${prog} PRIVATE SDL2::SDL2main)
            endif()
            if (BUILD_SHARED_LIBS)
                target_link_libraries(${prog} PRIVATE SDL2::SDL2)
            else()
                target_link_libraries(${prog} PRIVATE SDL2::SDL2-static)
            endif()
        endforeach()
    endif()
endif()
