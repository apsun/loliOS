#!/bin/bash
set -e
mp3_dir=`dirname $0`

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

# Make binaries executable
chmod +x "${mp3_dir}/elfconvert"

# Compile userspace programs
mkdir -p "${mp3_dir}/userspace/build"
make -C "${mp3_dir}/userspace"
cp "${mp3_dir}/userspace/build/"* "${mp3_dir}/filesystem"

# Generate new filesystem image
rm -f "${mp3_dir}/kernel/filesys_img"
python "${mp3_dir}/createfs.py" -i "${mp3_dir}/filesystem" -o "${mp3_dir}/kernel/filesys_img"

# Build OS image
make -C "${mp3_dir}/kernel"
cd "${mp3_dir}/kernel"
sudo "./debug.sh"
cd "${mp3_dir}"
