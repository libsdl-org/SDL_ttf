LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_ttf

FREETYPE := $(LOCAL_PATH)/freetype

LOCAL_CFLAGS := -I$(LOCAL_PATH)/../SDL/include -I$(FREETYPE)/include

LOCAL_SRC_FILES := SDL_ttf.c \
		freetype/src/autofit/autofit.c   \
		freetype/src/base/ftbase.c       \
		freetype/src/base/ftbitmap.c     \
		freetype/src/base/ftglyph.c      \
		freetype/src/base/ftinit.c       \
		freetype/src/base/ftstroke.c     \
		freetype/src/base/ftsystem.c	 \
		freetype/src/bdf/bdf.c           \
		freetype/src/cff/cff.c           \
		freetype/src/cid/type1cid.c      \
		freetype/src/gzip/ftgzip.c       \
		freetype/src/lzw/ftlzw.c         \
		freetype/src/pcf/pcf.c           \
		freetype/src/pfr/pfr.c           \
		freetype/src/psaux/psaux.c       \
		freetype/src/pshinter/pshinter.c \
		freetype/src/psnames/psmodule.c  \
		freetype/src/raster/raster.c     \
		freetype/src/sfnt/sfnt.c         \
		freetype/src/smooth/smooth.c     \
		freetype/src/truetype/truetype.c \
		freetype/src/type1/type1.c       \
		freetype/src/type42/type42.c     \
		freetype/src/winfonts/winfnt.c   \

LOCAL_CFLAGS += -DFT2_BUILD_LIBRARY

LOCAL_SHARED_LIBRARIES := SDL2

include $(BUILD_SHARED_LIBRARY)
