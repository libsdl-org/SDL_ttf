# Makefile.am for the SDL truetype font loading library and viewer
ACLOCAL_AMFLAGS = -I acinclude

lib_LTLIBRARIES = libSDL_ttf.la

libSDL_ttfincludedir = $(includedir)/SDL
libSDL_ttfinclude_HEADERS =	\
	SDL_ttf.h

libSDL_ttf_la_SOURCES =		\
	SDL_ttf.c

EXTRA_DIST =			\
	Android.mk		\
	external		\
	CHANGES			\
	version.rc		\
	VisualC			\
	Xcode			\
	Xcode-iOS		\
	MPWmake.sea.bin		\
	Watcom-Win32.zip	\
	SDL_ttf.spec		\
	SDL_ttf.spec.in		\
	gcc-fat.sh		\
	autogen.sh

libSDL_ttf_la_LDFLAGS = 	\
	-no-undefined		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) $(LT_EXTRA)
if USE_VERSION_RC
libSDL_ttf_la_DEPENDENCIES = version.o
endif
libSDL_ttf_la_LIBADD = @MATHLIB@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = SDL_ttf.pc

.rc.o:
	$(RC) $< $@

noinst_PROGRAMS = showfont glfont

showfont_LDADD = libSDL_ttf.la @ICONV_LIBS@
glfont_LDADD = libSDL_ttf.la @GL_LIBS@ @MATHLIB@

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: distcheck

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz

