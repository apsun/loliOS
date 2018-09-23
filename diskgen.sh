#!/bin/bash
set -euo pipefail
root_dir="$(readlink -e "$(dirname "$0")")"

if [ "$EUID" -ne 0 ]; then
    echo "This script must be run as root!"
    exit 1
fi

temp_dir="$(mktemp -d)"
trap 'rm -rf "${temp_dir}"' EXIT
mount -o loop,offset=32256 "${root_dir}/disk.img" "${temp_dir}"
trap 'umount "${temp_dir}" && rm -rf "${temp_dir}"' EXIT
cp -f "${root_dir}/kernel/bootimg" "${temp_dir}/bootimg"
cp -f "${root_dir}/filesys_img.new" "${temp_dir}/filesys_img"
sync
