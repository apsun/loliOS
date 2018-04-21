#!/bin/bash
set -e
mp3_dir=$(readlink -e "$(dirname "$0")")

# Don't close on error (useful if running as a shortcut)
# trap "read -p 'Press ENTER to continue...'" ERR

# Guard against sudo-happy users
if [ "$EUID" -eq 0 ]; then
    echo "Do not run this script as root!"
    exit 1
fi

# Add extra flags if running outside the VM
if [ "$(uname -r)" != "2.6.22.5" ]; then
    export CFLAGS="-Wno-implicit-fallthrough"
    export LDFLAGS="--build-id=none"
fi

# Read compat flag (used to boot the original filesys_img)
compat="false"
while getopts ":c" opt; do
    case "$opt" in
    c)
        compat="true"
        ;;
    *)
        echo "Invalid option: -$OPTARG"
        exit 1
        ;;
    esac
done
shift $((OPTIND-1))

# If command is "clean", run make clean and git clean
if [ "$1" = "clean" ]; then
    command -v git >/dev/null && git clean -fx "${mp3_dir}/filesystem"
    make -C "${mp3_dir}/userspace" clean
    make -C "${mp3_dir}/kernel" clean
    exit 0
fi

if [ "$compat" = "true" ]; then
    # Copy filesys_img.new from original version
    cp "${mp3_dir}/kernel/filesys_img" "${mp3_dir}/kernel/filesys_img.new"
else
    # Make binaries executable
    chmod +x "${mp3_dir}/elfconvert"

    # Compile userspace programs
    mkdir -p "${mp3_dir}/userspace/build"
    make -C "${mp3_dir}/userspace"
    cp "${mp3_dir}/userspace/build/"* "${mp3_dir}/filesystem"

    # Generate new filesystem image
    rm -f "${mp3_dir}/kernel/filesys_img.new"
    "${mp3_dir}/createfs.py" -i "${mp3_dir}/filesystem" -o "${mp3_dir}/kernel/filesys_img.new"
fi

# Build OS image
make -C "${mp3_dir}/kernel"
(cd "${mp3_dir}/kernel" && sudo "./debug.sh")

# If command is "run", boot the VM
if [ "$1" = "run" ]; then
    qemu-system-i386 -hda "${mp3_dir}/kernel/mp3.img" -m 256 \
        -gdb tcp:127.0.0.1:1234 \
        -soundhw sb16 \
        -net nic,model=ne2k_isa \
        -net user,hostfwd=udp::4321-:4321
fi
