CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -Isrc \
            $(shell pkg-config --cflags opencv4) \
            $(shell pkg-config --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0)
LIBS     := $(shell pkg-config --libs opencv4) \
            $(shell pkg-config --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0)

SRC_DIR  := src
BUILD    := build
BIN      := bin
TARGET   := $(BIN)/vision_core

SRCS := $(shell find $(SRC_DIR) -name '*.cpp')
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(BUILD)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(BIN)
