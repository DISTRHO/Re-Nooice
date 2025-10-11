#!/usr/bin/make -f
# Makefile for DISTRHO Plugins
# SPDX-License-Identifier: ISC

# ---------------------------------------------------------------------------------------------------------------------
# Project name, used for binaries

NAME = ReSpeex

# ---------------------------------------------------------------------------------------------------------------------
# Directory setup

DPF_BUILD_DIR = ../build/respeex
DPF_TARGET_DIR = ../bin
SPEEXDSP_PATH = ../deps/speexdsp

# ---------------------------------------------------------------------------------------------------------------------
# Files to build

FILES_DSP = \
	PluginDSP.cpp \
	$(SPEEXDSP_PATH)/libspeexdsp/fftwrap.c \
	$(SPEEXDSP_PATH)/libspeexdsp/filterbank.c \
	$(SPEEXDSP_PATH)/libspeexdsp/kiss_fft.c \
	$(SPEEXDSP_PATH)/libspeexdsp/kiss_fftr.c \
	$(SPEEXDSP_PATH)/libspeexdsp/mdf.c \
	$(SPEEXDSP_PATH)/libspeexdsp/preprocess.c

# ---------------------------------------------------------------------------------------------------------------------
# Do some magic

MAPI_MODULE_NAME = mapi_respeex

include ../deps/dpf/Makefile.plugins.mk

BASE_FLAGS += -DEXPORT=
BASE_FLAGS += -DFLOATING_POINT
BASE_FLAGS += -DUSE_KISS_FFT
BASE_FLAGS += -I$(SPEEXDSP_PATH)/include

# ---------------------------------------------------------------------------------------------------------------------
# Enable all possible plugin types

all: clap jack ladspa lv2_sep vst2 vst3

# ---------------------------------------------------------------------------------------------------------------------
