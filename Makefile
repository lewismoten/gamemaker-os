# SPDX-License-Identifier: MIT
# GMOS.ELF for sandbox-kernel, built only against the vendored SDK in sdk/
# (sdk/PINNED names the kernel commit). TOOLCHAIN is the Arm GNU toolchain prefix.
TOOLCHAIN ?= /Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/arm-none-eabi-
CC := $(TOOLCHAIN)gcc
HOSTCC ?= cc
PYTHON ?= python3
PFLAGS := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -std=c11 -Os -ffreestanding -fno-builtin -nostdlib -Wall -Wextra -Werror \
  -Isdk/include -Isrc -Tsdk/lib/sandbox.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,-z,max-page-size=4096
CORE := src/gam.c src/menu.c src/view.c src/font_data.c
SDK := sdk/lib/crt0.S sdk/lib/mem.c
.PHONY: all host test clean
all: build/GMOS.ELF
build build/host:
	mkdir -p $@
src/font_data.c: tools/font.txt tools/make_font.py
	$(PYTHON) tools/make_font.py
build/GMOS.ELF: src/main.c $(CORE) $(wildcard src/*.h) $(SDK) sdk/lib/sandbox.ld $(wildcard sdk/include/sandbox/*.h) Makefile | build
	$(CC) $(PFLAGS) $(SDK) src/main.c $(CORE) -lgcc -o $@
# The pure parts (title reader, list, drawing) as a host library for tests/.
host: build/host/libgmos.so
build/host/libgmos.so: $(CORE) $(wildcard src/*.h) Makefile | build/host
	$(HOSTCC) -shared -fPIC -O1 -std=c11 -Wall -Wextra -Werror -Isrc $(CORE) -o $@
test: host
	$(PYTHON) -m unittest discover -s tests -v
clean:
	rm -rf build
