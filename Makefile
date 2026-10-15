CC ?= cc
AR ?= ar

NEOGLOBAL_REAL ?= f64

ifeq ($(NEOGLOBAL_REAL),f32)
NEOGLOBAL_REAL_CPPFLAGS := -DNEOGLOBAL_USE_F32
else ifneq ($(NEOGLOBAL_REAL),f64)
$(error NEOGLOBAL_REAL must be either f32 or f64)
endif

BUILD_DIR := build/$(NEOGLOBAL_REAL)
BUILD_ROOT := build
LUA_DIR   := third_party/lua/src
LUA_LIB   := $(BUILD_DIR)/liblua.a
TARGET    := $(BUILD_DIR)/neoglobal
TEST_TARGET := $(BUILD_DIR)/test_neoglobal
BBOB_TARGET := $(BUILD_ROOT)/bbob/neoglobal_bbob
BBOB_CMAES_TARGET := $(BUILD_ROOT)/bbob/cmaes_bbob
COCO_DIR ?= third_party/coco
COCO_C := $(COCO_DIR)/code-experiments/build/c/coco.c
COCO_CMAES_DIR := $(COCO_DIR)/code-experiments/examples/bbob2009-c-cmaes
COCO_CPPFLAGS ?= -I$(COCO_DIR)/code-experiments/build/c -I$(COCO_DIR)/code-experiments/src
COCO_LDLIBS ?= -lm

CPPFLAGS   := -Iinclude -I$(LUA_DIR) -Ithird_party/cfd_lib $(NEOGLOBAL_REAL_CPPFLAGS)
CFLAGS     ?= -O2 -std=c11
NEO_CFLAGS := $(CFLAGS) -Wall -Wextra -Wpedantic
LUA_CFLAGS := $(CFLAGS)

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
	LUA_CFLAGS += -DLUA_USE_LINUX
	LDLIBS += -lm -ldl
else ifeq ($(UNAME_S),Darwin)
	LUA_CFLAGS += -DLUA_USE_MACOSX
	LDLIBS += -lm
else
	LDLIBS += -lm
endif

# Lua library sources.
# lua.c and luac.c are intentionally excluded because they contain
# the standalone interpreter/compiler entry points.
LUA_SRCS := \
	lapi.c \
	lcode.c \
	lctype.c \
	ldebug.c \
	ldo.c \
	ldump.c \
	lfunc.c \
	lgc.c \
	llex.c \
	lmem.c \
	lobject.c \
	lopcodes.c \
	lparser.c \
	lstate.c \
	lstring.c \
	ltable.c \
	ltm.c \
	lundump.c \
	lvm.c \
	lzio.c \
	lauxlib.c \
	lbaselib.c \
	lcorolib.c \
	ldblib.c \
	liolib.c \
	lmathlib.c \
	loadlib.c \
	loslib.c \
	lstrlib.c \
	ltablib.c \
	lutf8lib.c \
	linit.c

LUA_OBJS := $(patsubst %.c,$(BUILD_DIR)/lua/%.o,$(LUA_SRCS))

# NeoGLOBAL uses a unity translation unit for its own implementation.
NEO_SRCS := \
	src/neoglobal_unity.c

NEO_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(NEO_SRCS))

.PHONY: all clean test bbob

all: $(TARGET)

$(TARGET): $(NEO_OBJS) $(LUA_LIB)
	$(CC) $(NEO_OBJS) $(LUA_LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(LUA_LIB): $(LUA_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(BUILD_DIR)/lua/%.o: $(LUA_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(LUA_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NEO_CFLAGS) -c $< -o $@

$(BUILD_DIR)/neoglobal_unity.o: src/main.c src/problem.c src/lua_problem.c src/sampling.c src/clustering.c src/local_search.c src/optimizer.c include/neoglobal/problem.h include/neoglobal/sampling.h include/neoglobal/clustering.h include/neoglobal/local_search.h include/neoglobal/optimizer.h third_party/cfd_lib/cfd_core.h

test: $(TEST_TARGET)
	$(TEST_TARGET)

# COCO is an optional dependency used only by the separate BBOB benchmark.
bbob: $(BBOB_TARGET) $(BBOB_CMAES_TARGET)

$(COCO_C):
	@echo "COCO C source not found at $(COCO_C). Initialize and build COCO first; see README.md."
	@false

$(BBOB_TARGET): src/neoglobal_bbob_unity.c src/bbob_main.c src/problem.c src/sampling.c src/clustering.c src/local_search.c src/optimizer.c include/neoglobal/problem.h include/neoglobal/sampling.h include/neoglobal/clustering.h include/neoglobal/local_search.h include/neoglobal/optimizer.h third_party/cfd_lib/cfd_core.h $(COCO_C)
	@mkdir -p $(dir $@)
	$(CC) -Iinclude -Ithird_party/cfd_lib $(NEO_CFLAGS) $(COCO_CPPFLAGS) $< $(LDFLAGS) $(COCO_C) $(COCO_LDLIBS) -o $@

$(BBOB_CMAES_TARGET): src/bbob_cmaes_main.c $(COCO_CMAES_DIR)/cmaes.c $(COCO_CMAES_DIR)/cmaes.h $(COCO_CMAES_DIR)/cmaes_interface.h $(COCO_C)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic $(COCO_CPPFLAGS) -I$(COCO_CMAES_DIR) $< $(COCO_CMAES_DIR)/cmaes.c $(COCO_C) $(LDFLAGS) $(COCO_LDLIBS) -o $@

$(TEST_TARGET): $(BUILD_DIR)/test_unity.o $(LUA_LIB)
	$(CC) $(BUILD_DIR)/test_unity.o $(LUA_LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/test_unity.o: tests/test_unity.c tests/test_neoglobal.c tests/problems/sphere_small.lua src/problem.c src/lua_problem.c src/sampling.c src/clustering.c src/local_search.c src/optimizer.c include/neoglobal/problem.h include/neoglobal/sampling.h include/neoglobal/clustering.h include/neoglobal/local_search.h include/neoglobal/optimizer.h third_party/cfd_lib/cfd_core.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(NEO_CFLAGS) -c tests/test_unity.c -o $@

clean:
	rm -rf $(BUILD_ROOT)
