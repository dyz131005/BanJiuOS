KERNEL_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
OUTPUT_DIR := $(KERNEL_DIR)output
GCC_PATH := D:\program\cpp\mingw64\bin

CFLAGS := -ffreestanding -fno-pic -m64 -mno-red-zone -fno-stack-protector -Wall -Wextra
LDFLAGS := -T kernel.ld -ffreestanding -nostdlib -lgcc

SRCS := \
    src/core/main.c \
    src/core/string.c \
    src/core/font.c \
    src/core/framebuffer.c \
    src/core/shell.c \
    src/mm/pmm.c \
    src/mm/vmm.c \
    src/mm/kmalloc.c \
    src/proc/process.c \
    src/fs/fs.c \
    src/fs/ext4.c

OBJS := $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(SRCS))

all: kernel.elf

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR)/src/core
	mkdir -p $(OUTPUT_DIR)/src/mm
	mkdir -p $(OUTPUT_DIR)/src/proc
	mkdir -p $(OUTPUT_DIR)/src/fs

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	$(GCC_PATH)/gcc.exe $(CFLAGS) -I $(KERNEL_DIR)include -c $< -o $@

kernel.elf: $(OBJS)
	$(GCC_PATH)/ld.exe $(LDFLAGS) -o $(OUTPUT_DIR)/kernel.elf $(OBJS)

clean:
	rm -rf $(OUTPUT_DIR)

.PHONY: all clean