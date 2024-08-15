cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# See docs/release_checklist.md
set(MAJOR_VERSION 3)
set(MINOR_VERSION 0)
set(MICRO_VERSION 0)
set(SDL_REQUIRED_VERSION 3.0.0)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the SDL_ttf source code and call cmake from there")
endif()

project(SDL3_ttf
    LANGUAGES C
    VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}"
)

include("${SDL3_ttf_SOURCE_DIR}/cmake/GetGitRevisionDescription.cmake")
include("${SDL3_ttf_SOURCE_DIR}/cmake/PrivateSdlFunctions.cmake")
include("${SDL3_ttf_SOURCE_DIR}/cmake/sdlmanpages.cmake")
sdl_calculate_derived_version_variables(${MAJOR_VERSION} ${MINOR_VERSION} ${MICRO_VERSION})

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(SDLTTF_ROOTPROJECT ON)
else()
    set(SDLTTF_ROOTPROJECT OFF)
endif()

# Assume MSVC projects don't have a package manager and need vendored dependencies (by default).
# Most other platforms have some kind of package manager.
# FIXME: consider a package manager such as conan/vcpkg instead of vendoring
if(ANDROID OR MSVC)
    set(vendored_default TRUE)
else()
    set(vendored_default FALSE)
endif()

set(sdl3ttf_install_enableable ON)
if ((TARGET SDL3-shared OR TARGET SDL3-static) AND SDL_DISABLE_INSTALL)
    # Cannot install SDL3_ttf when SDL3 is built in same built, and is not installed.
    set(sdl3ttf_install_enableable OFF)
endif()

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(CheckSymbolExists)

set(PLATFORM_SUPPORTS_SHARED ON)
if(VITA OR PSP OR PS2 OR N3DS OR RISCOS)
  set(PLATFORM_SUPPORTS_SHARED OFF)
endif()

option(CMAKE_POSITION_INDEPENDENT_CODE "Build static libraries with -fPIC" ${PLATFORM_SUPPORTS_SHARED})
cmake_dependent_option(BUILD_SHARED_LIBS "Build the library as a shared library" ON PLATFORM_SUPPORTS_SHARED OFF)

cmake_dependent_option(SDLTTF_INSTALL "Enable SDL3_ttf install target" ${SDLTTF_ROOTPROJECT} "${sdl3ttf_install_enableable}" OFF)
cmake_dependent_option(SDLTTF_INSTALL_MAN "Install man pages for SDL3_ttf" OFF "SDLTTF_INSTALL" OFF)
option(SDLTTF_VENDORED "Use vendored third-party libraries" ${vendored_default})
option(SDLTTF_WERROR "Treat warnings as errors" OFF)

option(SDLTTF_SAMPLES "Build the SDL3_ttf sample program(s)" ${SDLTTF_ROOTPROJECT})
cmake_dependent_option(SDLTTF_SAMPLES_INSTALL "Install the SDL3_ttf sample program(s)" OFF "SDLTTF_SAMPLES;SDLTTF_INSTALL" OFF)

# For style consistency, create a SDLTTF_FREETYPE CMake variable. This variable is NOT configurable.
set(SDLTTF_FREETYPE ON)
set(SDLTTF_FREETYPE_VENDORED "${SDLTTF_VENDORED}")

option(SDLTTF_HARFBUZZ "Use harfbuzz to improve text shaping" OFF)
set(SDLTTF_HARFBUZZ_VENDORED "${SDLTTF_VENDORED}")

