# AutoHeal Makefile
# Builds:
#   bin/autoheal      — main daemon
#   bin/rogue_cpu     — CPU spinner test program
#   bin/rogue_mem     — memory leak test program
#   bin/rogue_fork    — capped fork bomb test program
#
# Usage:
#   make            # build everything
#   make daemon     # build the daemon only
#   make rogues     # build the rogues only
#   make clean      # remove build artifacts
#   make run        # build and run in foreground

CXX       ?= g++
CXXSTD    := -std=c++17
WARN      := -Wall -Wextra -Wpedantic -Wno-unused-parameter
OPT       := -O2
DEBUG     := -g

CXXFLAGS  := $(CXXSTD) $(WARN) $(OPT) $(DEBUG) -pthread
LDFLAGS   := -pthread

# Daemon needs Boost.System for websocketpp's asio transport.
DAEMON_LIBS := -lboost_system

BUILD_DIR := build
BIN_DIR   := bin

DAEMON_SRCS := \
    src/main.cpp \
    src/engine/engine.cpp \
    src/observer/observer.cpp \
    src/brain/brain.cpp \
    src/healer/healer.cpp \
    src/interface/ws_server.cpp \
    src/interface/json_serializer.cpp \
    src/common/snapshot.cpp \
    src/common/ignore_list.cpp \
    src/common/logger.cpp

DAEMON_OBJS := $(DAEMON_SRCS:%.cpp=$(BUILD_DIR)/%.o)

ROGUE_SRCS := \
    rogues/rogue_cpu.cpp \
    rogues/rogue_mem.cpp \
    rogues/rogue_fork.cpp

ROGUE_BINS := $(ROGUE_SRCS:rogues/%.cpp=$(BIN_DIR)/%)

.PHONY: all daemon rogues clean run dirs

all: daemon rogues

daemon: dirs $(BIN_DIR)/autoheal

rogues: dirs $(ROGUE_BINS)

dirs:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)/src/engine $(BUILD_DIR)/src/observer \
	          $(BUILD_DIR)/src/brain $(BUILD_DIR)/src/healer \
	          $(BUILD_DIR)/src/interface $(BUILD_DIR)/src/common

$(BIN_DIR)/autoheal: $(DAEMON_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(DAEMON_LIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BIN_DIR)/rogue_%: rogues/rogue_%.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

run: daemon
	./$(BIN_DIR)/autoheal --foreground

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
