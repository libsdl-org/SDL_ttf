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
m4_define([MICRO_VERSION_MACRO], [11])
dnl 'patch' version added to not bump version to 2.0.12,
dnl  because SDL2_ttf does have a 2.0.12 release.  this
dnl  affects only the package and libtool versions,  but
dnl  not SDL_TTF_VERSION which still reads 2.0.11..
m4_define([PATCH_VERSION_MACRO], [1])
m4_define([INTERFACE_AGE_MACRO], [1])
m4_define([BINARY_AGE_MACRO], [11])

AC_INIT([SDL_ttf],
        MAJOR_VERSION_MACRO.MINOR_VERSION_MACRO.MICRO_VERSION_MACRO.PATCH_VERSION_MACRO,
        [https://github.com/libsdl-org/SDL_ttf/issues],
        [SDL_ttf])
AC_CONFIG_MACRO_DIR([acinclude])
AC_CONFIG_SRCDIR([SDL_ttf.c])

AC_SUBST([MAJOR_VERSION], MAJOR_VERSION_MACRO)
AC_SUBST([MINOR_VERSION], MINOR_VERSION_MACRO)
AC_SUBST([MICRO_VERSION], MICRO_VERSION_MACRO)
AC_SUBST([PATCH_VERSION], PATCH_VERSION_MACRO)
AC_SUBST([INTERFACE_AGE], INTERFACE_AGE_MACRO)
AC_SUBST([BINARY_AGE], BINARY_AGE_MACRO)

dnl libtool versioning
LT_INIT([win32-dll])

LT_RELEASE=$MAJOR_VERSION.$MINOR_VERSION
LT_CURRENT=`expr $MICRO_VERSION - $INTERFACE_AGE`
LT_REVISION=`expr $INTERFACE_AGE + $PATCH_VERSION`
LT_AGE=`expr $BINARY_AGE - $INTERFACE_AGE`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Detect the canonical build and host environments
AC_CANONICAL_HOST

dnl Setup for automake
AM_INIT_AUTOMAKE([foreign tar-ustar])

dnl Check for tools
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
if test -z "$host_alias"; then
    hostaliaswindres=
else
    hostaliaswindres="$host_alias-windres"
fi
AC_CHECK_PROGS(WINDRES, [windres $hostaliaswindres $host_os-windres])

case "$host" in
    *-*-beos*)
        ac_default_prefix=/boot/develop/tools/gnupro
        ;;
    *-*-cygwin* | *-*-mingw*)
        if test "$build" != "$host"; then # cross-compiling
            # Default cross-compile location
            ac_default_prefix=/usr/local/cross-tools/i386-mingw32
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

dnl See if GCC's -Wall is supported.
CheckWarnAll()
{
    AC_MSG_CHECKING(for GCC -Wall option)
    have_gcc_Wall=no

    save_CFLAGS="$CFLAGS"
    CFLAGS="$save_CFLAGS -Wall"
    AC_TRY_COMPILE([int x = 0;],, [have_gcc_Wall=yes])
    AC_MSG_RESULT($have_gcc_Wall)
    CFLAGS="$save_CFLAGS"

    if test x$have_gcc_Wall = xyes; then
        CFLAGS="$CFLAGS -Wall"

dnl Haiku headers use multicharacter constants all over the place. Ignore these warnings when using -Wall.
        AC_MSG_CHECKING(for necessary GCC -Wno-multichar option)
        need_gcc_Wno_multichar=no
        case "$host" in
            *-*-beos* | *-*-haiku*)
                need_gcc_Wno_multichar=yes
                ;;
        esac
        AC_MSG_RESULT($need_gcc_Wno_multichar)
        if test x$need_gcc_Wno_multichar = xyes; then
            CFLAGS="$CFLAGS -Wno-multichar"
        fi
    fi
}

dnl See if GCC's -fvisibility=hidden is supported (gcc4 and later, usually).
CheckVisibilityHidden()
{
    AC_MSG_CHECKING(for GCC -fvisibility=hidden option)
    have_gcc_fvisibility=no
    case "$host" in
    *-*-cygwin* | *-*-mingw* | *-*-os2*)
        AC_MSG_RESULT([ignored for $host_os])
        return
        ;;
    esac

    visibility_CFLAGS="-fvisibility=hidden"
    save_CFLAGS="$CFLAGS"
    CFLAGS="$save_CFLAGS $visibility_CFLAGS -Werror"
    AC_TRY_COMPILE([
    #if !defined(__GNUC__) || __GNUC__ < 4
    #error SDL only uses visibility attributes in GCC 4 or newer
    #endif
    ],, [have_gcc_fvisibility=yes])
    AC_MSG_RESULT($have_gcc_fvisibility)
    CFLAGS="$save_CFLAGS"

    if test x$have_gcc_fvisibility = xyes; then
        CFLAGS="$CFLAGS $visibility_CFLAGS"
    fi
}

dnl check for GCC warning options
CheckWarnAll

dnl check for GCC visibility attributes
CheckVisibilityHidden

dnl check for alloca()
AC_FUNC_ALLOCA

dnl Check for SDL
SDL_VERSION=1.2.4
AC_SUBST(SDL_VERSION)
AM_PATH_SDL($SDL_VERSION,
            :,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

dnl Check for the FreeType 2 library
PKG_CHECK_MODULES([FT2], [freetype2 >= 7.0.1], [
    PC_REQUIRES="freetype2 $PC_REQUIRES"
], [dnl
    AC_CHECK_FT2(,,[AC_MSG_ERROR([dnl
*** Unable to find FreeType2 library (http://www.freetype.org/)])]
    )
    PC_LIBS="$FT2_LIBS $PC_LIBS"
])
CFLAGS="$CFLAGS $FT2_CFLAGS"
LIBS="$LIBS $FT2_LIBS"

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
 glOrtho(-2.0, 2.0, -2.0, 2.0, -20.0, 20.0);
]])], [have_opengl=yes],[])
AC_MSG_RESULT($have_opengl)
LIBS="$save_LIBS"
if test x$have_opengl = xyes; then
    CFLAGS="$CFLAGS -DHAVE_OPENGL"
    GL_LIBS="$SYS_GL_LIBS"
else
    GL_LIBS=""
fi
AC_CHECK_LIB(iconv, libiconv_open, [ICONV_LIBS="-liconv"])
AC_SUBST([GL_LIBS])
AC_SUBST([ICONV_LIBS])
AC_SUBST([MATHLIB])
AC_SUBST([WINDRES])
AC_SUBST([PC_REQUIRES])
AC_SUBST([PC_LIBS])

# Finally create all the generated files
AC_CONFIG_FILES([
Makefile
SDL_ttf.spec
SDL_ttf.pc
])
AC_OUTPUT
