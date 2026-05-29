#!/bin/bash

set -e

CMAKE_CONFIG=Debug

export CC=cc
export CXX=c++
GENERATOR=Ninja
BUILD_DIR=build

KERNEL_BIN=kernel

LIMINE_REPO="https://github.com/Limine-Bootloader/Limine.git"
LIMINE_BRANCH="v11.x-binary"
LIMINE_DIR_NAME=limine

ISO_DIR=iso_root
ISO_NAME=littleos

VM_NAME=LittleOS_VM
QEMU_VGA=qxl
QEMU_DISPLAY=sdl

ENABLE_KVM=true

is() {
  command -v "$1" >/dev/null 2>&1
}

dirmkcheck() {
  if [ ! -d "$1" ]; then
    echo "-> Membuat direktori: $1"
    mkdir -p "$1"
  fi
}

timescmp() {
  local src=$1
  local dest=$2

  [ "$src" -nt "$dest" ]
}

recopy() {
  local src="$1"
  local dest="$2"

  if [ ! -f "$src" ]; then
    echo "-> Error: File sumber $src tidak ditemukan."
    return 1
  fi

  # Logika: Jika dest tidak ada ATAU src lebih baru dari dest, maka copy
  if [ ! -f "$dest" ] || timescmp "$src" "$dest"; then
    echo "-> Updating: $dest"
    cp "$src" "$dest"
  fi
}

task_copy_if_missing() {
  local src=$1
  local dest=$2
  if [ ! -f "$dest" ]; then
    echo "-> [Task] Copying missing file: $src"
    cp "$src" "$dest"
  fi
}

task_move_if_missing() {
  local src=$1
  local dest=$2
  if [ ! -f "$dest" ]; then
    echo "-> [Task] Copying missing file: $src"
    mv "$src" "$dest"
  fi
}

# Configure d cmake
if is cmake && [ ! -f $BUILD_DIR/build.ninja ]; then
  cmake -B $BUILD_DIR -S . -G "$GENERATOR"
fi

if [ ! -d "$LIMINE_DIR_NAME" ]; then
  read -rp "Folder '$LIMINE_DIR_NAME' belum ditemukan. Download sekarang? [Y/n]: " choice

  choice=${choice:-y}

  if [[ "$choice" =~ ^[Yy]$ ]]; then
    echo "-> Mengunduh Limine..."
    git clone -b $LIMINE_BRANCH --single-branch $LIMINE_REPO $LIMINE_DIR_NAME
  else
    echo "-> Download dibatalkan. Silakan cek koneksi internet."
    exit 1
  fi
fi

dirmkcheck $ISO_DIR/EFI/BOOT
dirmkcheck $ISO_DIR/boot/limine

task_copy_if_missing "./$LIMINE_DIR_NAME/BOOTX64.EFI" "./$ISO_DIR/EFI/BOOT"
task_copy_if_missing "./$LIMINE_DIR_NAME/BOOTIA32.EFI" "./$ISO_DIR/EFI/BOOT"
task_copy_if_missing "./$LIMINE_DIR_NAME/limine-uefi-cd.bin" "./$ISO_DIR/boot/limine"
task_copy_if_missing "./$LIMINE_DIR_NAME/limine-bios-cd.bin" "./$ISO_DIR/boot/limine"
task_copy_if_missing "./$LIMINE_DIR_NAME/limine-bios.sys" "./$ISO_DIR/boot/limine"

recopy limine.conf "$ISO_DIR/boot/limine"

# BUILD KERNEL + ISO
cmake --build $BUILD_DIR --config "$CMAKE_CONFIG"

task_move_if_missing $BUILD_DIR/${KERNEL_BIN} $ISO_DIR/boot/

xorriso -as mkisofs -R -r -J \
  -b boot/limine/limine-bios-cd.bin \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  -hfsplus -apm-block-size 2048 \
  --efi-boot boot/limine/limine-uefi-cd.bin \
  -efi-boot-part --efi-boot-image \
  --protective-msdos-label \
  $ISO_DIR -o "${ISO_NAME}.iso"

make -C $LIMINE_DIR_NAME/

./${LIMINE_DIR_NAME}/limine bios-install "${ISO_NAME}.iso"

# PRIORITY: QEMU
KVM=
if $ENABLE_KVM; then
  [ -e /dev/kvm ] && KVM="-enable-kvm"
fi

if is qemu-system-x86_64; then
  echo "-> QEMU detected, launching..."

  qemu-system-x86_64 \
    -machine q35 \
    -m 512M \
    -cdrom "${ISO_NAME}.iso" \
    -vga "$QEMU_VGA" \
    -display "$QEMU_DISPLAY" \
    $KVM

  exit 0
fi

# FALLBACK: VirtualBox
if is vboxmanage; then
  echo "-> VirtualBox detected, launching..."

  if ! vboxmanage list vms | grep -q "\"$VM_NAME\""; then
    echo "-> VM '$VM_NAME' not found, creating..."
    vboxmanage createvm --name "$VM_NAME" --ostype "Other_64" --register
    vboxmanage modifyvm "$VM_NAME" --memory 256 --vram 128 --cpus 1 --chipset piix3

    VBoxManage setextradata "LittleOS_VM" GUI/Scale on

    vboxmanage storagectl "$VM_NAME" --name "IDE Controller" --add ide
    vboxmanage modifyvm "$VM_NAME" --nic1 none
  fi

  vboxmanage controlvm "$VM_NAME" poweroff &>/dev/null || true
  vboxmanage storageattach "$VM_NAME" --storagectl "IDE Controller" --port 0 --device 0 --type dvddrive --medium "${ISO_NAME}.iso"

  vboxmanage startvm "$VM_NAME"
  exit 0
fi

# ERROR: NONE FOUND
echo "ERROR: Neither QEMU nor VirtualBox is installed!"
exit 1