# Save BUILD_SHARED_LIBS variable
set(SDLTTF_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")

set(sdl_required_components Headers)

if(SDLTTF_BUILD_SHARED_LIBS)
    set(sdl3_ttf_target_name SDL3_ttf-shared)
    set(sdl3_target_name SDL3::SDL3-shared)

    list(APPEND sdl_required_components SDL3-shared)
else()
    set(sdl3_ttf_target_name SDL3_ttf-static)
    set(sdl3_target_name SDL3::SDL3)
endif()

if(NOT TARGET SDL3::Headers OR NOT TARGET ${sdl3_target_name})
    find_package(SDL3 ${SDL_REQUIRED_VERSION} REQUIRED COMPONENTS ${sdl_required_components})
endif()

# Enable large file support on 32-bit glibc, so that the vendored libraries
# can access files with large inode numbers
check_symbol_exists("__GLIBC__" "stdlib.h" LIBC_IS_GLIBC)
if (LIBC_IS_GLIBC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(_FILE_OFFSET_BITS=64)
endif()

add_library(${sdl3_ttf_target_name}
    src/SDL_ttf.c
)
add_library(SDL3_ttf::${sdl3_ttf_target_name} ALIAS ${sdl3_ttf_target_name})
if(NOT TARGET SDL3_ttf::SDL3_ttf)
    add_library(SDL3_ttf::SDL3_ttf ALIAS ${sdl3_ttf_target_name})
endif()
target_include_directories(${sdl3_ttf_target_name}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_definitions(${sdl3_ttf_target_name} PRIVATE
    BUILD_SDL
    SDL_BUILD_MAJOR_VERSION=${MAJOR_VERSION}
    SDL_BUILD_MINOR_VERSION=${MINOR_VERSION}
    SDL_BUILD_MICRO_VERSION=${MICRO_VERSION}
)
target_link_libraries(${sdl3_ttf_target_name} PUBLIC SDL3::Headers)
if(SDLTTF_BUILD_SHARED_LIBS)
    target_link_libraries(${sdl3_ttf_target_name} PRIVATE SDL3::SDL3-shared)
endif()
sdl_add_warning_options(${sdl3_ttf_target_name} WARNING_AS_ERROR ${SDLTTF_WERROR})
if(WIN32 AND SDLTTF_BUILD_SHARED_LIBS)
    target_sources(${sdl3_ttf_target_name} PRIVATE
        src/version.rc
    )
endif()
set_target_properties(${sdl3_ttf_target_name} PROPERTIES
    OUTPUT_NAME SDL3_ttf
    DEFINE_SYMBOL DLL_EXPORT
    EXPORT_NAME ${sdl3_ttf_target_name}
    C_VISIBILITY_PRESET "hidden"
)
if(NOT ANDROID)
    set_target_properties(${sdl3_ttf_target_name} PROPERTIES
        SOVERSION "${SO_VERSION_MAJOR}"
        VERSION "${SO_VERSION}"
    )
    if(APPLE)
        cmake_minimum_required(VERSION 3.17)
        set_target_properties(${sdl3_ttf_target_name} PROPERTIES
            MACHO_COMPATIBILITY_VERSION "${DYLIB_COMPAT_VERSION}"
            MACHO_CURRENT_VERSION "${DYLIB_CURRENT_VERSION}"
        )
    endif()
endif()
if(SDLTTF_BUILD_SHARED_LIBS)
    if(WIN32)
        set_target_properties(${sdl3_ttf_target_name} PROPERTIES
            PREFIX ""
        )
    endif()
else()
    if(MSVC)
        set_target_properties(${sdl3_ttf_target_name} PROPERTIES
            OUTPUT_NAME "SDL3_ttf-static"
        )
    endif()
endif()

if(SDLTTF_BUILD_SHARED_LIBS)
    # Use `Compatible Interface Properties` to ensure a shared SDL3_ttf is linked to a shared SDL3 library
    set_property(TARGET ${sdl3_ttf_target_name} PROPERTY INTERFACE_SDL3_SHARED ${SDLTTF_BUILD_SHARED_LIBS})
    set_property(TARGET ${sdl3_ttf_target_name} APPEND PROPERTY COMPATIBLE_INTERFACE_BOOL SDL3_SHARED)
endif()

if(SDLTTF_BUILD_SHARED_LIBS)
    sdl_target_link_options_no_undefined(${sdl3_ttf_target_name})
endif()

sdl_target_link_option_version_file(${sdl3_ttf_target_name} "${CMAKE_CURRENT_SOURCE_DIR}/src/SDL_ttf.sym")

if(SDLTTF_BUILD_SHARED_LIBS)
    # Make sure static library dependencies are built with -fPIC when building a shared SDL3_ttf
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(INSTALL_EXTRA_TARGETS)
set(PC_LIBS)
set(PC_REQUIRES)

# Build freetype and harfbuzz as a static library
set(BUILD_SHARED_LIBS OFF)

if(SDLTTF_HARFBUZZ)
    if(SDLTTF_HARFBUZZ_VENDORED)
        message(STATUS "${PROJECT_NAME}: Using vendored harfbuzz library")
        # HB_BUILD_UTILS variable is used by harfbuzz
        set(HB_BUILD_UTILS OFF CACHE BOOL "harfbuzz build utils" FORCE)
        # SKIP_INSTALL_LIBRARIES variable is used by harfbuzz
        set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "harfbuzz install option" FORCE)
        # HB_HAVE_FREETYPE variable is used by harfbuzz
        set(HB_HAVE_FREETYPE ${SDLTTF_FREETYPE} CACHE BOOL "harfbuzz freetype helpers" FORCE)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/harfbuzz/CMakeLists.txt")
            message(FATAL_ERROR "No harfbuzz sources found. Install a harfbuzz development package or run the download script in the external folder.")
        endif()
        add_subdirectory(external/harfbuzz EXCLUDE_FROM_ALL)
        # harfbuzz is a c++ project, enable c++ here to ensure linking to the c++ standard library
        enable_language(CXX)
        if(NOT SDLTTF_BUILD_SHARED_LIBS)
            list(APPEND INSTALL_EXTRA_TARGETS harfbuzz)
            list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:harfbuzz>)
        endif()
        if(NOT TARGET harfbuzz::harfbuzz)
            add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
        endif()
    else()
        message(STATUS "${PROJECT_NAME}: Using system harfbuzz library")
        find_package(harfbuzz REQUIRED)
        list(APPEND PC_REQUIRES harfbuzz)
    endif()
    target_compile_definitions(${sdl3_ttf_target_name} PRIVATE TTF_USE_HARFBUZZ=1)
    target_link_libraries(${sdl3_ttf_target_name} PRIVATE harfbuzz::harfbuzz)
endif()

if(SDLTTF_FREETYPE)
    if(SDLTTF_FREETYPE_VENDORED)
        message(STATUS "${PROJECT_NAME}: Using vendored freetype library")
        # FT_DISABLE_ZLIB variable is used by freetype
        set(FT_DISABLE_ZLIB ON CACHE BOOL "freetype zlib option")
        # FT_DISABLE_BZIP2 variable is used by freetype
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "freetype bzip2 option")
        # FT_DISABLE_PNG variable is used by freetype
        set(FT_DISABLE_PNG ON CACHE BOOL "freetype png option")
        # FT_DISABLE_BROTLI variable is used by freetype
        set(FT_DISABLE_BROTLI ON CACHE BOOL "freetype option")
        if(SDLTTF_HARFBUZZ)
            # FT_DISABLE_HARFBUZZ variable is used by freetype
            set(FT_DISABLE_HARFBUZZ OFF CACHE BOOL "freetype harfbuzz option" FORCE)
            # FT_REQUIRE_HARFBUZZ variable is used by freetype
            set(FT_REQUIRE_HARFBUZZ ON CACHE BOOL "freetype harfbuzz option" FORCE)
        else()
            # FT_DISABLE_HARFBUZZ variable is used by freetype
            set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "freetype harfbuzz option" FORCE)
            # FT_REQUIRE_HARFBUZZ variable is used by freetype
            set(FT_REQUIRE_HARFBUZZ OFF CACHE BOOL "freetype harfbuzz option" FORCE)
        endif()
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/freetype/CMakeLists.txt")
            message(FATAL_ERROR "No freetype sources found. Install a freetype development package or run the download script in the external folder.")
        endif()
        add_subdirectory(external/freetype EXCLUDE_FROM_ALL)
        if(NOT TARGET Freetype::Freetype)
            add_library(Freetype::Freetype ALIAS freetype)
        endif()
        if(NOT SDLTTF_BUILD_SHARED_LIBS)
            list(APPEND INSTALL_EXTRA_TARGETS freetype)
            list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:freetype>)
        endif()
    else()
        message(STATUS "${PROJECT_NAME}: Using system freetype library")
        find_package(Freetype REQUIRED)
        list(APPEND PC_REQUIRES freetype2)
    endif()
    target_link_libraries(${sdl3_ttf_target_name} PRIVATE Freetype::Freetype)
