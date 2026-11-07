# SPDX-License-Identifier: MIT
# GMOS.ELF for sandbox-kernel, built only against the vendored SDK in sdk/
# (sdk/PINNED names the kernel commit) and the shared Game-Maker play core in
# third_party/gmcore/ (PINNED names its commit). TOOLCHAIN is the Arm GNU toolchain prefix.
TOOLCHAIN ?= /Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/arm-none-eabi-
CC := $(TOOLCHAIN)gcc
CXX := $(TOOLCHAIN)g++
HOSTCC ?= cc
HOSTCXX ?= c++
PYTHON ?= python3
ARCH := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffreestanding -fno-builtin -nostdlib -Wall -Wextra -Werror
PFLAGS := $(ARCH) -std=c11 -Isdk/include -Isrc
# The core is header-only C++ (no exceptions, RTTI or static constructors).
PXXFLAGS := $(ARCH) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -Isrc -Ithird_party/gmcore
LFLAGS := -nostdlib -Tsdk/lib/sandbox.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,-z,max-page-size=4096
CORE := src/gam.c src/menu.c src/view.c src/font_data.c src/gif.c src/fli.c src/text.c src/font8_data.c src/session.c
PLAYCORE := src/game.cpp
GMCORE := third_party/gmcore/gmcore.h third_party/gmcore/gmmon.h
SDK := sdk/lib/crt0.S sdk/lib/mem.c src/libc.c
.PHONY: all host test clean
all: build/GMOS.ELF
build build/host:
	mkdir -p $@
src/font_data.c: tools/font.txt tools/make_font.py
	$(PYTHON) tools/make_font.py
build/game.o: $(PLAYCORE) src/game.h $(GMCORE) Makefile | build
	$(CXX) $(PXXFLAGS) -c $(PLAYCORE) -o $@
build/GMOS.ELF: src/main.c build/game.o $(CORE) $(wildcard src/*.h) $(SDK) sdk/lib/sandbox.ld $(wildcard sdk/include/sandbox/*.h) Makefile | build
	$(CC) $(PFLAGS) $(LFLAGS) $(SDK) src/main.c $(CORE) build/game.o -lgcc -o $@
# The pure parts (title reader, list, drawing, game, session) as a host library
# for tests/, and gmplay, which prints the Pocket Player host test's state lines.
host: build/host/libgmos.so build/host/gmplay
build/host/libgmos.so: $(CORE) $(PLAYCORE) $(GMCORE) $(wildcard src/*.h) tests/host/hostio.c Makefile | build/host
	$(HOSTCXX) -std=c++17 -O1 -fPIC -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore -c $(PLAYCORE) -o build/host/game.o
	$(HOSTCC) -shared -fPIC -O1 -std=c11 -Wall -Wextra -Werror -Isrc $(CORE) tests/host/hostio.c build/host/game.o -lc++ -o $@
build/host/gmplay: tests/host/gmplay.cpp tests/host/hostio.h $(PLAYCORE) $(GMCORE) src/game.h Makefile | build/host
	$(HOSTCXX) -std=c++17 -O1 -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore $(PLAYCORE) tests/host/gmplay.cpp -o $@
test: host
	$(PYTHON) -m unittest discover -s tests -v
clean:
	rm -rf build
