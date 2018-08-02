#!/bin/bash
set -euo pipefail
kernel_dir="$(readlink -e "$(dirname "$0")")"
rm -rf /mnt/tmpmp3
mkdir -p /mnt/tmpmp3
mount -o loop,offset=32256 "${kernel_dir}/mp3.img" /mnt/tmpmp3
cp -f "${kernel_dir}/bootimg" /mnt/tmpmp3/bootimg
cp -f "${kernel_dir}/filesys_img.new" /mnt/tmpmp3/filesys_img
umount /mnt/tmpmp3
rm -rf /mnt/tmpmp3
