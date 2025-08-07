cmake_minimum_required(VERSION 3.16...4.0)

if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(cmake_build_type_undefined 1)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# See docs/release_checklist.md
set(MAJOR_VERSION 3)
set(MINOR_VERSION 3)
set(MICRO_VERSION 0)
set(SDL_REQUIRED_VERSION 3.2.6)

project(SDL3_ttf
    LANGUAGES C
    VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}"
)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/GetGitRevisionDescription.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/PkgConfigHelper.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/PrivateSdlFunctions.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sdlcpu.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sdlplatform.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/sdlmanpages.cmake")
sdl_calculate_derived_version_variables(${MAJOR_VERSION} ${MINOR_VERSION} ${MICRO_VERSION})

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(SDLTTF_ROOTPROJECT ON)
else()
    set(SDLTTF_ROOTPROJECT OFF)
endif()

# By default, configure in RelWithDebInfo configuration
if(SDLTTF_ROOTPROJECT)
  get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(is_multi_config)
    # The first item in CMAKE_CONFIGURATION_TYPES is the default configuration
    if(DEFINED CMAKE_CONFIGURATION_TYPES AND "RelWithDebInfo" IN_LIST CMAKE_CONFIGURATION_TYPES)
      list(REMOVE_ITEM CMAKE_CONFIGURATION_TYPES "RelWithDebInfo")
      list(INSERT CMAKE_CONFIGURATION_TYPES 0 "RelWithDebInfo")
      set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "CMake configuration types" FORCE)
    endif()
  else()
    if(cmake_build_type_undefined)
      set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "CMake build type" FORCE)
    endif()
  endif()
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
if(EMSCRIPTEN OR VITA OR PSP OR PS2 OR N3DS OR RISCOS)
  set(PLATFORM_SUPPORTS_SHARED OFF)
endif()

option(CMAKE_POSITION_INDEPENDENT_CODE "Build static libraries with -fPIC" ${PLATFORM_SUPPORTS_SHARED})
cmake_dependent_option(BUILD_SHARED_LIBS "Build the library as a shared library" ON PLATFORM_SUPPORTS_SHARED OFF)

cmake_dependent_option(SDLTTF_INSTALL "Enable SDL3_ttf install target" ${SDLTTF_ROOTPROJECT} "${sdl3ttf_install_enableable}" OFF)
cmake_dependent_option(SDLTTF_INSTALL_CPACK "Create binary SDL3_ttf archive using CPack" ${SDLTTF_ROOTPROJECT} "SDLTTF_INSTALL" OFF)
cmake_dependent_option(SDLTTF_INSTALL_MAN "Install man pages for SDL3_ttf" OFF "SDLTTF_INSTALL" OFF)
cmake_dependent_option(SDLTTF_RELOCATABLE "Create relocatable SDL_ttf package" "${MSVC}" SDLTTF_INSTALL OFF)
option(SDLTTF_VENDORED "Use vendored third-party libraries" ${vendored_default})
option(SDLTTF_WERROR "Treat warnings as errors" OFF)

option(SDLTTF_STRICT "Fail when a dependency could not be found" OFF)
set(find_package_strict_arg )
set(fatal_error "STATUS")
if(SDLTTF_STRICT)
    set(find_package_strict_arg "REQUIRED")
    set(fatal_error "FATAL_ERROR")
endif()

option(SDLTTF_SAMPLES "Build the SDL3_ttf sample program(s)" ${SDLTTF_ROOTPROJECT})
cmake_dependent_option(SDLTTF_SAMPLES_INSTALL "Install the SDL3_ttf sample program(s)" OFF "SDLTTF_SAMPLES;SDLTTF_INSTALL" OFF)

# For style consistency, create a SDLTTF_FREETYPE CMake variable. This variable is NOT configurable.
set(SDLTTF_FREETYPE ON)
set(SDLTTF_FREETYPE_VENDORED "${SDLTTF_VENDORED}")

set(HARFBUZZ_REQUIRED_VERSION "2.3.1")
option(SDLTTF_HARFBUZZ "Use harfbuzz to improve text shaping" ON)
set(SDLTTF_HARFBUZZ_VENDORED "${SDLTTF_VENDORED}")

