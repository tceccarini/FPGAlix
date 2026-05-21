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

INSTALL_DIR := /opt/FPGAlix/bin
SYSTEMD_DIR := /etc/systemd/system

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC) | bin build
	$(CC) $(CFLAGS) -o $@ $<

bin build:
	mkdir -p $@

install: $(TARGET)
	install -d $(INSTALL_DIR)
	install -m 755 $(TARGET) $(INSTALL_DIR)/hexip_daemon
	install -m 644 systemd/hexip_daemon.service $(SYSTEMD_DIR)/hexip_daemon.service
	systemctl daemon-reload
	systemctl enable hexip_daemon
	systemctl start hexip_daemon

clean:
	rm -f $(TARGET) build/*
