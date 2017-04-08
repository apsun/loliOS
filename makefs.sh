#!/bin/bash
set -e
mp3_dir=`dirname $0`

# Guard against sudo-happy users
if [ "$EUID" -eq 0 ]; then
    echo "Do not run this script as root!"
    exit 1
fi

# Compile userspace programs
make -C "${mp3_dir}/syscalls"
cp "${mp3_dir}/syscalls/to_fsdir/"* "${mp3_dir}/fsdir"

# Compile fish program
make -C "${mp3_dir}/fish"
cp "${mp3_dir}/fish/fish" "${mp3_dir}/fsdir"

# Copy fsdir to tmp and create the rtc file
rm -rf "/tmp/fsdir"
cp -r "${mp3_dir}/fsdir" "/tmp/fsdir"
sudo mknod "/tmp/fsdir/rtc" c 10 61

# Generate new filesystem image
rm -f "${mp3_dir}/student-distrib/filesys_img"
"${mp3_dir}/createfs" "/tmp/fsdir" -o "${mp3_dir}/student-distrib/filesys_img"

# Build OS image
cd "${mp3_dir}/student-distrib"
sudo "./debug.sh"
