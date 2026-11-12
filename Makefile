# SPDX-License-Identifier: MIT
# GMOS.ELF for sandbox-kernel, built only against the vendored SDK in sdk/
# (sdk/PINNED names the kernel commit) and the shared Game-Maker play core in
# third_party/gmcore/ (PINNED names its commit). TOOLCHAIN is the Arm GNU toolchain prefix.
TOOLCHAIN ?= /Applications/ArmGNUToolchain/15.3.rel2/arm-none-eabi/bin/arm-none-eabi-
CC := $(TOOLCHAIN)gcc
CXX := $(TOOLCHAIN)g++
HOSTCC ?= cc
HOSTCXX ?= c++
HOST_SYSTEM := $(shell uname -s)
HOSTCXXLIB ?= $(if $(filter Darwin,$(HOST_SYSTEM)),-lc++,-lstdc++)
PYTHON ?= python3
# Keep the vendored SDK, puff and QuickJS on their original warning policy.
# These warning options are supported by both Clang and GCC.
VENDOR_WARNINGS := -Wall -Wextra -Werror
WARNINGS := $(VENDOR_WARNINGS) -Wcast-qual -Wshadow -Wmisleading-indentation -Wimplicit-fallthrough -Wformat=2
C_WARNINGS := $(WARNINGS) -Wmissing-prototypes -Wstrict-prototypes
CXX_WARNINGS := $(WARNINGS) -Wmissing-declarations
ARCH := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffreestanding -fno-builtin -nostdlib
# src/board: headers the board build has no C library for (setjmp.h, for puff).
SCAN_VERBOSE ?= 0
VENDOR_PFLAGS := $(ARCH) $(VENDOR_WARNINGS) -std=c11 -Isdk/include -Isrc -Isrc/board -Ithird_party/puff
PFLAGS := $(ARCH) $(C_WARNINGS) -DGMOS_SCAN_VERBOSE=$(SCAN_VERBOSE) -std=c11 -Isdk/include -Isrc -Isrc/board -Ithird_party/puff
# The core is header-only C++ (no exceptions, RTTI or static constructors).
# System includes keep vendor header diagnostics out of the project warning policy.
PXXFLAGS := $(ARCH) $(CXX_WARNINGS) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -Isrc -isystem third_party/gmcore
LFLAGS := -nostdlib -Tsdk/lib/sandbox.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,-z,max-page-size=4096
CORE := src/eink_page.c src/einkart_data.c src/opl.c src/opl_tables.c src/cmf.c src/audio.c src/uisound.c src/vocplay.c src/pcspeaker.c src/ostune_data.c src/asset_scratch.c src/zip.c src/gam.c src/menu.c src/collections.c src/settings.c src/library.c src/scan_cache.c src/thumb.c src/thumbq.c src/view.c src/font_data.c src/gif.c src/fli.c src/text.c src/font8_data.c src/session.c src/dosmenu.c src/sound.c src/hiscore.c src/savefile.c src/gmoslog.c src/hiscreen.c src/hisload.c src/saveplay.c src/bootart.c src/bootart_data.c src/headart_data.c src/beat.c src/entryloop.c
PLAYCORE := src/game.cpp
GMPCORE := src/gmp.cpp src/asset_scratch.h third_party/gmcore/gmsmall.h third_party/puff/puff.h
GMCORE := third_party/gmcore/gmcore.h third_party/gmcore/gmmon.h
SDK := sdk/lib/crt0.S src/libc.c
VENDOR_OBJS := build/sdk_mem.o build/puff.o
.PHONY: all host test test-asan clean js quota
all: build/GMOS.ELF
build:
	mkdir -p $@
src/font_data.c: tools/font.txt tools/make_font.py
	$(PYTHON) tools/make_font.py
build/game.o: $(PLAYCORE) src/game.h $(GMCORE) Makefile | build
	$(CXX) $(PXXFLAGS) -c $(PLAYCORE) -o $@
build/gmp.o: $(GMPCORE) src/gmp.h Makefile | build
	$(CXX) $(PXXFLAGS) -Ithird_party/puff -c src/gmp.cpp -o $@
