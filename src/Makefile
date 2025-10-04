#!/usr/bin/make -f
# Makefile for DISTRHO Plugins
# SPDX-License-Identifier: ISC

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

ifeq ($(RTCD),1)
FILES_DSP += \
	$(RNNOISE_PATH)/src/x86/nnet_avx2.c \
	$(RNNOISE_PATH)/src/x86/nnet_sse4_1.c \
	$(RNNOISE_PATH)/src/x86/x86cpu.c \
	$(RNNOISE_PATH)/src/x86/x86_dnn_map.c
endif

# ---------------------------------------------------------------------------------------------------------------------
# Do some magic

include ../deps/dpf/Makefile.plugins.mk

# BASE_FLAGS += -DDISABLE_DEBUG_FLOAT
# BASE_FLAGS += -DFLOAT_APPROX
BASE_FLAGS += -DRNNOISE_EXPORT=
BASE_FLAGS += -I$(RNNOISE_PATH)/include
BASE_FLAGS += -I$(RNNOISE_PATH)/src
BASE_FLAGS += -fno-fast-math

ifeq ($(RTCD),1)
BASE_FLAGS += -DCPU_INFO_BY_ASM -DRNN_ENABLE_X86_RTCD -mavx -mfma -mavx2 -msse4.1
endif

# -Wno-sign-compare -Wno-parentheses -Wno-long-long

# ---------------------------------------------------------------------------------------------------------------------
# Enable all possible plugin types

all: au clap dssi lv2_sep vst2 vst3

# ---------------------------------------------------------------------------------------------------------------------
