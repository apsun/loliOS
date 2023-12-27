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
debug=0
lto=0
optlevel=
netdebug=0
rebuild=0
ubsan=0
while getopts ":cglO:nru" opt; do
    case "${opt}" in
    c)
        compat=1
        ;;
    g)
        debug=1
        ;;
    l)
        lto=1
        ;;
    O)
        optlevel="${OPTARG}"
        ;;
    n)
        netdebug=1
        ;;
    r)
        rebuild=1
        ;;
    u)
        ubsan=1
        ;;
    *)
        echo "Invalid option: -${OPTARG}"
        exit 1
        ;;
    esac
done
shift $((OPTIND-1))

# If command is "clean" or -r flag is passed, run make clean and git clean
if [ "${rebuild}" -eq 1 ] || { [ "$#" -gt 0 ] && [ "$1" = "clean" ]; }; then
    command -v git >/dev/null && git clean -fx "${root_dir}/filesystem"
    make -C "${root_dir}/userspace" clean
    make -C "${root_dir}/kernel" clean
    rm -f "${root_dir}/filesys_img.new"
    rm -f "${root_dir}/disk.img"
    if [ "$#" -gt 0 ] && [ "$1" = "clean" ]; then
        exit 0
    fi
fi

# If command is "debug", start GDB attached to QEMU
if [ "$#" -gt 0 ] && [ "$1" = "debug" ]; then
    if [ "$#" -gt 1 ]; then
        gdb \
            -ex "target remote 127.0.0.1:1234" \
            -ex "add-symbol-file '${root_dir}/userspace/build/$2'" \
            "${root_dir}/kernel/bootimg"
    else
        gdb \
            -ex "target remote 127.0.0.1:1234" \
            "${root_dir}/kernel/bootimg"
    fi
    exit 0
fi

if [ -n "${optlevel}" ]; then
    export CFLAGS="${CFLAGS-} -O${optlevel}"
fi

if [ "${debug}" -eq 1 ]; then
    export CFLAGS="${CFLAGS-} -g"
    export CPPFLAGS="${CPPFLAGS-} -DDEBUG_PRINT=1"
fi

if [ "${ubsan}" -eq 1 ]; then
    export CFLAGS="${CFLAGS-} -fsanitize=undefined"
    export CPPFLAGS="${CPPFLAGS-} -DUBSAN_ENABLED=1 -DMYA_POISON=1"
fi

if [ "${lto}" -eq 1 ]; then
    export CFLAGS="${CFLAGS-} -flto=auto"
fi

# If compat mode is set, use the original filesystem image,
# otherwise build it from filesystem/ + userspace/
if [ "${compat}" -eq 1 ]; then
    cp "${root_dir}/filesys_img" "${root_dir}/filesys_img.new"
else
    # Compile userspace programs
    make -j "$(nproc)" -C "${root_dir}/userspace"
    cp "${root_dir}/userspace/build/"* "${root_dir}/filesystem/"

    # Build filesystem image
    rm -f "${root_dir}/filesys_img.new"
    python3 "${root_dir}/createfs.py" -i "${root_dir}/filesystem" -o "${root_dir}/filesys_img.new"
fi

# Build kernel executable
make -j "$(nproc)" -C "${root_dir}/kernel"

# Generate disk image (mount needs to be run as root)
cp "${root_dir}/orig.img" "${root_dir}/disk.img"
temp_dir="$(mktemp -d)"
trap 'rmdir "${temp_dir}"' EXIT
sudo -s <<EOF
mount -o loop,offset=32256 "${root_dir}/disk.img" "${temp_dir}"
trap 'umount "${temp_dir}"' EXIT
cp -f "${root_dir}/kernel/bootimg" "${temp_dir}/bootimg"
cp -f "${root_dir}/filesys_img.new" "${temp_dir}/filesys_img"
EOF

# If netdebug is set, dump net traffic to /tmp/net.pcap
netfilter=()
if [ "${netdebug}" -eq 1 ]; then
    netfilter=("-object" "filter-dump,id=ne2k_filter,netdev=ne2k,file=/tmp/net.pcap")
fi

# If command is "run", boot the VM
if [ "$#" -gt 0 ] && [ "$1" = "run" ]; then
    qemu-system-i386 \
        -m 256 \
        -drive format=raw,file="${root_dir}/disk.img" \
        -gdb tcp:127.0.0.1:1234 \
        -serial stdio \
        -device isa-vga \
        -device sb16 \
        -device ne2k_isa,netdev=ne2k \
        -netdev user,id=ne2k,hostfwd=udp::4321-:4321,hostfwd=tcp::5432-:5432 \
        "${netfilter[@]}"
fi
