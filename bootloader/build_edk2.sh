#!/bin/bash

#
# BanJiuOS Boot Loader Build Script (EDK II Version)
#
# This script builds the boot loader using EDK II.
#
# Copyright (c) 2024 BanJiuOS Project. All rights reserved.
#

set -e

EDK2_PATH="${EDK2_PATH:-/path/to/edk2}"
BANJUOS_PATH="$(cd "$(dirname "$0")" && pwd)"
BUILD_TARGET="${BUILD_TARGET:-RELEASE}"
BUILD_ARCH="${BUILD_ARCH:-X64}"

echo "BanJiuOS Boot Loader Build Script (EDK II)"
echo "============================================"
echo ""
echo "EDK2 Path:     $EDK2_PATH"
echo "BanJiuOS Path: $BANJUOS_PATH"
echo "Build Target:  $BUILD_TARGET"
echo "Build Arch:    $BUILD_ARCH"
echo ""

if [ ! -d "$EDK2_PATH" ]; then
    echo "Error: EDK II not found at $EDK2_PATH"
    echo ""
    echo "Please set EDK2_PATH environment variable:"
    echo "  export EDK2_PATH=/path/to/edk2"
    echo ""
    echo "Or install EDK II:"
    echo "  git clone https://github.com/tianocore/edk2.git"
    echo "  cd edk2"
    echo "  . edksetup.sh"
    echo "  make -C BaseTools"
    echo ""
    exit 1
fi

cd "$EDK2_PATH"

if [ ! -f "edksetup.sh" ]; then
    echo "Error: edksetup.sh not found"
    exit 1
fi

source edksetup.sh

if [ ! -d "BaseTools" ]; then
    echo "Building BaseTools..."
    make -C BaseTools
fi

echo ""
echo "Copying BanJiuOS module to EDK II..."
mkdir -p "$EDK2_PATH/BanJiuOS"
cp "$BANJUOS_PATH/BanJiuBootLoader.inf" "$EDK2_PATH/BanJiuOS/"
cp "$BANJUOS_PATH/BanJiuOS.dsc" "$EDK2_PATH/BanJiuOS/"
cp "$BANJUOS_PATH/src/BootLoader.c" "$EDK2_PATH/BanJiuOS/"

echo ""
echo "Building BanJiuOS Boot Loader..."
build -p BanJiuOS/BanJiuOS.dsc -a $BUILD_ARCH -t $BUILD_TARGET

echo ""
echo "Copying output to iso/EFI/BOOT..."
mkdir -p "$BANJUOS_PATH/../iso/EFI/BOOT"
cp "$EDK2_PATH/Build/BanJiuOS/$BUILD_TARGET_$BUILD_ARCH/BanJiuBootLoader.efi" \
   "$BANJUOS_PATH/../iso/EFI/BOOT/BOOTX64.EFI"

echo ""
echo "Build complete!"
echo "Output: $BANJUOS_PATH/../iso/EFI/BOOT/BOOTX64.EFI"