#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

include deps/dpf/Makefile.base.mk

all: renooice gen

# ---------------------------------------------------------------------------------------------------------------------

renooice: models
	$(MAKE) -C src

ifneq ($(CROSS_COMPILING),true)
gen: renooice deps/dpf/utils/lv2_ttl_generator
	@$(CURDIR)/deps/dpf/utils/generate-ttl.sh

deps/dpf/utils/lv2_ttl_generator:
	$(MAKE) -C deps/dpf/utils/lv2-ttl-generator
else
gen:
endif

# ---------------------------------------------------------------------------------------------------------------------
# auto-download model files

models: deps/rnnoise/src/rnnoise_data.h

deps/rnnoise/src/rnnoise_data.h:
	cd deps/rnnoise && sh download_model.sh

# ---------------------------------------------------------------------------------------------------------------------

clean:
	$(MAKE) clean -C deps/dpf/utils/lv2-ttl-generator
	$(MAKE) clean -C src
	rm -f deps/rnnoise/src/*.d
	rm -f deps/rnnoise/src/*.o
	rm -rf bin build

distclean: clean
	rm -f deps/rnnoise/src/rnnoise_data.*
	rm -f deps/rnnoise/src/rnnoise_data_little.*
	rm -rf deps/rnnoise/models

# ---------------------------------------------------------------------------------------------------------------------
