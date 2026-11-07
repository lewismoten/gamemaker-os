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
CORE := src/gam.c src/menu.c src/library.c src/thumb.c src/view.c src/font_data.c src/gif.c src/fli.c src/text.c src/font8_data.c src/session.c
PLAYCORE := src/game.cpp
GMCORE := third_party/gmcore/gmcore.h third_party/gmcore/gmmon.h
SDK := sdk/lib/crt0.S sdk/lib/mem.c src/libc.c
.PHONY: all host test test-asan clean
all: build/GMOS.ELF
build:
	mkdir -p $@
src/font_data.c: tools/font.txt tools/make_font.py
	$(PYTHON) tools/make_font.py
build/game.o: $(PLAYCORE) src/game.h $(GMCORE) Makefile | build
	$(CXX) $(PXXFLAGS) -c $(PLAYCORE) -o $@
build/GMOS.ELF: src/main.c build/game.o $(CORE) $(wildcard src/*.h) $(SDK) sdk/lib/sandbox.ld $(wildcard sdk/include/sandbox/*.h) Makefile | build
	$(CC) $(PFLAGS) $(LFLAGS) $(SDK) src/main.c $(CORE) build/game.o -lgcc -o $@
# The pure parts (title reader, list, drawing, game, session) as a host library
# for tests/, and gmplay, which prints the Pocket Player host test's state lines.
HOSTDIR ?= build/host
HOSTFLAGS ?= -O1
host: $(HOSTDIR)/libgmos.so $(HOSTDIR)/gmplay
$(HOSTDIR):
	mkdir -p $@
$(HOSTDIR)/libgmos.so: $(CORE) $(PLAYCORE) $(GMCORE) $(wildcard src/*.h) tests/host/hostio.c Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -fPIC -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore -c $(PLAYCORE) -o $(HOSTDIR)/game.o
	$(HOSTCC) -shared -fPIC $(HOSTFLAGS) -std=c11 -Wall -Wextra -Werror -Isrc $(CORE) tests/host/hostio.c $(HOSTDIR)/game.o -lc++ -o $@
$(HOSTDIR)/gmplay: tests/host/gmplay.cpp tests/host/hostio.h $(PLAYCORE) $(GMCORE) src/game.h Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore $(PLAYCORE) tests/host/gmplay.cpp -o $@
test: host
	GMOS_HOST=$(HOSTDIR) $(PYTHON) -m unittest discover -s tests -v
# The host suite again with AddressSanitizer and UndefinedBehaviorSanitizer in
# build/asan: any overflow or undefined behavior stops the run with a report.
# Python loads the library, so the ASan runtime is preloaded into it (macOS);
# on Linux, LD_PRELOAD of libasan does the same. First the C fuzz driver
# (tests/host/fuzz.c; FUZZ_GAMES: folders of game folders it also lists and draws).
FUZZ_ROUNDS ?= 300
FUZZ_GAMES ?=
ASAN := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined
ASANRT = $(shell $(HOSTCC) -print-file-name=libclang_rt.asan_osx_dynamic.dylib)
ASANPY = $(shell tools/asan_python.sh $(PYTHON))
test-asan:
	$(MAKE) HOSTDIR=build/asan HOSTFLAGS="$(ASAN)" host build/asan/fuzz
	ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/asan/fuzz tests/games $(FUZZ_ROUNDS) $(FUZZ_GAMES)
	DYLD_INSERT_LIBRARIES=$(ASANRT) ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	  GMOS_HOST=build/asan $(ASANPY) -m unittest discover -s tests
$(HOSTDIR)/fuzz: tests/host/fuzz.c tests/host/hostio.h $(HOSTDIR)/libgmos.so | $(HOSTDIR)
	$(HOSTCC) $(HOSTFLAGS) -std=c11 -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore tests/host/fuzz.c $(HOSTDIR)/libgmos.so -o $@
clean:
	rm -rf build
