#!/usr/bin/make -f
# Makefile for DISTRHO Plugins
# SPDX-License-Identifier: ISC

# ---------------------------------------------------------------------------------------------------------------------
# Include base makefile for a few definitions

include ../deps/dpf/Makefile.base.mk

ifeq ($(CPU_I386_OR_X86_64),true)
ifneq ($(WASM),true)
X86_RTCD = true
endif
endif

# ---------------------------------------------------------------------------------------------------------------------
# Project name, used for binaries

NAME = ReNooice

# ---------------------------------------------------------------------------------------------------------------------
# Directory setup

DPF_BUILD_DIR = ../build
DPF_TARGET_DIR = ../bin

RNNOISE_PATH = ../deps/rnnoise

# ---------------------------------------------------------------------------------------------------------------------
# Files to build

FILES_DSP = \
	PluginDSP.cpp \
	$(RNNOISE_PATH)/src/celt_lpc.c \
	$(RNNOISE_PATH)/src/denoise.c \
	$(RNNOISE_PATH)/src/kiss_fft.c \
	$(RNNOISE_PATH)/src/nnet.c \
	$(RNNOISE_PATH)/src/nnet_default.c \
	$(RNNOISE_PATH)/src/parse_lpcnet_weights.c \
	$(RNNOISE_PATH)/src/pitch.c \
	$(RNNOISE_PATH)/src/rnn.c \
	$(RNNOISE_PATH)/src/rnnoise_data.c \
	$(RNNOISE_PATH)/src/rnnoise_tables.c

ifeq ($(X86_RTCD),true)
FILES_DSP += \
	$(RNNOISE_PATH)/src/x86/nnet_avx2.c \
	$(RNNOISE_PATH)/src/x86/nnet_sse4_1.c \
	$(RNNOISE_PATH)/src/x86/x86cpu.c \
	$(RNNOISE_PATH)/src/x86/x86_dnn_map.c
endif

# ---------------------------------------------------------------------------------------------------------------------
# Do some magic

MAPI_MODULE_NAME = mapi_renooice

include ../deps/dpf/Makefile.plugins.mk

BASE_FLAGS += -DDISABLE_DEBUG_FLOAT
BASE_FLAGS += -DFLOAT_APPROX
BASE_FLAGS += -DRNNOISE_EXPORT=
BASE_FLAGS += -I$(RNNOISE_PATH)/include
BASE_FLAGS += -I$(RNNOISE_PATH)/src
# BASE_FLAGS += -fno-fast-math
# -Wno-sign-compare -Wno-parentheses -Wno-long-long

ifeq ($(X86_RTCD),true)
BASE_FLAGS += -DCPU_INFO_BY_ASM -DRNN_ENABLE_X86_RTCD

$(BUILD_DIR)/$(RNNOISE_PATH)/src/x86/nnet_avx2.c.o: BASE_FLAGS += -mavx -mfma -mavx2

$(BUILD_DIR)/$(RNNOISE_PATH)/src/x86/nnet_sse4_1.c.o: BASE_FLAGS += -msse4.1

else ifeq ($(WASM),true)

BASE_FLAGS += -U__AVX__ -U__SSE2__

endif

# ---------------------------------------------------------------------------------------------------------------------
# Enable all possible plugin types

all: clap dssi lv2_sep vst2 vst3

# ---------------------------------------------------------------------------------------------------------------------
