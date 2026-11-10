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
ARCH := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffreestanding -fno-builtin -nostdlib -Wall -Wextra -Werror
# src/board: headers the board build has no C library for (setjmp.h, for puff).
SCAN_VERBOSE ?= 0
PFLAGS := $(ARCH) -DGMOS_SCAN_VERBOSE=$(SCAN_VERBOSE) -std=c11 -Isdk/include -Isrc -Isrc/board -Ithird_party/puff
# The core is header-only C++ (no exceptions, RTTI or static constructors).
PXXFLAGS := $(ARCH) -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -Isrc -Ithird_party/gmcore
LFLAGS := -nostdlib -Tsdk/lib/sandbox.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,-z,max-page-size=4096
CORE := third_party/puff/puff.c src/asset_scratch.c src/zip.c src/gam.c src/menu.c src/library.c src/scan_cache.c src/thumb.c src/view.c src/font_data.c src/gif.c src/fli.c src/text.c src/font8_data.c src/session.c src/sound.c src/hiscore.c src/savefile.c src/hiscreen.c src/hisload.c src/bootart.c src/bootart_data.c src/headart_data.c src/eink.c src/einkart_data.c src/opl.c src/opl_tables.c src/cmf.c src/audio.c src/vocplay.c src/pcspeaker.c src/ostune_data.c
PLAYCORE := src/game.cpp
GMPCORE := src/gmp.cpp src/asset_scratch.h third_party/gmcore/gmsmall.h third_party/puff/puff.h
GMCORE := third_party/gmcore/gmcore.h third_party/gmcore/gmmon.h
# The OS's own build line for the e-ink title: the short git hash, "+" when the tree has uncommitted changes.
# build/buildid is rewritten only when it changes, so a new commit rebuilds the ELF and nothing else.
GMOS_BUILD := $(shell h=$$(git rev-parse --short=7 HEAD 2>/dev/null || echo unknown); git diff --quiet HEAD -- 2>/dev/null || h="$$h+"; echo "$$h")
SDK := sdk/lib/crt0.S sdk/lib/mem.c src/libc.c
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
build/buildid: FORCE | build
	@echo '$(GMOS_BUILD)' | cmp -s - $@ || echo '$(GMOS_BUILD)' > $@
