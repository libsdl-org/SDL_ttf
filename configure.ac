dnl Process this file with autoconf to produce a configure script.

dnl Set various version strings - taken gratefully from the GTk sources
# Making releases:
#   MICRO_VERSION_MACRO += 1;
#   INTERFACE_AGE_MACRO += 1;
#   BINARY_AGE_MACRO += 1;
# if any functions have been added, set INTERFACE_AGE_MACRO to 0.
# if backwards compatibility has been broken,
# set BINARY_AGE_MACRO and INTERFACE_AGE_MACRO to 0.
m4_define([MAJOR_VERSION_MACRO], [2])
m4_define([MINOR_VERSION_MACRO], [0])
m4_define([MICRO_VERSION_MACRO], [15])
m4_define([INTERFACE_AGE_MACRO], [1])
m4_define([BINARY_AGE_MACRO], [15])

AC_INIT([SDL2_ttf],
        MAJOR_VERSION_MACRO.MINOR_VERSION_MACRO.MICRO_VERSION_MACRO,
        [https://github.com/libsdl-org/SDL_ttf/issues],
        [SDL2_ttf])
AC_CONFIG_MACRO_DIR([acinclude])
AC_CONFIG_SRCDIR([SDL_ttf.c])

AC_SUBST([MAJOR_VERSION], MAJOR_VERSION_MACRO)
AC_SUBST([MINOR_VERSION], MINOR_VERSION_MACRO)
AC_SUBST([MICRO_VERSION], MICRO_VERSION_MACRO)
AC_SUBST([INTERFACE_AGE], INTERFACE_AGE_MACRO)
AC_SUBST([BINARY_AGE], BINARY_AGE_MACRO)

# libtool versioning
LT_INIT([win32-dll])

LT_RELEASE=$MAJOR_VERSION.$MINOR_VERSION
LT_CURRENT=`expr $MICRO_VERSION - $INTERFACE_AGE`
LT_REVISION=$INTERFACE_AGE
LT_AGE=`expr $BINARY_AGE - $INTERFACE_AGE`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Detect the canonical build and host environments
AC_CANONICAL_HOST

dnl Setup for automake
AM_INIT_AUTOMAKE([1.16 foreign subdir-objects tar-ustar])

dnl Check for tools
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
if test -z "$host_alias"; then
    hostaliaswindres=
else
    hostaliaswindres="$host_alias-windres"
fi
AC_CHECK_PROGS(WINDRES, [windres $hostaliaswindres $host_os-windres])

case "$host" in
    *-*-cygwin* | *-*-mingw*)
        ;;

    *)  AC_FUNC_ALLOCA
        ;;
esac


case "$host" in
    *-*-beos*)
        ac_default_prefix=/boot/develop/tools/gnupro
        ;;
    *-*-cygwin* | *-*-mingw*)
        if test "$build" != "$host"; then # cross-compiling
            # Default cross-compile location
            ac_default_prefix=/usr/local/cross-tools/$host
        else
            # Look for the location of the tools and install there
            if test "$BUILD_PREFIX" != ""; then
                ac_default_prefix=$BUILD_PREFIX
            fi
        fi
        if test x$WINDRES != x; then
            use_version_rc=true
        fi
        ;;
esac
AM_CONDITIONAL(USE_VERSION_RC, test x$use_version_rc = xtrue)

SUMMARY="\nSDL2_ttf Configure Summary:\n"

