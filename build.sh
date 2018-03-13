#!/bin/bash
set -e
mp3_dir=$(realpath "$(dirname $0)")

# Guard against sudo-happy users
if [ "$EUID" -eq 0 ]; then
    echo "Do not run this script as root!"
    exit 1
fi

# If first arg is "clean", run make clean
if [ "$1" == "clean" ]; then
    make -C "${mp3_dir}/userspace" clean
    make -C "${mp3_dir}/kernel" clean
    exit 0
fi

# Add extra flags if running outside the VM
if [ $(uname -r) != "2.6.22.5" ]; then
    export CFLAGS="-Wno-implicit-fallthrough"
    export LDFLAGS="--build-id=none"
fi

# Make binaries executable
chmod +x "${mp3_dir}/elfconvert"

# Compile userspace programs
mkdir -p "${mp3_dir}/userspace/build"
make -C "${mp3_dir}/userspace"
cp "${mp3_dir}/userspace/build/"* "${mp3_dir}/filesystem"

# Generate new filesystem image
rm -f "${mp3_dir}/kernel/filesys_img"
"${mp3_dir}/createfs.py" -i "${mp3_dir}/filesystem" -o "${mp3_dir}/kernel/filesys_img"

# Build OS image
make -C "${mp3_dir}/kernel"
(cd "${mp3_dir}/kernel" && sudo "./debug.sh")

# If first arg is "run", boot the VM
if [ "$1" == "run" ]; then
    qemu-system-i386 -hda "${mp3_dir}/kernel/mp3.img" -m 256 -name loliOS \
        -soundhw sb16 \
        -device ne2k_isa
fi