endif()

# Restore BUILD_SHARED_LIBS variable
set(BUILD_SHARED_LIBS ${SDLTTF_BUILD_SHARED_LIBS})

if(SDLTTF_INSTALL)
    install(
        TARGETS ${sdl3_ttf_target_name}
        EXPORT SDL3_ttfTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT devel
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT library
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT library
    )
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_ttf/SDL_ttf.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL3_ttf" COMPONENT devel
    )

    if(INSTALL_EXTRA_TARGETS)
        install(TARGETS ${INSTALL_EXTRA_TARGETS}
            EXPORT SDL3_ttfTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT devel
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT library
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT library
            PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT devel
        )
    endif()

    if(WIN32 AND NOT MINGW)
        set(SDLTTF_INSTALL_CMAKEDIR_DEFAULT "cmake")
    else()
        set(SDLTTF_INSTALL_CMAKEDIR_DEFAULT "${CMAKE_INSTALL_LIBDIR}/cmake")
    endif()
    set(SDLTTF_INSTALL_CMAKEDIR_ROOT "${SDLTTF_INSTALL_CMAKEDIR_DEFAULT}" CACHE STRING "Location where to install SDL3_ttfConfig.cmake")
    set(SDLTTF_PKGCONFIG_INSTALLDIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    if(WIN32 AND NOT MINGW)
        set(SDLTTF_INSTALL_CMAKEDIR "${SDLTTF_INSTALL_CMAKEDIR_ROOT}")
        set(LICENSES_PREFIX "licenses/SDL3_ttf")
    else()
        set(SDLTTF_INSTALL_CMAKEDIR "${SDLTTF_INSTALL_CMAKEDIR_ROOT}/SDL3_ttf")
        set(LICENSES_PREFIX "${CMAKE_INSTALL_DATAROOTDIR}/licenses/SDL3_ttf")
    endif()

    configure_package_config_file(cmake/SDL3_ttfConfig.cmake.in SDL3_ttfConfig.cmake
        INSTALL_DESTINATION "${SDLTTF_INSTALL_CMAKEDIR}"
    )
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/SDL3_ttfConfigVersion.cmake"
        COMPATIBILITY AnyNewerVersion
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/SDL3_ttfConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/SDL3_ttfConfigVersion.cmake"
        DESTINATION ${SDLTTF_INSTALL_CMAKEDIR}
        COMPONENT devel
    )
    install(EXPORT SDL3_ttfTargets
        FILE ${sdl3_ttf_target_name}-targets.cmake
        NAMESPACE SDL3_ttf::
        DESTINATION "${SDLTTF_INSTALL_CMAKEDIR}"
        COMPONENT devel
    )

    file(RELATIVE_PATH SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${CMAKE_INSTALL_PREFIX}/${SDLTTF_PKGCONFIG_INSTALLDIR}" "${CMAKE_INSTALL_PREFIX}")
    string(REGEX REPLACE "[/]+$" "" SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")
    set(SDL_PKGCONFIG_PREFIX "\${pcfiledir}/${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")

    string(JOIN " " PC_REQUIRES ${PC_REQUIRES})
    string(JOIN " " PC_LIBS ${PC_LIBS})
    configure_file(cmake/sdl3-ttf.pc.in sdl3-ttf.pc @ONLY)

    # Always install sdl3-ttf.pc file: libraries might be different between config modes
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sdl3-ttf.pc"
        DESTINATION "${SDLTTF_PKGCONFIG_INSTALLDIR}" COMPONENT devel)

    install(FILES "LICENSE.txt"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME}"
        COMPONENT library
    )

    if(SDLTTF_INSTALL_MAN)
        sdl_get_git_revision_hash(SDLTTF_REVISION)
        SDL_generate_manpages(
            HEADERS_DIR "${PROJECT_SOURCE_DIR}/include/SDL3_ttf"
            SYMBOL "TTF_Init"
            WIKIHEADERS_PL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build-scripts/wikiheaders.pl"
            REVISION "${SDLTTF_REVISION}"
        )
    endif()
endif()

if(SDLTTF_SAMPLES)
    add_executable(glfont examples/glfont.c)
    add_executable(showfont examples/showfont.c)
    add_executable(testapp examples/testapp.c)

    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
    if(TARGET OpenGL::OpenGL)
        target_compile_definitions(glfont PRIVATE HAVE_OPENGL)
        target_link_libraries(glfont PRIVATE OpenGL::OpenGL)
    elseif(TARGET OpenGL::GL)
        target_compile_definitions(glfont PRIVATE HAVE_OPENGL)
        target_link_libraries(glfont PRIVATE OpenGL::GL)
    endif()

    foreach(prog glfont showfont testapp)
        sdl_add_warning_options(${prog} WARNING_AS_ERROR ${SDLTTF_WERROR})
        target_link_libraries(${prog} PRIVATE SDL3_ttf::${sdl3_ttf_target_name})
        target_link_libraries(${prog} PRIVATE ${sdl3_target_name})

        if(SDLTTF_SAMPLES_INSTALL)
            install(TARGETS ${prog}
                RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            )
        endif()
    endforeach()
endif()
