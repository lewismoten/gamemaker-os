# SPDX-License-Identifier: MIT
# Game-Maker profile for Sandbox JS: builds the Sandbox JS core (a sandbox-kernel checkout, js/) with
# src/js/profile_gm.c and this repository's play core. Prototype; does not touch GMOS.ELF or JS.ELF
# (docs/js-profile.md). Usage, from the repository root:
#   make -f jsprof.mk host  SANDBOX_JS_DIR=/path/to/sandbox-kernel/js   -> build/jsprof/host/core/host/jsrun
#   make -f jsprof.mk size  SANDBOX_JS_DIR=...                          -> build/jsprof/board/core/JS.ELF and the size report
SANDBOX_JS_DIR ?= ../sandbox-kernel/js
TOOLCHAIN ?= /Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/arm-none-eabi-
HOSTCC ?= cc
HOSTCXX ?= c++
HOSTCXXLIB ?= $(if $(filter Darwin,$(shell uname -s)),-lc++,-lstdc++)
OUT := $(CURDIR)/build/jsprof
PROFILE := $(CURDIR)/src/js/profile_gm.c
# gif.c and font_data.c are the core's own (its gif.c has gp_gif, gp_gif_draw, gp_gif_palette).
LIBSRC := src/session.c src/sound.c src/fli.c src/text.c src/thumb.c src/font8_data.c
WARN := -Wall -Wextra -Werror -Wcast-qual -Wshadow -Wmisleading-indentation -Wimplicit-fallthrough -Wformat=2 -Wmissing-prototypes -Wstrict-prototypes
CXXWARN := -Wall -Wextra -Werror -Wcast-qual -Wshadow -Wmisleading-indentation -Wimplicit-fallthrough -Wformat=2 -Wmissing-declarations
ARMF := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffunction-sections -fdata-sections
HDR := $(wildcard src/*.h) $(wildcard third_party/gmcore/*.h)
.PHONY: host board size
$(OUT)/host $(OUT)/board:
	mkdir -p $@
$(OUT)/host/libs.stamp: $(LIBSRC) src/game.cpp $(HDR) jsprof.mk | $(OUT)/host
	for f in $(LIBSRC); do $(HOSTCC) -std=gnu11 -O1 -Isrc -c $$f -o $(OUT)/host/$$(basename $$f .c).o || exit 1; done
	$(HOSTCXX) -std=c++17 -O1 -Isrc -isystem third_party/gmcore -c src/game.cpp -o $(OUT)/host/game.o
	touch $@
host: $(OUT)/host/libs.stamp
	$(MAKE) -C $(SANDBOX_JS_DIR) host BUILD=$(OUT)/host/core JS_PROFILE=$(PROFILE) \
	  JS_PROFILE_LIBS='$(foreach f,$(LIBSRC),$(OUT)/host/$(basename $(notdir $f)).o) $(OUT)/host/game.o $(HOSTCXXLIB)'
$(OUT)/board/libs.stamp: $(LIBSRC) src/game.cpp $(HDR) jsprof.mk | $(OUT)/board
	for f in $(LIBSRC); do $(TOOLCHAIN)gcc $(ARMF) -std=gnu11 $(WARN) -Isrc -I$(SANDBOX_JS_DIR)/../sdk/include -c $$f -o $(OUT)/board/$$(basename $$f .c).o || exit 1; done
	$(TOOLCHAIN)g++ $(ARMF) $(CXXWARN) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -Isrc -isystem third_party/gmcore -c src/game.cpp -o $(OUT)/board/game.o
	touch $@
board: $(OUT)/board/libs.stamp
	$(MAKE) -C $(SANDBOX_JS_DIR) all BUILD=$(OUT)/board/core JS_PROFILE=$(PROFILE) \
	  JS_PROFILE_LIBS='$(foreach f,$(LIBSRC),$(OUT)/board/$(basename $(notdir $f)).o) $(OUT)/board/game.o'
size: board
	$(MAKE) -C $(SANDBOX_JS_DIR) size BUILD=$(OUT)/board/core JS_PROFILE=$(PROFILE) JS_PROFILE_LIBS=
