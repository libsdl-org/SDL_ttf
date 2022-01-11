# Makefile.am for the SDL truetype font loading library and viewer
ACLOCAL_AMFLAGS = -I acinclude

lib_LTLIBRARIES = libSDL2_ttf.la

libSDL2_ttfincludedir = $(includedir)/SDL2
libSDL2_ttfinclude_HEADERS =	\
	SDL_ttf.h

libSDL2_ttf_la_SOURCES =		\
	SDL_ttf.c $(LOCAL_FT2_SOURCES) $(LOCAL_HB_SOURCES)

libSDL2_ttf_la_CPPFLAGS = $(TTF_CFLAGS) $(LOCAL_FT2_FLAGS) $(LOCAL_HB_FLAGS)

FREETYPE_PATH = external/freetype-2.10.4
FREETYPE_SOURCES = \
	$(FREETYPE_PATH)/src/autofit/autofit.c \
	$(FREETYPE_PATH)/src/base/ftbase.c \
	$(FREETYPE_PATH)/src/base/ftbbox.c \
	$(FREETYPE_PATH)/src/base/ftbdf.c \
	$(FREETYPE_PATH)/src/base/ftbitmap.c \
	$(FREETYPE_PATH)/src/base/ftcid.c \
	$(FREETYPE_PATH)/src/base/ftdebug.c \
	$(FREETYPE_PATH)/src/base/ftfstype.c \
	$(FREETYPE_PATH)/src/base/ftgasp.c \
	$(FREETYPE_PATH)/src/base/ftglyph.c \
	$(FREETYPE_PATH)/src/base/ftgxval.c \
	$(FREETYPE_PATH)/src/base/ftinit.c \
	$(FREETYPE_PATH)/src/base/ftmm.c \
	$(FREETYPE_PATH)/src/base/ftotval.c \
	$(FREETYPE_PATH)/src/base/ftpatent.c \
	$(FREETYPE_PATH)/src/base/ftpfr.c \
	$(FREETYPE_PATH)/src/base/ftstroke.c \
	$(FREETYPE_PATH)/src/base/ftsynth.c \
	$(FREETYPE_PATH)/src/base/ftsystem.c \
	$(FREETYPE_PATH)/src/base/fttype1.c \
	$(FREETYPE_PATH)/src/base/ftwinfnt.c \
	$(FREETYPE_PATH)/src/bdf/bdf.c \
	$(FREETYPE_PATH)/src/bzip2/ftbzip2.c \
	$(FREETYPE_PATH)/src/cache/ftcache.c \
	$(FREETYPE_PATH)/src/cff/cff.c \
	$(FREETYPE_PATH)/src/cid/type1cid.c \
	$(FREETYPE_PATH)/src/gzip/ftgzip.c \
	$(FREETYPE_PATH)/src/lzw/ftlzw.c \
	$(FREETYPE_PATH)/src/pcf/pcf.c \
	$(FREETYPE_PATH)/src/pfr/pfr.c \
	$(FREETYPE_PATH)/src/psaux/psaux.c \
	$(FREETYPE_PATH)/src/pshinter/pshinter.c \
	$(FREETYPE_PATH)/src/psnames/psmodule.c \
	$(FREETYPE_PATH)/src/raster/raster.c \
	$(FREETYPE_PATH)/src/sfnt/sfnt.c \
	$(FREETYPE_PATH)/src/smooth/smooth.c \
	$(FREETYPE_PATH)/src/truetype/truetype.c \
	$(FREETYPE_PATH)/src/type1/type1.c \
	$(FREETYPE_PATH)/src/type42/type42.c \
	$(FREETYPE_PATH)/src/winfonts/winfnt.c

if USE_BUILTIN_FREETYPE
LOCAL_FT2_FLAGS = -I$(srcdir)/$(FREETYPE_PATH)/include -DFT2_BUILD_LIBRARY -DFT_PUBLIC_FUNCTION_ATTRIBUTE=
LOCAL_FT2_SOURCES = $(FREETYPE_SOURCES)
endif

