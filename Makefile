CC ?= cc
AR ?= ar

BUILD_DIR := build
LUA_DIR   := third_party/lua/src
LUA_LIB   := $(BUILD_DIR)/liblua.a
TARGET    := $(BUILD_DIR)/neoglobal

CPPFLAGS   := -Iinclude -I$(LUA_DIR)
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
	src/main.c \
	src/neoglobal_unity.c

NEO_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(NEO_SRCS))

.PHONY: all clean

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

clean:
	rm -rf $(BUILD_DIR)
