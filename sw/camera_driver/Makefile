# SPDX-License-Identifier: GPL-2.0
# Builds fpgalix_camera.ko (V4L2 + Altera mSGDMA driver) for Cyclone V SoC.
#
# Layout:
#   src/   — C sources and headers (never touched by the build)
#   build/ — intermediate artifacts (.o, .mod, Kbuild, symlinks)
#   bin/   — final fpgalix_camera.ko

ARCH          := arm
CROSS_COMPILE := arm-none-linux-gnueabihf-
KDIR          := $(CURDIR)/../../repos/linux-socfpga

SRC_DIR   := $(CURDIR)/src
BUILD_DIR := $(CURDIR)/build
BIN_DIR   := $(CURDIR)/bin
MODULE    := fpgalix_camera

C_SRCS := camera.c msgdma.c sensor.c
H_SRCS := camera.h msgdma.h sensor.h config.h

BUILD_C  := $(addprefix $(BUILD_DIR)/, $(C_SRCS))
BUILD_H  := $(addprefix $(BUILD_DIR)/, $(H_SRCS))
BUILD_KB := $(BUILD_DIR)/Kbuild

.PHONY: all clean

all: $(BIN_DIR)/$(MODULE).ko

# Copy the finished module to bin/
$(BIN_DIR)/$(MODULE).ko: $(BUILD_DIR)/$(MODULE).ko | $(BIN_DIR)
	cp $< $@

# Run the kernel build
$(BUILD_DIR)/$(MODULE).ko: $(BUILD_KB) $(BUILD_C) $(BUILD_H)
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) \
	    ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

# Generate Kbuild in build/ — lists the objects that form the module
$(BUILD_KB): | $(BUILD_DIR)
	@printf 'obj-m          := $(MODULE).o\n$(MODULE)-y := camera.o msgdma.o sensor.o\n' > $@

# Symlinks in build/ pointing to the real source files in src/
# The kernel build follows them transparently.
$(BUILD_DIR)/%.c: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@ln -sf $< $@

$(BUILD_DIR)/%.h: $(SRC_DIR)/%.h | $(BUILD_DIR)
	@ln -sf $< $@

$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@

clean:
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) \
	    ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -rf $(BUILD_DIR) $(BIN_DIR)
