# Cross-compilation setup:
#   export ARCH=arm
#   export CROSS_COMPILE=arm-none-linux-gnueabihf-

ifndef CROSS_COMPILE
$(error CROSS_COMPILE is not set. Example: export CROSS_COMPILE=arm-none-linux-gnueabihf-)
endif

CC      := $(CROSS_COMPILE)gcc
CFLAGS  := -Wall -Wextra -O2
TARGET  := bin/hexip_daemon
SRC     := src/main.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC) | bin build
	$(CC) $(CFLAGS) -o $@ $<

bin build:
	mkdir -p $@

clean:
	rm -f $(TARGET) build/*
