#!/bin/bash

# BanJiuOS ISO Build Script

set -e

ISO_DIR="d:/Projects/BanJiuOS/iso"
OUTPUT_DIR="d:/Projects/BanJiuOS/output"
ISO_NAME="BanJiuOS-1.0.0-x86_64.iso"
BUILD_BOOTLOADER="yes"
BUILD_INSTALLER="yes"

echo "BanJiuOS ISO Build System"
echo "=========================="
echo ""

if [ "$BUILD_BOOTLOADER" = "yes" ]; then
    echo "[1/4] Building bootloader..."
    cd d:/Projects/BanJiuOS/bootloader
    make clean 2>/dev/null || true
    make gnu-efi
    make install
    cd ../..
fi

if [ "$BUILD_INSTALLER" = "yes" ]; then
    echo "[2/4] Building installer..."
    cd d:/Projects/BanJiuOS/bootloader/Installer
    make clean 2>/dev/null || true
    make
    make install
    cd ../../..
fi

echo "[3/4] Creating ISO structure..."

mkdir -p "$ISO_DIR/EFI/BOOT/Fonts"
mkdir -p "$ISO_DIR/Boot/themes"
mkdir -p "$ISO_DIR/System"
mkdir -p "$ISO_DIR/Installer"
mkdir -p "$OUTPUT_DIR"

if [ ! -f "$ISO_DIR/EFI/BOOT/BOOTX64.EFI" ]; then
    echo "Warning: BOOTX64.EFI not found, creating placeholder..."
    echo "PLACEHOLDER_BOOTX64_EFI" > "$ISO_DIR/EFI/BOOT/BOOTX64.EFI"
fi

if [ ! -f "$ISO_DIR/System/kernel.zyxt" ]; then
    echo "Warning: kernel.zyxt not found, creating placeholder..."
    echo "PLACEHOLDER_KERNEL" > "$ISO_DIR/System/kernel.zyxt"
fi

if [ ! -f "$ISO_DIR/System/initramfs.img" ]; then
    echo "Warning: initramfs.img not found, creating placeholder..."
    echo "PLACEHOLDER_INITRAMFS" > "$ISO_DIR/System/initramfs.img"
fi

echo "[4/4] Building ISO image..."

if command -v xorriso &> /dev/null; then
    xorriso -as mkisofs \
        -R -f -pad \
        -no-emul-boot \
        -boot-load-size 4 \
        -append_partition 2 0xEF "$ISO_DIR/EFI" \
        -b Boot/grub/i386-pc/boot.img \
        -no-emul-boot \
        -boot-load-size 4 \
        -output "$OUTPUT_DIR/$ISO_NAME" \
        "$ISO_DIR"

    echo "ISO created: $OUTPUT_DIR/$ISO_NAME"
elif command -v genisoimage &> /dev/null; then
    genisoimage -o "$OUTPUT_DIR/$ISO_NAME" \
        -b Boot/grub/i386-pc/boot.img \
        -no-emul-boot \
        -boot-load-size 4 \
        -eltorito-alt-boot \
        -e EFI/BOOT/BOOTX64.EFI \
        -no-emul-boot \
        -R -J -V "BanJiuOS" \
        "$ISO_DIR"

    echo "ISO created: $OUTPUT_DIR/$ISO_NAME"
else
    echo "Warning: No ISO creation tool found (xorriso or genisoimage)"
    echo "ISO contents are ready in: $ISO_DIR"
    echo ""
    echo "To create ISO manually, install xorriso or genisoimage and run:"
    echo "  xorriso -as mkisofs -R -f -pad -no-emul-boot -boot-load-size 4 \\
         -b Boot/grub/i386-pc/boot.img -no-emul-boot \\
         -eltorito-alt-boot -e EFI/BOOT/BOOTX64.EFI -no-emul-boot \\
         -o $OUTPUT_DIR/$ISO_NAME $ISO_DIR"
fi

echo ""
echo "Build complete!"
echo "ISO file: $OUTPUT_DIR/$ISO_NAME"