option(SDLTTF_PLUTOSVG "Use plutosvg for color emoji support" ON)
set(SDLTTF_PLUTOSVG_VENDORED "${SDLTTF_VENDORED}")

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

SDL_DetectTargetCPUArchitectures(SDL_CPU_NAMES)
SDL_DetectCMakePlatform()

# Enable large file support on 32-bit glibc, so that the vendored libraries
# can access files with large inode numbers
check_symbol_exists("__GLIBC__" "stdlib.h" LIBC_IS_GLIBC)
if (LIBC_IS_GLIBC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(_FILE_OFFSET_BITS=64)
endif()

add_library(${sdl3_ttf_target_name}
    src/SDL_hashtable.c
    src/SDL_hashtable_ttf.c
    src/SDL_gpu_textengine.c
    src/SDL_renderer_textengine.c
    src/SDL_surface_textengine.c
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
if ("c_std_99" IN_LIST CMAKE_C_COMPILE_FEATURES)
    target_compile_features(${sdl3_ttf_target_name} PRIVATE c_std_99)
else()
    message(WARNING "target_compile_features does not know c_std_99 for C compiler")
endif()
if(WIN32 AND SDLTTF_BUILD_SHARED_LIBS)
    target_sources(${sdl3_ttf_target_name} PRIVATE
        src/version.rc
    )
    if(MINGW)
        target_link_options(${sdl3_ttf_target_name} PRIVATE -static-libgcc)
    endif()
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
        cmake_minimum_required(VERSION 3.17...3.28)
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

list(APPEND SDLTTF_BACKENDS HARFBUZZ)
set(SDLTTF_HARFBUZZ_ENABLED FALSE)
if(SDLTTF_HARFBUZZ)
    set(harfbuzz_sources)
    set(harfbuzz_link_libraries)
    set(harfbuzz_include_directories)
    if(SDLTTF_HARFBUZZ_VENDORED)
        message(STATUS "${PROJECT_NAME}: Using vendored harfbuzz library")
        # HB_BUILD_SUBSET variable is used by harfbuzz
        set(HB_BUILD_SUBSET OFF CACHE BOOL "build harfbuzz-subset" FORCE)
        # HB_BUILD_UTILS variable is used by harfbuzz
        set(HB_BUILD_UTILS OFF CACHE BOOL "harfbuzz build utils" FORCE)
        # SKIP_INSTALL_LIBRARIES variable is used by harfbuzz
        set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "harfbuzz install option" FORCE)
        # HB_HAVE_FREETYPE variable is used by harfbuzz
        set(HB_HAVE_FREETYPE ${SDLTTF_FREETYPE} CACHE BOOL "harfbuzz freetype helpers" FORCE)
        # target-specific harfbuzz backends :
        if(APPLE)
            # Minimum version for $<LINK_LIBRARY:feature,library-list>
            cmake_minimum_required(VERSION 3.24)
            # HB_HAVE_CORETEXT variable is used by harfbuzz
            set(HB_HAVE_CORETEXT ON CACHE BOOL "harfbuzz coretext backend" FORCE)
        endif()
        if(WIN32)
            # HB_HAVE_UNISCRIBE variable is used by harfbuzz
            set(HB_HAVE_UNISCRIBE ON CACHE BOOL "harfbuzz uniscribe backend" FORCE)
            # HB_HAVE_GDI variable is used by harfbuzz
            set(HB_HAVE_GDI ON CACHE BOOL "harfbuzz windows backend" FORCE)
        endif()
        ## HACK: HACK: These fail detection, we rely on our own vendored FreeType having them ##
        set(HAVE_FT_GET_VAR_BLEND_COORDINATES 1 CACHE BOOL "FT_Get_Var_Blend_Coordinates" FORCE)
        set(HAVE_FT_SET_VAR_BLEND_COORDINATES 1 CACHE BOOL "FT_Set_Var_Blend_Coordinates" FORCE)
        set(HAVE_FT_DONE_MM_VAR 1 CACHE BOOL "FT_Done_MM_Var" FORCE)
        set(HAVE_FT_GET_TRANSFORM 1 CACHE BOOL "FT_Get_Transform" FORCE)
        sdl_check_project_in_subfolder(external/harfbuzz harfbuzz SDLTTF_VENDORED)
        add_subdirectory(external/harfbuzz external/harfbuzz-build EXCLUDE_FROM_ALL)
        # harfbuzz is a c++ project, enable c++ here to ensure linking to the c++ standard library.
        enable_language(CXX)
        if(NOT TARGET harfbuzz::harfbuzz)
            add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
        endif()
        set(SDLTTF_HARFBUZZ_ENABLED TRUE)
        if(SDLTTF_BUILD_SHARED_LIBS)
            set(harfbuzz_link_libraries harfbuzz::harfbuzz)
        else()
            set(harfbuzz_sources $<TARGET_OBJECTS:harfbuzz::harfbuzz>)
            set(harfbuzz_include_directories $<TARGET_PROPERTY:harfbuzz::harfbuzz,INTERFACE_INCLUDE_DIRECTORIES>)
            if(WIN32)
                # for uniscribe and gdi backends :
                list(APPEND harfbuzz_link_libraries usp10 gdi32 rpcrt4)
                list(APPEND PC_LIBS -lusp10 -lgdi32 -lrpcrt4)
            elseif(APPLE)
                # for coretext backend :
                list(APPEND harfbuzz_link_libraries "$<LINK_LIBRARY:FRAMEWORK,CoreText>")
                list(APPEND harfbuzz_link_libraries "$<LINK_LIBRARY:FRAMEWORK,CoreGraphics>")
                list(APPEND harfbuzz_link_libraries "$<LINK_LIBRARY:FRAMEWORK,CoreFoundation>")
                list(APPEND PC_LIBS "-Wl,-framework,CoreText")
                list(APPEND PC_LIBS "-Wl,-framework,CoreGraphics")
                list(APPEND PC_LIBS "-Wl,-framework,CoreFoundation")
            else()
                find_package(Threads)
                set(harfbuzz_link_libraries Threads::Threads)
            endif()
        endif()
    else()
        find_package(harfbuzz "${HARFBUZZ_REQUIRED_VERSION}" ${find_package_strict_arg})
        if(harfbuzz_FOUND)
            message(STATUS "${PROJECT_NAME}: Using system harfbuzz library")
            list(APPEND PC_REQUIRES harfbuzz)
            set(SDLTTF_HARFBUZZ_ENABLED TRUE)
            set(harfbuzz_link_libraries harfbuzz::harfbuzz)
        else()
            message(${fatal_error} "harfbuzz NOT found")
        endif()
    endif()
    if(SDLTTF_HARFBUZZ_ENABLED)
        target_compile_definitions(${sdl3_ttf_target_name} PRIVATE TTF_USE_HARFBUZZ=1)
        target_sources(${sdl3_ttf_target_name} PRIVATE ${harfbuzz_sources})
        target_include_directories(${sdl3_ttf_target_name} PRIVATE ${harfbuzz_include_directories})
        target_link_libraries(${sdl3_ttf_target_name} PRIVATE ${harfbuzz_link_libraries})
    endif()
endif()

list(APPEND SDLTTF_BACKENDS FREETYPE)
set(SDLTTF_FREETYPE_ENABLED FALSE)
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
        add_subdirectory(external/freetype external/freetype-build EXCLUDE_FROM_ALL)
        if(NOT TARGET Freetype::Freetype)
            add_library(Freetype::Freetype ALIAS freetype)
        endif()
        if(SDLTTF_BUILD_SHARED_LIBS)
            target_link_libraries(${sdl3_ttf_target_name} PRIVATE Freetype::Freetype)
        else()
            target_sources(${sdl3_ttf_target_name} PRIVATE $<TARGET_OBJECTS:Freetype::Freetype>)
            target_include_directories(${sdl3_ttf_target_name} PRIVATE $<TARGET_PROPERTY:Freetype::Freetype,INTERFACE_INCLUDE_DIRECTORIES>)
        endif()
    else()
        message(STATUS "${PROJECT_NAME}: Using system freetype library")
        find_package(Freetype REQUIRED)
        list(APPEND PC_REQUIRES freetype2)
        target_link_libraries(${sdl3_ttf_target_name} PRIVATE Freetype::Freetype)
    endif()
    set(SDLTTF_FREETYPE_ENABLED TRUE)
endif()

list(APPEND SDLTTF_BACKENDS PLUTOSVG)
set(SDLTTF_PLUTOSVG_ENABLED FALSE)
if(SDLTTF_PLUTOSVG)
    set(plutosvg_compile_definitions)
    set(plutosvg_include_directories)
    set(plutosvg_link_libraries)
    set(plutosvg_sources)
    if(SDLTTF_PLUTOSVG_VENDORED)
        message(STATUS "${PROJECT_NAME}: Using vendored plutosvg library")
        set(BUILD_SHARED_LIBS OFF)
        set(PLUTOSVG_ENABLE_FREETYPE ON CACHE BOOL "plutosvg enable freetype" FORCE)
        set(PLUTOSVG_BUILD_EXAMPLES OFF CACHE BOOL "plutosvg build examples" FORCE)
        set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "plutovg build examples" FORCE)
        sdl_check_project_in_subfolder(external/plutovg plutovg SDLTTF_VENDORED)
        add_subdirectory(external/plutovg external/plutovg-build EXCLUDE_FROM_ALL)
        sdl_check_project_in_subfolder(external/plutosvg plutosvg SDLTTF_VENDORED)
        add_subdirectory(external/plutosvg external/plutosvg-guild EXCLUDE_FROM_ALL)
        set(SDLTTF_PLUTOSVG_ENABLED TRUE)
        if(SDLTTF_BUILD_SHARED_LIBS)
            set(plutosvg_link_libraries plutosvg::plutosvg)
        else()
            set(plutosvg_compile_definitions
                $<TARGET_PROPERTY:plutovg::plutovg,INTERFACE_COMPILE_DEFINITIONS>
                $<TARGET_PROPERTY:plutosvg::plutosvg,INTERFACE_COMPILE_DEFINITIONS>)
            set(plutosvg_include_directories
                $<TARGET_PROPERTY:plutovg::plutovg,INTERFACE_INCLUDE_DIRECTORIES>
                $<TARGET_PROPERTY:plutosvg::plutosvg,INTERFACE_INCLUDE_DIRECTORIES>)
            set(plutosvg_sources $<TARGET_OBJECTS:plutosvg> $<TARGET_OBJECTS:plutovg>)
        endif()
    else()
        find_package(plutosvg ${find_package_strict_arg})
        if(plutosvg_FOUND)
            message(STATUS "${PROJECT_NAME}: Using system plutosvg library")
            set(plutosvg_link_libraries plutosvg::plutosvg)
            list(APPEND PC_REQUIRES plutosvg)
            set(SDLTTF_PLUTOSVG_ENABLED TRUE)
        else()
            message(${fatal_error} "plutosvg NOT found")
        endif()
    endif()
    if(SDLTTF_PLUTOSVG_ENABLED)
        target_compile_definitions(${sdl3_ttf_target_name} PRIVATE TTF_USE_PLUTOSVG=1)
        target_sources(${sdl3_ttf_target_name} PRIVATE ${plutosvg_sources})
        target_compile_definitions(${sdl3_ttf_target_name} PRIVATE ${plutosvg_compile_definitions})
        target_include_directories(${sdl3_ttf_target_name} PRIVATE ${plutosvg_include_directories})
        target_link_libraries(${sdl3_ttf_target_name} PRIVATE ${plutosvg_link_libraries})
    endif()
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
        "${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_ttf/SDL_textengine.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_ttf/SDL_ttf.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL3_ttf" COMPONENT devel
    )
    if(BUILD_SHARED_LIBS)
        set(pdbdir "${CMAKE_INSTALL_BINDIR}")
    else()
        set(pdbdir "${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(MSVC)
        SDL_install_pdb("${sdl3_ttf_target_name}" "${pdbdir}")
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
        NO_SET_AND_CHECK_MACRO
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
    if(NOT SDLTTF_VENDORED)
      install(
          FILES
              cmake/PkgConfigHelper.cmake
              cmake/Findharfbuzz.cmake
              cmake/Findplutosvg.cmake
              cmake/Findplutovg.cmake
          DESTINATION "${SDLTTF_INSTALL_CMAKEDIR}"
          COMPONENT devel
      )
    endif()
    install(EXPORT SDL3_ttfTargets
        FILE ${sdl3_ttf_target_name}-targets.cmake
        NAMESPACE SDL3_ttf::
        DESTINATION "${SDLTTF_INSTALL_CMAKEDIR}"
        COMPONENT devel
    )

    export(TARGETS ${sdl3_ttf_target_name} NAMESPACE "SDL3_ttf::" FILE "${sdl3_ttf_target_name}-targets.cmake")

    if(SDLTTF_RELOCATABLE)
      file(RELATIVE_PATH SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${CMAKE_INSTALL_PREFIX}/${SDLTTF_PKGCONFIG_INSTALLDIR}" "${CMAKE_INSTALL_PREFIX}")
      string(REGEX REPLACE "[/]+$" "" SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")
      set(SDL_PKGCONFIG_PREFIX "\${pcfiledir}/${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")
    else()
      set(SDL_PKGCONFIG_PREFIX "${CMAKE_INSTALL_PREFIX}")
    endif()

    if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
      set(INCLUDEDIR_FOR_PKG_CONFIG "${CMAKE_INSTALL_INCLUDEDIR}")
    else()
      set(INCLUDEDIR_FOR_PKG_CONFIG "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()
    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
      set(LIBDIR_FOR_PKG_CONFIG "${CMAKE_INSTALL_LIBDIR}")
    else()
      set(LIBDIR_FOR_PKG_CONFIG "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
    endif()

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

    if(SDLTTF_INSTALL_CPACK)
        if(MSVC)
            set(CPACK_GENERATOR "ZIP")
        else()
            set(CPACK_GENERATOR "TGZ")
        endif()
        configure_file(cmake/CPackProjectConfig.cmake.in CPackProjectConfig.cmake @ONLY)
        set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CPackProjectConfig.cmake")
        # CPACK_SOURCE_PACKAGE_FILE_NAME must end with "-src" (so we can block creating a source archive)
        set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-src")
        set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
        include(CPack)
    endif()

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
    function(add_sdl_ttf_example_executable TARGET)
        if(ANDROID)
            add_library(${TARGET} SHARED ${ARGN})
        else()
            add_executable(${TARGET} ${ARGN})
        endif()
        sdl_add_warning_options(${TARGET} WARNING_AS_ERROR ${SDLTTF_WERROR})
        sdl_target_link_options_no_undefined(${TARGET})
        target_link_libraries(${TARGET} PRIVATE SDL3_ttf::${sdl3_ttf_target_name})
        target_link_libraries(${TARGET} PRIVATE ${sdl3_target_name})
        if("c_std_99" IN_LIST CMAKE_C_COMPILE_FEATURES)
            target_compile_features(${TARGET} PRIVATE c_std_99)
        endif()

        if(SDLTTF_SAMPLES_INSTALL)
            install(TARGETS ${TARGET}
                RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL3_ttf"
            )
        endif()
    endfunction()
    add_sdl_ttf_example_executable(glfont examples/glfont.c)
    add_sdl_ttf_example_executable(showfont examples/showfont.c examples/editbox.c)
    add_sdl_ttf_example_executable(testapp examples/testapp.c)
    add_sdl_ttf_example_executable(testgputext examples/testgputext.c)

    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
    if(TARGET OpenGL::OpenGL)
        target_compile_definitions(glfont PRIVATE HAVE_OPENGL)
        target_link_libraries(glfont PRIVATE OpenGL::OpenGL)
    elseif(TARGET OpenGL::GL)
        target_compile_definitions(glfont PRIVATE HAVE_OPENGL)
        target_link_libraries(glfont PRIVATE OpenGL::GL)
    endif()
endif()

set(available_deps)
set(unavailable_deps)
foreach(dep IN LISTS SDLTTF_BACKENDS)
  set(var SDLTTF_${dep}_ENABLED)
  if(NOT DEFINED ${var})
    message(AUTHOR_WARNING "${var} not defined")
  endif()
  if(${var})
    list(APPEND available_deps ${dep})
  else()
    list(APPEND unavailable_deps ${dep})
  endif()
endforeach()
string(JOIN " " avail_str ${available_deps})
string(TOLOWER "${avail_str}" avail_str)
string(JOIN " " unavail_str ${unavailable_deps})
string(TOLOWER "${unavail_str}" unavail_str)
message(STATUS "SDL3_ttf backends:")
message(STATUS "- enabled:  ${avail_str}")
message(STATUS "- disabled: ${unavail_str}")
