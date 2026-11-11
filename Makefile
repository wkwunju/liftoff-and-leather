# Liftoff & Leather — static web bundle (index.html + index.js + index.wasm)
EMCC ?= emcc
RAYLIB_DIR := third_party/raylib
RAYLIB_SRC := $(RAYLIB_DIR)/src
RAYLIB_A := $(RAYLIB_SRC)/libraylib.a

DIST := dist
SHELL_HTML := web/shell.html
SRC := src/game.c

WEB_EXPORTS := -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 -sGL_ENABLE_GET_PROC_ADDRESS=1

.PHONY: all clean distclean

all: $(DIST)/index.html $(DIST)/.nojekyll

$(RAYLIB_A):
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_WEB RAYLIB_MODULE_MODELS=FALSE RAYLIB_MODULE_AUDIO=FALSE RAYLIB_BUILD_MODE=RELEASE

$(DIST)/index.html: $(SRC) $(SHELL_HTML) $(RAYLIB_A)
	mkdir -p $(DIST)
	$(EMCC) $(SRC) -o $(DIST)/index.html \
		-I$(RAYLIB_SRC) $(RAYLIB_A) \
		-O2 -Wall -DPLATFORM_WEB \
		$(WEB_EXPORTS) \
		--shell-file $(SHELL_HTML)
	@echo "Built $(DIST)/index.html, $(DIST)/index.js, $(DIST)/index.wasm"

$(DIST)/.nojekyll:
	mkdir -p $(DIST)
	touch $(DIST)/.nojekyll

clean:
	rm -rf $(DIST)

distclean: clean
	$(MAKE) -C $(RAYLIB_SRC) clean || true
