# Save the local path
SDL_TTF_LOCAL_PATH := $(call my-dir)

# Enable this if you want to use PlutoSVG for emoji support
SUPPORT_PLUTOSVG ?= true
PLUTOSVG_LIBRARY_PATH := external/plutosvg
PLUTOVG_LIBRARY_PATH := external/plutovg

# Enable this if you want to use HarfBuzz
SUPPORT_HARFBUZZ ?= true
HARFBUZZ_LIBRARY_PATH := external/harfbuzz

FREETYPE_LIBRARY_PATH := external/freetype

# Build freetype library
ifneq ($(FREETYPE_LIBRARY_PATH),)
    include $(SDL_TTF_LOCAL_PATH)/$(FREETYPE_LIBRARY_PATH)/Android.mk
endif

# Build the library
ifeq ($(SUPPORT_HARFBUZZ),true)
    include $(SDL_TTF_LOCAL_PATH)/$(HARFBUZZ_LIBRARY_PATH)/Android.mk
endif

# Restore local path
LOCAL_PATH := $(SDL_TTF_LOCAL_PATH)
include $(CLEAR_VARS)

LOCAL_MODULE := SDL3_ttf

LOCAL_SRC_FILES := \
	src/SDL_ttf.c.neon \
	src/SDL_hashtable.c \
	src/SDL_hashtable_ttf.c \
	src/SDL_gpu_textengine.c \
	src/SDL_renderer_textengine.c \
	src/SDL_surface_textengine.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_CFLAGS += -O2

LOCAL_LDFLAGS := -Wl,--no-undefined -Wl,--version-script=$(LOCAL_PATH)/src/SDL_ttf.sym

ifneq ($(FREETYPE_LIBRARY_PATH),)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(FREETYPE_LIBRARY_PATH)/include
    LOCAL_STATIC_LIBRARIES += freetype
endif

ifeq ($(SUPPORT_HARFBUZZ),true)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(HARFBUZZ_LIBRARY_PATH)/src
    LOCAL_CFLAGS += -DTTF_USE_HARFBUZZ
    LOCAL_STATIC_LIBRARIES += harfbuzz
endif

ifeq ($(SUPPORT_PLUTOSVG),true)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(PLUTOVG_LIBRARY_PATH)/include
    LOCAL_C_FLAGS += -DTTF_USE_PLUTOSVG -DPLUTOSVG_HAS_FREETYPE
    LOCAL_SRC_FILES += \
        external/plutosvg/source/plutosvg.c \
        external/plutovg/source/plutovg-blend.c \
        external/plutovg/source/plutovg-canvas.c \
        external/plutovg/source/plutovg-font.c \
        external/plutovg/source/plutovg-ft-math.c \
        external/plutovg/source/plutovg-ft-raster.c \
        external/plutovg/source/plutovg-ft-stroker.c \
        external/plutovg/source/plutovg-matrix.c \
        external/plutovg/source/plutovg-paint.c \
        external/plutovg/source/plutovg-path.c \
        external/plutovg/source/plutovg-rasterize.c \
        external/plutovg/source/plutovg-surface.c
endif

LOCAL_SHARED_LIBRARIES := SDL3

LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include

include $(BUILD_SHARED_LIBRARY)

###########################
#
# SDL3_ttf static library
#
###########################

LOCAL_MODULE := SDL3_ttf_static

LOCAL_MODULE_FILENAME := libSDL3_ttf

LOCAL_LDLIBS :=
LOCAL_EXPORT_LDLIBS :=

include $(BUILD_STATIC_LIBRARY)

