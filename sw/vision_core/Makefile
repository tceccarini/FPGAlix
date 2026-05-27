CXX        ?= g++
PKG_CONFIG ?= pkg-config
CXXFLAGS   ?= -std=c++17 -Wall -Wextra -O2

CXXFLAGS_COMMON := -std=c++17 -Wall -Wextra -O2
CXXFLAGS_ARM    := -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -ftree-vectorize

BUILDROOT_HOST  := ../../repos/buildroot/output/host

INCLUDES := -Isrc \
            $(shell $(PKG_CONFIG) --cflags opencv4) \
            $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0)
LIBS     := $(shell $(PKG_CONFIG) --libs opencv4) \
            $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0)

SRC_DIR  := src
BUILD    := build
BIN      := bin
TARGET   := $(BIN)/vision_core

LIB_SRCS  := $(shell find $(SRC_DIR) -name '*.cpp')
ROOT_SRCS := $(wildcard *.cpp)
LIB_OBJS  := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD)/%.o, $(LIB_SRCS))
ROOT_OBJS := $(patsubst %.cpp, $(BUILD)/%.o, $(ROOT_SRCS))
OBJS      := $(LIB_OBJS) $(ROOT_OBJS)

.PHONY: all arm intel clean

all: intel

intel: $(TARGET)

arm:
	$(MAKE) $(TARGET) \
	    CXX=$(BUILDROOT_HOST)/bin/arm-buildroot-linux-gnueabihf-g++ \
	    PKG_CONFIG=$(BUILDROOT_HOST)/bin/pkg-config \
	    CXXFLAGS="$(CXXFLAGS_COMMON) $(CXXFLAGS_ARM)"

$(TARGET): $(OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(BUILD)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(BIN)
