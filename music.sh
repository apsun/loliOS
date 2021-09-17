#!/bin/bash
set -euo pipefail

port=7878
sample_rate=44100
sample_fmt=pcm_s16le
num_channels=2

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <input>"
    exit 1
fi

wavfile="$(mktemp -u)"
ffmpeg \
    -i "$1" \
    -ar "${sample_rate}" \
    -ac "${num_channels}" \
    -c:a "${sample_fmt}" \
    -f wav \
    "${wavfile}"
trap 'rm -f "${wavfile}"' EXIT

trap "exit 0" INT
while :; do
    nc -lp "${port}" < "${wavfile}" || true
done