build/sdk_mem.o: sdk/lib/mem.c Makefile | build
	$(CC) $(VENDOR_PFLAGS) -c $< -o $@
build/puff.o: third_party/puff/puff.c third_party/puff/puff.h Makefile | build
	$(CC) $(VENDOR_PFLAGS) -c $< -o $@
build/GMOS.ELF: src/input.c src/awake.c src/main.c src/eink.c src/savefile_sbx.c src/sdpack.c src/sdzip.c build/game.o build/gmp.o $(CORE) $(wildcard src/*.h) $(SDK) $(VENDOR_OBJS) sdk/lib/sandbox.ld $(wildcard sdk/include/sandbox/*.h) Makefile | build
	$(CC) $(PFLAGS) $(LFLAGS) $(SDK) $(VENDOR_OBJS) src/input.c src/awake.c src/main.c src/eink.c src/savefile_sbx.c src/sdpack.c src/sdzip.c $(CORE) build/game.o build/gmp.o -lgcc -o $@
# The pure parts (title reader, list, drawing, game, session) as a host library
# for tests/, and gmplay, which prints the Pocket Player host test's state lines.
HOSTDIR ?= build/host
HOSTFLAGS ?= -O1
VENDOR_HOSTFLAGS = $(HOSTFLAGS) -std=c11 $(VENDOR_WARNINGS)
VENDOR_QJS_HOSTFLAGS = $(HOSTFLAGS) -D_GNU_SOURCE -DNDEBUG -w
host: $(HOSTDIR)/scroll_check $(HOSTDIR)/frame_bench $(HOSTDIR)/libeink.so $(HOSTDIR)/libgmos.so $(HOSTDIR)/libsdpack.so $(HOSTDIR)/gmplay $(HOSTDIR)/jsrun
$(HOSTDIR):
	mkdir -p $@
$(HOSTDIR)/puff.o: third_party/puff/puff.c third_party/puff/puff.h Makefile | $(HOSTDIR)
	$(HOSTCC) $(VENDOR_HOSTFLAGS) -fPIC -c $< -o $@
$(HOSTDIR)/libgmos.so: $(HOSTDIR)/puff.o $(CORE) $(PLAYCORE) $(GMCORE) $(GMPCORE) $(wildcard src/*.h) tests/host/hostio.c tests/host/hostio.h Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -fPIC $(CXX_WARNINGS) -Isrc -isystem third_party/gmcore -c $(PLAYCORE) -o $(HOSTDIR)/game.o
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -fPIC $(CXX_WARNINGS) -Isrc -isystem third_party/gmcore -Ithird_party/puff -c src/gmp.cpp -o $(HOSTDIR)/gmp.o
	$(HOSTCC) -shared -fPIC $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Isrc -Ithird_party/puff $(CORE) tests/host/hostio.c $(HOSTDIR)/puff.o $(HOSTDIR)/game.o $(HOSTDIR)/gmp.o $(HOSTCXXLIB) -o $@
# Exercise the production e-ink client with a fake device and monotonic clock.
$(HOSTDIR)/libeink.so: src/eink.c src/eink.h src/bootart_data.c src/font_data.c tests/host/eink.c tests/host/eink/sandbox/syscall.h $(wildcard sdk/include/sandbox/*.h) Makefile | $(HOSTDIR)
	$(HOSTCC) -shared -fPIC $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Itests/host/eink -Isdk/include -Isrc src/eink.c src/bootart_data.c src/font_data.c tests/host/eink.c -o $@
# Board pack reader with a host transport shim; gmp.o is built by libgmos above.
$(HOSTDIR)/libsdpack.so: $(HOSTDIR)/libgmos.so src/sdpack.c tests/host/sdpack.c tests/host/sdpack/sandbox/syscall.h $(wildcard sdk/include/sandbox/*.h) Makefile
	$(HOSTCC) -shared -fPIC $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Itests/host/sdpack -Isdk/include -Isrc -Ithird_party/puff src/asset_scratch.c src/sdpack.c tests/host/sdpack.c $(HOSTDIR)/puff.o $(HOSTDIR)/gmp.o $(HOSTCXXLIB) -o $@
$(HOSTDIR)/gmplay: tests/host/gmplay.cpp tests/host/hostio.h $(PLAYCORE) $(GMCORE) $(GMPCORE) src/game.h Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) $(CXX_WARNINGS) -Isrc -isystem third_party/gmcore -Ithird_party/puff -c src/gmp.cpp -o $(HOSTDIR)/gmplay_gmp.o
	$(HOSTCC) $(VENDOR_HOSTFLAGS) -c third_party/puff/puff.c -o $(HOSTDIR)/gmplay_puff.o
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Isrc -c src/asset_scratch.c -o $(HOSTDIR)/gmplay_scratch.o
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) $(CXX_WARNINGS) -Isrc -isystem third_party/gmcore -Ithird_party/puff $(PLAYCORE) tests/host/gmplay.cpp $(HOSTDIR)/gmplay_gmp.o $(HOSTDIR)/gmplay_puff.o $(HOSTDIR)/gmplay_scratch.o -o $@
# Per-card thumbnail cost on the host: make thumbcost GAMES=<folder of game folders> [SUB=/disk]
thumbcost: host tests/host/thumbcost.c
	$(HOSTCC) $(HOSTFLAGS) -std=gnu11 -Isrc -isystem third_party/gmcore tests/host/thumbcost.c $(HOSTDIR)/libgmos.so -Wl,-rpath,$(abspath $(HOSTDIR)) -o $(HOSTDIR)/thumbcost
	$(HOSTDIR)/thumbcost $(GAMES) $(SUB)
# The kernel refuses a program whose loaded image tops U_IMAGE_QUOTA (8 MiB): fail while 64 KiB short of it.
quota: build/GMOS.ELF
	$(PYTHON) tools/check_quota.py build/GMOS.ELF $(wildcard build/js/JS.full.elf)
test: host quota
	GMOS_HOST=$(HOSTDIR) $(PYTHON) -m unittest discover -s tests -v
# The host suite again with AddressSanitizer and UndefinedBehaviorSanitizer in
# build/asan: any overflow or undefined behavior stops the run with a report.
# Python loads the library, so the ASan runtime is preloaded into it (macOS);
# on Linux, LD_PRELOAD of libasan does the same. First the C fuzz driver
# (tests/host/fuzz.c; FUZZ_GAMES: folders of game folders it also lists and draws).
FUZZ_ROUNDS ?= 300
FUZZ_GAMES ?=
ASAN := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined
ASANRT = $(shell $(HOSTCC) -print-file-name=$(if $(filter Darwin,$(HOST_SYSTEM)),libclang_rt.asan_osx_dynamic.dylib,libasan.so))
ASAN_PRELOAD = $(if $(filter Darwin,$(HOST_SYSTEM)),DYLD_INSERT_LIBRARIES,LD_PRELOAD)=$(ASANRT)
ASANPY = $(shell tools/asan_python.sh $(PYTHON))
test-asan:
	$(MAKE) HOSTDIR=build/asan HOSTFLAGS="$(ASAN)" host build/asan/fuzz
	ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/asan/fuzz tests/games $(FUZZ_ROUNDS) $(FUZZ_GAMES)
	$(ASAN_PRELOAD) ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	  GMOS_HOST=build/asan $(ASANPY) -m unittest discover -s tests
$(HOSTDIR)/fuzz: tests/host/fuzz.c tests/host/hostio.h $(HOSTDIR)/libgmos.so | $(HOSTDIR)
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Isrc -isystem third_party/gmcore tests/host/fuzz.c $(HOSTDIR)/libgmos.so -o $@
# JS.ELF: the JS runtime spike (src/js/, docs/js.md): QuickJS-ng in
# third_party/quickjs-ng, the element tree, the game element over the same play
# core, newlib's libc and libm from the toolchain under a fixed heap (src/js/sys.c).
QJS := quickjs libregexp libunicode dtoa
QJSFLAGS := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffunction-sections -fdata-sections -D__DJGPP -DJS_HAVE_THREADS=0 -DNDEBUG -Wno-implicit-function-declaration -w
JSC := src/js/jsrt.c src/js/dom.c src/session.c src/dosmenu.c src/sound.c src/gif.c src/fli.c src/text.c src/thumb.c src/font_data.c src/font8_data.c
JSFLAGS := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffunction-sections -fdata-sections -std=gnu11 $(C_WARNINGS) -Wno-main -Isdk/include -Isrc -isystem third_party/quickjs-ng
js: build/JS.ELF
build/js:
	mkdir -p $@
build/js/prelude.c: src/js/prelude.js tools/embed_js.py | build/js
	$(PYTHON) tools/embed_js.py $< $@ jsrt_prelude
build/js/picker.c: src/js/picker.js tools/embed_js.py | build/js
	$(PYTHON) tools/embed_js.py $< $@ jsrt_picker
build/js/q_%.o: third_party/quickjs-ng/%.c $(wildcard third_party/quickjs-ng/*.h) Makefile | build/js
	$(CC) $(QJSFLAGS) -c $< -o $@
build/js/game.o: $(PLAYCORE) src/game.h $(GMCORE) Makefile | build/js
	$(CXX) $(PXXFLAGS) -ffunction-sections -fdata-sections -c $(PLAYCORE) -o $@
build/JS.ELF: src/js/main.c src/js/plat_sbx.c src/js/sys.c $(JSC) build/js/prelude.c build/js/picker.c $(QJS:%=build/js/q_%.o) build/js/game.o $(wildcard src/js/*.h) $(wildcard src/*.h) sdk/lib/crt0.S sdk/lib/sandbox.ld Makefile
	$(CC) $(JSFLAGS) -nostartfiles -Tsdk/lib/sandbox.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,-z,max-page-size=4096 -Wl,--gc-sections \
	  sdk/lib/crt0.S src/js/main.c src/js/plat_sbx.c src/js/sys.c $(JSC) build/js/prelude.c build/js/picker.c $(QJS:%=build/js/q_%.o) build/js/game.o -lm -lc -lgcc -o build/js/JS.full.elf
	$(TOOLCHAIN)size build/js/JS.full.elf
	$(TOOLCHAIN)strip -o $@ build/js/JS.full.elf
# The runtime on the host: tests/host/jsrun.c (tests/test_js.py and the benchmarks).
$(HOSTDIR)/js_prelude.c: src/js/prelude.js tools/embed_js.py | $(HOSTDIR)
	$(PYTHON) tools/embed_js.py $< $@ jsrt_prelude
$(HOSTDIR)/js_picker.c: src/js/picker.js tools/embed_js.py | $(HOSTDIR)
	$(PYTHON) tools/embed_js.py $< $@ jsrt_picker
$(HOSTDIR)/q_%.o: third_party/quickjs-ng/%.c $(wildcard third_party/quickjs-ng/*.h) Makefile | $(HOSTDIR)
	$(HOSTCC) $(VENDOR_QJS_HOSTFLAGS) -c $< -o $@
$(HOSTDIR)/jsrun: tests/host/jsrun.c $(JSC) $(HOSTDIR)/js_prelude.c $(HOSTDIR)/js_picker.c $(QJS:%=$(HOSTDIR)/q_%.o) $(PLAYCORE) $(GMCORE) $(wildcard src/js/*.h) $(wildcard src/*.h) Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) $(CXX_WARNINGS) -Isrc -isystem third_party/gmcore -c $(PLAYCORE) -o $(HOSTDIR)/js_game.o
	$(HOSTCC) $(HOSTFLAGS) -std=gnu11 $(C_WARNINGS) -Isrc -isystem third_party/quickjs-ng tests/host/jsrun.c $(JSC) $(HOSTDIR)/js_prelude.c $(HOSTDIR)/js_picker.c $(QJS:%=$(HOSTDIR)/q_%.o) $(HOSTDIR)/js_game.o $(HOSTCXXLIB) -lm -o $@
clean:
	rm -rf build

$(HOSTDIR)/scroll_check: tests/host/scroll_check.cpp tests/host/hostio.h $(HOSTDIR)/libgmos.so Makefile
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) $(CXX_WARNINGS) -Isrc -isystem third_party/gmcore $< $(HOSTDIR)/libgmos.so -o $@
# Reproducible frame pipeline benchmark (first scene, scripted left/right input).
$(HOSTDIR)/frame_bench: tests/host/frame_bench.cpp tests/host/hostio.h $(HOSTDIR)/libgmos.so Makefile
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) $(CXX_WARNINGS) -Isrc -isystem third_party/gmcore $< $(HOSTDIR)/libgmos.so -o $@

# Long-session timing regression using production session/core, without ctypes.
host: $(HOSTDIR)/timing_check
$(HOSTDIR)/timing_check: tests/host/timing_check.c tests/host/hostio.h $(HOSTDIR)/libgmos.so Makefile
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Isrc -Ithird_party/puff $< $(HOSTDIR)/libgmos.so -o $@

# Production power client and SDK records through a fake raw syscall transport.
host: $(HOSTDIR)/awake_check
$(HOSTDIR)/awake_check: src/awake.c src/awake.h src/session.h tests/host/awake_check.c $(HOSTDIR)/libgmos.so $(wildcard sdk/include/sandbox/*.h) Makefile | $(HOSTDIR)
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -DHOST_TEST -Isdk/include -Isrc src/awake.c tests/host/awake_check.c $(HOSTDIR)/libgmos.so -o $@

# Native remote settings, persistence and ABI 1.11 timeout records.
host: $(HOSTDIR)/settings_check
$(HOSTDIR)/settings_check: src/awake.c tests/host/settings_check.c $(HOSTDIR)/libgmos.so $(wildcard src/*.h) $(wildcard sdk/include/sandbox/*.h) Makefile | $(HOSTDIR)
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -DHOST_TEST -Isdk/include -Isrc src/awake.c tests/host/settings_check.c $(HOSTDIR)/libgmos.so -o $@

# Menu UI sounds: cues per action, the off switch, burst bounds.
host: $(HOSTDIR)/uisound_check
$(HOSTDIR)/uisound_check: tests/host/uisound_check.c $(HOSTDIR)/libgmos.so $(wildcard src/*.h) Makefile | $(HOSTDIR)
	$(HOSTCC) $(HOSTFLAGS) -std=c11 -D_POSIX_C_SOURCE=200809L $(C_WARNINGS) -Isrc tests/host/uisound_check.c $(HOSTDIR)/libgmos.so -o $@

# Pages and animations against the kernel's heartbeat (bounded steps, hook, wakes).
host: $(HOSTDIR)/fli_pages_check
$(HOSTDIR)/fli_pages_check: tests/host/fli_pages_check.c tests/host/hostio.h src/session.h $(HOSTDIR)/libgmos.so Makefile
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Isrc tests/host/fli_pages_check.c $(HOSTDIR)/libgmos.so -o $@

# The name-entry loop keeps the audio pump and heartbeat running while it waits for keys.
host: $(HOSTDIR)/entry_check
$(HOSTDIR)/entry_check: tests/host/entry_check.c src/entryloop.h $(HOSTDIR)/libgmos.so Makefile
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Isrc tests/host/entry_check.c $(HOSTDIR)/libgmos.so -o $@

# The heartbeat call: health first (a late one survives the kernel's 2 s rule).
host: $(HOSTDIR)/beat_check
$(HOSTDIR)/beat_check: tests/host/beat_check.c src/beat.h $(HOSTDIR)/libgmos.so Makefile
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Isrc tests/host/beat_check.c $(HOSTDIR)/libgmos.so -o $@

# Production input reader with a fake event device; no display or audio.
host: $(HOSTDIR)/input_check
$(HOSTDIR)/input_check: src/input.c src/input.h tests/host/input_check.c tests/host/hostio.h tests/host/input/sandbox/syscall.h $(wildcard sdk/include/sandbox/*.h) $(HOSTDIR)/libgmos.so Makefile
	$(HOSTCC) $(HOSTFLAGS) -std=c11 $(C_WARNINGS) -Itests/host/input -Isdk/include -Isrc src/input.c tests/host/input_check.c $(HOSTDIR)/libgmos.so -o $@
