#!/bin/bash
set -euo pipefail
root_dir="$(readlink -e "$(dirname "$0")")"

# Don't close on error (useful if running as a shortcut)
# trap "read -p 'Press ENTER to continue...'" ERR

# Guard against sudo-happy users
if [ "${EUID}" -eq 0 ]; then
    echo "Do not run this script as root!"
    exit 1
fi

# Parse flag options
compat=0
optimize=0
netdebug=0
while getopts ":con" opt; do
    case "${opt}" in
    c)
        compat=1
        ;;
    o)
        optimize=1
        ;;
    n)
        netdebug=1
        ;;
    *)
        echo "Invalid option: -${OPTARG}"
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

# If command is "debug", start GDB attached to QEMU
if [ "$#" -gt 0 ] && [ "$1" = "debug" ]; then
    if [ "$#" -gt 1 ]; then
        gdb -x "${root_dir}/debug.gdbinit" \
            -ex "add-symbol-file '${root_dir}/userspace/build/$2'" \
            "${root_dir}/kernel/bootimg"
    else
        gdb -x "${root_dir}/debug.gdbinit" "${root_dir}/kernel/bootimg"
    fi
    exit 0
fi

# Set compiler flags based on optimize mode
if [ "${optimize}" -eq 1 ]; then
    export CFLAGS="${CFLAGS-} -O2"
else
    export CFLAGS="${CFLAGS-} -Og -g -fsanitize=undefined -fsanitize-undefined-trap-on-error"
    export CPPFLAGS="${CPPFLAGS-} -DDEBUG_PRINT=1"
fi

# If compat mode is set, use the original filesystem image,
# otherwise build it from filesystem/ + userspace/
if [ "${compat}" -eq 1 ]; then
    cp "${root_dir}/filesys_img" "${root_dir}/filesys_img.new"
else
    # Make binaries executable
    chmod +x "${root_dir}/createfs.py"

    # Compile userspace programs
    make -j "$(nproc)" -C "${root_dir}/userspace"
    cp "${root_dir}/userspace/build/"* "${root_dir}/filesystem/"

    # Build filesystem image
    rm -f "${root_dir}/filesys_img.new"
    "${root_dir}/createfs.py" -i "${root_dir}/filesystem" -o "${root_dir}/filesys_img.new"
fi

# Build kernel executable
make -j "$(nproc)" -C "${root_dir}/kernel"

# Generate disk image
chmod +x "${root_dir}/diskgen.sh"
cp "${root_dir}/orig.img" "${root_dir}/disk.img"
sudo "${root_dir}/diskgen.sh"

# If netdebug is set, dump net traffic to /tmp/net.pcap
netfilter=()
if [ "${netdebug}" -eq 1 ]; then
    netfilter=("-object" "filter-dump,id=ne2k_filter,netdev=ne2k,file=/tmp/net.pcap")
fi

# If command is "run", boot the VM
if [ "$#" -gt 0 ] && [ "$1" = "run" ]; then
    qemu-system-i386 \
        -M isapc \
        -m 256 \
        -cpu qemu32 \
        -drive format=raw,file="${root_dir}/disk.img" \
        -gdb tcp:127.0.0.1:1234 \
        -device isa-vga \
        -device sb16 \
        -device ne2k_isa,netdev=ne2k \
        -netdev user,id=ne2k,hostfwd=udp::4321-:4321,hostfwd=tcp::5432-:5432 \
        "${netfilter[@]}"
fi
