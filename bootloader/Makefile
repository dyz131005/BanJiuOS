CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -O2 -fno-stack-protector -fno-pie
LDFLAGS = -nostdlib -e efi_main

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
EFI_DIR = $(BUILD_DIR)/EFI/BOOT

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TARGET_X64 = $(EFI_DIR)/BOOTX64.EFI
TARGET_IA32 = $(EFI_DIR)/BOOTIA32.EFI

.PHONY: all clean install gnu-efi edk2 help

all: gnu-efi

gnu-efi:
	@echo "Building BanJiuOS Boot Loader with GNU-EFI..."
	@mkdir -p $(EFI_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/main.c $(SRC_DIR)/banjuefi.c -o $(TARGET_X64) $(LDFLAGS) -DEFI_PLATFORM_X64
	@echo "Built: $(TARGET_X64)"

install: all
	@echo "Installing to iso/EFI/BOOT..."
	@mkdir -p d:/Projects/BanJiuOS/iso/EFI/BOOT
	cp $(TARGET_X64) d:/Projects/BanJiuOS/iso/EFI/BOOT/
	cp $(TARGET_IA32) d:/Projects/BanJiuOS/iso/EFI/BOOT/ 2>/dev/null || true
	@echo "Install complete."

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "BanJiuOS Boot Loader Build System"
	@echo "=================================="
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build with GNU-EFI (default)"
	@echo "  gnu-efi   - Build using GNU-EFI library"
	@echo "  edk2     - Build using EDK2"
	@echo "  install   - Install to iso/EFI/BOOT/"
	@echo "  clean     - Remove build artifacts"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  GNU-EFI:   https://sourceforge.net/projects/gnu-efi/"
	@echo "  EDK2:      https://github.com/tianocore/edk2"
	@echo ""
	@echo "For GNU-EFI, ensure gnu-efi library is installed and set GNUEFI_PATH"