HARFBUZZ_PATH = external/harfbuzz-2.8.0
HARFBUZZ_SOURCES = \
	$(HARFBUZZ_PATH)/src/hb-aat-layout.cc \
	$(HARFBUZZ_PATH)/src/hb-aat-map.cc \
	$(HARFBUZZ_PATH)/src/hb-blob.cc \
	$(HARFBUZZ_PATH)/src/hb-buffer-serialize.cc \
	$(HARFBUZZ_PATH)/src/hb-buffer.cc \
	$(HARFBUZZ_PATH)/src/hb-common.cc \
	$(HARFBUZZ_PATH)/src/hb-face.cc \
	$(HARFBUZZ_PATH)/src/hb-fallback-shape.cc \
	$(HARFBUZZ_PATH)/src/hb-font.cc \
	$(HARFBUZZ_PATH)/src/hb-ft.cc \
	$(HARFBUZZ_PATH)/src/hb-number.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-cff1-table.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-cff2-table.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-face.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-font.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-layout.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-map.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-math.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-metrics.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-arabic.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-default.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-hangul.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-hebrew.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-indic-table.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-indic.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-khmer.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-myanmar.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-syllabic.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-thai.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-use.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-complex-vowel-constraints.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-fallback.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape-normalize.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-shape.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-tag.cc \
	$(HARFBUZZ_PATH)/src/hb-ot-var.cc \
	$(HARFBUZZ_PATH)/src/hb-set.cc \
	$(HARFBUZZ_PATH)/src/hb-shape-plan.cc \
	$(HARFBUZZ_PATH)/src/hb-shape.cc \
	$(HARFBUZZ_PATH)/src/hb-shaper.cc \
	$(HARFBUZZ_PATH)/src/hb-static.cc \
	$(HARFBUZZ_PATH)/src/hb-ucd.cc \
	$(HARFBUZZ_PATH)/src/hb-coretext.cc \
	$(HARFBUZZ_PATH)/src/hb-gdi.cc \
	$(HARFBUZZ_PATH)/src/hb-uniscribe.cc \
	$(HARFBUZZ_PATH)/src/hb-unicode.cc

if USE_BUILTIN_HARFBUZZ
LOCAL_HB_FLAGS = -I$(srcdir)/$(HARFBUZZ_PATH) -I$(srcdir)/$(HARFBUZZ_PATH)/src -DHAVE_CONFIG_H -DFT_CONFIG_OPTION_USE_HARFBUZZ
LOCAL_HB_SOURCES = $(HARFBUZZ_SOURCES)
if OS_WIN32
LINKER = $(CXXLINK)
else
LINKER = $(LINK)
endif
else
LINKER = $(LINK)
endif

if USE_VERSION_RC
libSDL2_ttf_la_LDFLAGS = 	\
	-no-undefined		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -Wl,version.o
libSDL2_ttf_la_DEPENDENCIES = version.o
else
libSDL2_ttf_la_LDFLAGS = 	\
	-no-undefined		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) $(LT_EXTRA)
endif
libSDL2_ttf_la_LINK = $(LINKER) $(libSDL2_ttf_la_LDFLAGS)
libSDL2_ttf_la_LIBADD = $(TTF_LIBS) @MATHLIB@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = SDL2_ttf.pc

%.o : %.rc
	$(WINDRES) $< $@

EXTRA_DIST = CHANGES.txt COPYING.txt README.txt \
	Android.mk		\
	Makefile.os2		\
	Xcode			\
	VisualC			\
	VisualC-WinRT		\
	version.rc		\
	external		\
	SDL2_ttf.spec		\
	SDL2_ttf.spec.in	\
	SDL2_ttfConfig.cmake	\
	autogen.sh gcc-fat.sh

noinst_PROGRAMS = showfont glfont

showfont_LDADD = libSDL2_ttf.la
glfont_LDADD = libSDL2_ttf.la @GL_LIBS@ @MATHLIB@

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: distcheck

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz

distclean-local:
	-rm -rf `find $(srcdir) -name .deps`
dist-hook:
	-rm -rf `find $(distdir) -name .deps`
