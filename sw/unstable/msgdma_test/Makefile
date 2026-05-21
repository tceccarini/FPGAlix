# SPDX-License-Identifier: GPL-2.0
# Standalone msgdma test module build

ARCH          := arm
CROSS_COMPILE := arm-none-linux-gnueabihf-
KDIR          := $(CURDIR)/../../repos/linux-socfpga

SRC_DIR   := $(CURDIR)/src
BUILD_DIR := $(CURDIR)/build
BIN_DIR   := $(CURDIR)/bin
MODULE    := fpgalix_msgdma_test
DRIVER_SRC := $(CURDIR)/../camera_driver/src

C_SRCS := msgdma_test.c msgdma.c
H_SRCS := msgdma.h camera.h config.h

BUILD_C  := $(addprefix $(BUILD_DIR)/, $(C_SRCS))
BUILD_H  := $(addprefix $(BUILD_DIR)/, $(H_SRCS))
BUILD_KB := $(BUILD_DIR)/Kbuild

.PHONY: all clean

all: $(BIN_DIR)/$(MODULE).ko

$(BIN_DIR)/$(MODULE).ko: $(BUILD_DIR)/$(MODULE).ko | $(BIN_DIR)
	cp $< $@

$(BUILD_DIR)/$(MODULE).ko: $(BUILD_KB) $(BUILD_C) $(BUILD_H)
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) \
	    ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

$(BUILD_KB): | $(BUILD_DIR)
	@printf 'obj-m          := $(MODULE).o\n$(MODULE)-y := msgdma_test.o msgdma.o\n' > $@

$(BUILD_DIR)/msgdma_test.c: $(SRC_DIR)/msgdma_test.c | $(BUILD_DIR)
	@ln -sf $< $@

$(BUILD_DIR)/msgdma.c: $(DRIVER_SRC)/msgdma.c | $(BUILD_DIR)
	@ln -sf $< $@

$(BUILD_DIR)/msgdma.h: $(DRIVER_SRC)/msgdma.h | $(BUILD_DIR)
	@ln -sf $< $@

$(BUILD_DIR)/camera.h: $(DRIVER_SRC)/camera.h | $(BUILD_DIR)
	@ln -sf $< $@

$(BUILD_DIR)/config.h: $(DRIVER_SRC)/config.h | $(BUILD_DIR)
	@ln -sf $< $@

$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@

clean:
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) \
	    ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -rf $(BUILD_DIR) $(BIN_DIR)