dnl Check for SDL
SDL_VERSION=2.0.12
AC_SUBST(SDL_VERSION)
AM_PATH_SDL2($SDL_VERSION,
            :,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

dnl Check for the FreeType 2 library
AC_ARG_ENABLE(freetype-builtin, [AS_HELP_STRING([--enable-freetype-builtin], [Use included version of FreeType [default=yes]])], [], enable_freetype_builtin=yes)

if test x$enable_freetype_builtin = xyes; then
	SUMMARY="${SUMMARY}Using included FreeType : YES\n"
else
	PKG_CHECK_MODULES([FT2], [freetype2 >= 7.0.1], [], [dnl
		AC_CHECK_FT2(,,[AC_MSG_ERROR([dnl
*** Unable to find FreeType2 library (http://www.freetype.org/)])]
		)
	])
	CFLAGS="$CFLAGS $FT2_CFLAGS"
	LIBS="$LIBS $FT2_LIBS"

	SUMMARY="${SUMMARY}Using included FreeType : NO\n"
fi
AM_CONDITIONAL(USE_BUILTIN_FREETYPE, test x$enable_freetype_builtin = xyes)

dnl Check for the HarfBuzz library
AC_ARG_ENABLE(harfbuzz, [AS_HELP_STRING([--enable-harfbuzz], [Enable HarfBuzz [default=yes]])], [], enable_harfbuzz=yes)
AC_ARG_ENABLE(harfbuzz-builtin, [AS_HELP_STRING([--enable-harfbuzz-builtin], [Use included version of HarfBuzz [default=yes]])], [], enable_harfbuzz_builtin=yes)

if test x$enable_harfbuzz = xyes; then
	AC_DEFINE(TTF_USE_HARFBUZZ, 1, [])
	SUMMARY="${SUMMARY}Using HarfBuzz          : YES\n"

	if test x$enable_harfbuzz_builtin = xyes; then
		SUMMARY="${SUMMARY}Using included HarfBuzz : YES\n"
		AX_CXX_COMPILE_STDCXX(11)
		AC_CHECK_ALIGNOF([struct{char;}])
		case "$host" in
		  *-*-mingw*) is_win32=yes
			CXXFLAGS="$CXXFLAGS -static-libgcc"
			CFLAGS="$CFLAGS -static-libgcc"
		  ;;
		esac
		case "$host" in
		  arm-*-*)
		    if test "x$ac_cv_alignof_struct_char__" != x1; then
			CXXFLAGS="$CXXFLAGS -mstructure-size-boundary=8"
		    fi
		  ;;
		esac
		AM_CONDITIONAL(OS_WIN32, test "$hb_os_win32" = "yes")
		dnl silence a lot of harfbuzz warnings:
		CXXFLAGS="$CXXFLAGS -Wno-unused-result"
		dnl prevent linking to libstdc++:
		CXXFLAGS="$CXXFLAGS -fno-rtti -fno-exceptions -fno-threadsafe-statics"
	else
		SUMMARY="${SUMMARY}Using included HarfBuzz : NO\n"
		PKG_CHECK_MODULES([HB], [harfbuzz >= 2.3.1], harfbuzz=yes, harfbuzz=no)

		if test x$harfbuzz = xyes; then
			AC_DEFINE(TTF_USE_HARFBUZZ, 1, [])
			CFLAGS="$CFLAGS $HB_CFLAGS"
			LIBS="$LIBS $HB_LIBS"
			SUMMARY="${SUMMARY}Using harfbuzz      : YES\n"
		else
			AC_MSG_ERROR([dnl
*** Unable to find HarfBuzz library (http://www.freetype.org/)])
		fi
	fi
else
	AC_DEFINE(TTF_USE_HARFBUZZ, 0, [])
	SUMMARY="${SUMMARY}Using HarfBuzz          : NO\n"
	enable_harfbuzz_builtin=no
fi
AM_CONDITIONAL(USE_BUILTIN_HARFBUZZ, test x$enable_harfbuzz_builtin = xyes)

dnl See if GCC's -fvisibility=hidden is supported (gcc4 and later, usually).
dnl  Details of this flag are here: http://gcc.gnu.org/wiki/Visibility
CheckVisibilityHidden()
{
    AC_MSG_CHECKING(for GCC -fvisibility=hidden option)
    have_gcc_fvisibility=no

    visibility_CFLAGS="-fvisibility=hidden"
    save_CFLAGS="$CFLAGS"
    CFLAGS="$save_CFLAGS $visibility_CFLAGS -Werror"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #if !defined(__GNUC__) || __GNUC__ < 4
    #error SDL only uses visibility attributes in GCC 4 or newer
    #endif
    ]],[])], [have_gcc_fvisibility=yes],[])
    AC_MSG_RESULT($have_gcc_fvisibility)
    CFLAGS="$save_CFLAGS"

    if test x$have_gcc_fvisibility = xyes; then
        CFLAGS="$CFLAGS $visibility_CFLAGS"
        CXXFLAGS="$CXXFLAGS $visibility_CFLAGS"
    fi
}
CheckVisibilityHidden

dnl Check for OpenGL
case "$host" in
    *-*-cygwin* | *-*-mingw*)
        MATHLIB=""
        SYS_GL_LIBS="-lopengl32"
        ;;
    *-*-beos*)
        MATHLIB=""
        SYS_GL_LIBS="-lGL"
        ;;
    *-*-darwin*)
        MATHLIB=""
        SYS_GL_LIBS="-Wl,-framework,OpenGL"
        ;;
    *-*-aix*)
        if test x$ac_cv_c_compiler_gnu = xyes; then
            CFLAGS="-mthreads"
        fi
        SYS_GL_LIBS=""
        ;;
    *)
        MATHLIB="-lm"
        AC_PATH_X
        AC_PATH_XTRA
        if test x$have_x = xyes; then
            CFLAGS="$CFLAGS $X_CFLAGS"
            SYS_GL_LIBS="$X_LIBS -lGL"
        else
            SYS_GL_LIBS="-lGL"
        fi
        ;;
esac
AC_MSG_CHECKING(for OpenGL support)
have_opengl=no
save_LIBS="$LIBS"
LIBS="$LIBS $SYS_GL_LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
 #include "SDL_opengl.h"
]], [[
 glOrtho( -2.0, 2.0, -2.0, 2.0, -20.0, 20.0 );
]])], [have_opengl=yes],[])
AC_MSG_RESULT($have_opengl)
LIBS="$save_LIBS"
if test x$have_opengl = xyes; then
    CFLAGS="$CFLAGS -DHAVE_OPENGL"
    GL_LIBS="$SYS_GL_LIBS"
else
    GL_LIBS=""
fi
AC_SUBST([GL_LIBS])
AC_SUBST([MATHLIB])
AC_SUBST([WINDRES])

# Finally create all the generated files
AC_CONFIG_FILES([
Makefile
SDL2_ttf.spec
SDL2_ttf.pc
])

AC_CONFIG_COMMANDS([summary], [printf "$SUMMARY"], [SUMMARY="$SUMMARY"])
AC_OUTPUT
