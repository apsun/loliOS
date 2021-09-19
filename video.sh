#!/bin/bash
set -euo pipefail
root_dir="$(readlink -e "$(dirname "$0")")"

port=8989
video_width=1024
video_height=576
video_bits_per_pixel=16
audio_sample_rate=44100
audio_channel_count=2
audio_bits_per_sample=16

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <input>"
    exit 1
fi

trap "exit 0" INT
while :; do
    "${root_dir}/elvi.py" \
        --video-width "${video_width}" \
        --video-height "${video_height}" \
        --video-bits-per-pixel "${video_bits_per_pixel}" \
        --audio-sample-rate "${audio_sample_rate}" \
        --audio-channel-count "${audio_channel_count}" \
        --audio-bits-per-sample "${audio_bits_per_sample}" \
        "$1" \
    | nc -Nlp "${port}" || true
done
