#!/bin/bash
set -euo pipefail

port=8989
width=1024
height=576
pix_fmt=rgb565

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <input>"
    exit 1
fi

trap "exit 0" INT
while :; do
    ffmpeg \
        -i "$1" \
        -map 0:v:0 \
        -vf scale="${width}x${height}" \
        -c:v rawvideo \
        -pix_fmt "${pix_fmt}" \
        -f rawvideo \
        - \
    | nc -lp "${port}" || true
done
