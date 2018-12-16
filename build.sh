#!/bin/bash
set -euo pipefail
root_dir="$(readlink -e "$(dirname "$0")")"

# Don't close on error (useful if running as a shortcut)
# trap "read -p 'Press ENTER to continue...'" ERR

# Guard against sudo-happy users
if [ "$EUID" -eq 0 ]; then
    echo "Do not run this script as root!"
    exit 1
fi

# Parse flag options
nobuild="false"
compat="false"
debug="false"
while getopts ":dcn" opt; do
    case "$opt" in
    d)
        debug="true"
        ;;
    c)
        compat="true"
        ;;
    n)
        nobuild="true"
        ;;
    *)
        echo "Invalid option: -$OPTARG"
        exit 1
        ;;
    esac
done
shift $((OPTIND-1))

# If command is "clean", run make clean and git clean
if [ "$#" -gt 0 ] && [ "$1" = "clean" ]; then
    command -v git >/dev/null && git clean -fx "${root_dir}/filesystem"
    make -C "${root_dir}/userspace" clean
    make -C "${root_dir}/kernel" clean
    rm -f "${root_dir}/filesys_img.new"
    rm -f "${root_dir}/disk.img"
    exit 0
fi

# If debug mode is set, compile in -O0 with debug info
if [ "$debug" = "true" ]; then
    export CFLAGS="${CFLAGS-} -O0 -g"
else
    export CFLAGS="${CFLAGS-} -O2"
fi

if [ "$nobuild" != "true" ]; then
    if [ "$compat" = "true" ]; then
        # Copy filesys_img.new from original version
        cp "${root_dir}/filesys_img" "${root_dir}/filesys_img.new"
    else
        # Make binaries executable
        chmod +x "${root_dir}/elfconvert"
        chmod +x "${root_dir}/createfs.py"

        # Compile userspace programs
        make -C "${root_dir}/userspace"
        cp "${root_dir}/userspace/build/"* "${root_dir}/filesystem/"

        # Build filesystem image
        rm -f "${root_dir}/filesys_img.new"
        "${root_dir}/createfs.py" -i "${root_dir}/filesystem" -o "${root_dir}/filesys_img.new"
    fi

    # Build kernel executable
    make -C "${root_dir}/kernel"

    # Generate disk image
    cp "${root_dir}/orig.img" "${root_dir}/disk.img"
    sudo "${root_dir}/diskgen.sh"
fi

# If command is "run", boot the VM
if [ "$#" -gt 0 ] && [ "$1" = "run" ]; then
    qemu-system-i386 -hda "${root_dir}/disk.img" -m 256 \
        -gdb tcp:127.0.0.1:1234 \
        -soundhw sb16 \
        -device ne2k_isa,netdev=ne2k \
        -netdev user,id=ne2k,hostfwd=udp::4321-:4321,hostfwd=tcp::5432-:5432 \
        -object filter-dump,id=ne2k_filter,netdev=ne2k,file=/tmp/net.pcap
fi
