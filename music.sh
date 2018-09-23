#!/bin/bash
set -euo pipefail
port=7878
if [ "$#" -ne 1 ]; then
    echo "usage: $0 input.mp3"
    exit 1
fi
mp3file="$1"

# Convert audio to WAV
wavfile="$(mktemp -u)"
ffmpeg -i "${mp3file}" -f wav "${wavfile}"
trap 'rm -f "${wavfile}"' EXIT

# Serve music using netcat
echo "Serving '${mp3file}' at port ${port}..."
trap "exit 0" INT
while :; do
    nc -lp "${port}" < "${wavfile}" || true
done
