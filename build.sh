#!/bin/bash
set -e
mp3_dir=`dirname $0`

# Guard against sudo-happy users
if [ "$EUID" -eq 0 ]; then
    echo "Do not run this script as root!"
    exit 1
fi

# Make binaries executable
chmod +x "${mp3_dir}/elfconvert"

# Compile userspace programs
mkdir -p "${mp3_dir}/syscalls/to_fsdir"
make -C "${mp3_dir}/syscalls"
cp "${mp3_dir}/syscalls/to_fsdir/"* "${mp3_dir}/fsdir"

# Generate new filesystem image
rm -f "${mp3_dir}/student-distrib/filesys_img"
python "${mp3_dir}/createfs.py" -i "${mp3_dir}/fsdir" -o "${mp3_dir}/student-distrib/filesys_img"

# Build OS image
make -C "${mp3_dir}/student-distrib"
cd "${mp3_dir}/student-distrib"
sudo "./debug.sh"
cd "${mp3_dir}"