FORCE:
build/GMOS.ELF: build/buildid src/main.c src/savefile_sbx.c src/sdpack.c src/sdzip.c build/game.o build/gmp.o $(CORE) $(wildcard src/*.h) $(SDK) sdk/lib/sandbox.ld $(wildcard sdk/include/sandbox/*.h) Makefile | build
	$(CC) $(PFLAGS) -DGMOS_BUILD='"$(GMOS_BUILD)"' $(LFLAGS) $(SDK) src/main.c src/savefile_sbx.c src/sdpack.c src/sdzip.c $(CORE) build/game.o build/gmp.o -lgcc -o $@
# The pure parts (title reader, list, drawing, game, session) as a host library
# for tests/, and gmplay, which prints the Pocket Player host test's state lines.
HOSTDIR ?= build/host
HOSTFLAGS ?= -O1
host: $(HOSTDIR)/libgmos.so $(HOSTDIR)/libsdpack.so $(HOSTDIR)/gmplay $(HOSTDIR)/jsrun
$(HOSTDIR):
	mkdir -p $@
$(HOSTDIR)/libgmos.so: $(CORE) $(PLAYCORE) $(GMCORE) $(GMPCORE) $(wildcard src/*.h) tests/host/hostio.c tests/host/hostio.h Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -fPIC -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore -c $(PLAYCORE) -o $(HOSTDIR)/game.o
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -fPIC -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore -Ithird_party/puff -c src/gmp.cpp -o $(HOSTDIR)/gmp.o
	$(HOSTCC) -shared -fPIC $(HOSTFLAGS) -std=c11 -Wall -Wextra -Werror -Isrc -Ithird_party/puff $(CORE) tests/host/hostio.c $(HOSTDIR)/game.o $(HOSTDIR)/gmp.o $(HOSTCXXLIB) -o $@
# Board pack reader with a host transport shim; gmp.o is built by libgmos above.
$(HOSTDIR)/libsdpack.so: $(HOSTDIR)/libgmos.so src/sdpack.c tests/host/sdpack.c tests/host/sdpack/sandbox/syscall.h $(wildcard sdk/include/sandbox/*.h) Makefile
	$(HOSTCC) -shared -fPIC $(HOSTFLAGS) -std=c11 -Wall -Wextra -Werror -Itests/host/sdpack -Isdk/include -Isrc -Ithird_party/puff src/asset_scratch.c src/sdpack.c tests/host/sdpack.c third_party/puff/puff.c $(HOSTDIR)/gmp.o $(HOSTCXXLIB) -o $@
$(HOSTDIR)/gmplay: tests/host/gmplay.cpp tests/host/hostio.h $(PLAYCORE) $(GMCORE) $(GMPCORE) src/game.h Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore -Ithird_party/puff -c src/gmp.cpp -o $(HOSTDIR)/gmplay_gmp.o
	$(HOSTCC) $(HOSTFLAGS) -std=c11 -Wall -Wextra -Werror -c third_party/puff/puff.c -o $(HOSTDIR)/gmplay_puff.o
	$(HOSTCC) $(HOSTFLAGS) -Isrc -c src/asset_scratch.c -o $(HOSTDIR)/gmplay_scratch.o
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore -Ithird_party/puff $(PLAYCORE) tests/host/gmplay.cpp $(HOSTDIR)/gmplay_gmp.o $(HOSTDIR)/gmplay_puff.o $(HOSTDIR)/gmplay_scratch.o -o $@
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
	$(HOSTCC) $(HOSTFLAGS) -std=c11 -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore tests/host/fuzz.c $(HOSTDIR)/libgmos.so -o $@
# JS.ELF: the JS runtime spike (src/js/, docs/js.md): QuickJS-ng in
# third_party/quickjs-ng, the element tree, the game element over the same play
# core, newlib's libc and libm from the toolchain under a fixed heap (src/js/sys.c).
QJS := quickjs libregexp libunicode dtoa
QJSFLAGS := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffunction-sections -fdata-sections -D__DJGPP -DJS_HAVE_THREADS=0 -DNDEBUG -Wno-implicit-function-declaration -w
JSC := src/js/jsrt.c src/js/dom.c src/session.c src/sound.c src/gif.c src/fli.c src/text.c src/thumb.c src/font_data.c src/font8_data.c
JSFLAGS := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -Os -ffunction-sections -fdata-sections -std=gnu11 -Wall -Wextra -Werror -Wno-main -Isdk/include -Isrc -Ithird_party/quickjs-ng
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
$(HOSTDIR)/q_%.o: third_party/quickjs-ng/%.c $(wildcard third_party/quickjs-ng/*.h) | $(HOSTDIR)
	$(HOSTCC) $(HOSTFLAGS) -D_GNU_SOURCE -DNDEBUG -w -c $< -o $@
$(HOSTDIR)/jsrun: tests/host/jsrun.c $(JSC) $(HOSTDIR)/js_prelude.c $(HOSTDIR)/js_picker.c $(QJS:%=$(HOSTDIR)/q_%.o) $(PLAYCORE) $(GMCORE) $(wildcard src/js/*.h) $(wildcard src/*.h) Makefile | $(HOSTDIR)
	$(HOSTCXX) -std=c++17 $(HOSTFLAGS) -Wall -Wextra -Werror -Isrc -Ithird_party/gmcore -c $(PLAYCORE) -o $(HOSTDIR)/js_game.o
	$(HOSTCC) $(HOSTFLAGS) -std=gnu11 -Wall -Wextra -Werror -Isrc -Ithird_party/quickjs-ng tests/host/jsrun.c $(JSC) $(HOSTDIR)/js_prelude.c $(HOSTDIR)/js_picker.c $(QJS:%=$(HOSTDIR)/q_%.o) $(HOSTDIR)/js_game.o $(HOSTCXXLIB) -lm -o $@
clean:
	rm -rf build
