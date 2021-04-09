dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_MACRO_DIR([acinclude])
AC_CONFIG_SRCDIR([SDL_ttf.c])

dnl Set various version strings - taken gratefully from the GTk sources

# Making releases:
#   MICRO_VERSION += 1;
#   INTERFACE_AGE += 1;
#   BINARY_AGE += 1;
# if any functions have been added, set INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set BINARY_AGE and INTERFACE_AGE to 0.

MAJOR_VERSION=2
MINOR_VERSION=0
MICRO_VERSION=15
INTERFACE_AGE=1
BINARY_AGE=15
VERSION=$MAJOR_VERSION.$MINOR_VERSION.$MICRO_VERSION

AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)
AC_SUBST(INTERFACE_AGE)
AC_SUBST(BINARY_AGE)
AC_SUBST(VERSION)

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
AM_INIT_AUTOMAKE(SDL2_ttf, $VERSION)

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
    AC_TRY_COMPILE([
    #if !defined(__GNUC__) || __GNUC__ < 4
    #error SDL only uses visibility attributes in GCC 4 or newer
    #endif
    ],[
    ],[
    have_gcc_fvisibility=yes
    ])
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
AC_TRY_LINK([
 #include "SDL_opengl.h"
],[
 glOrtho( -2.0, 2.0, -2.0, 2.0, -20.0, 20.0 );
],[
have_opengl=yes
])
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
