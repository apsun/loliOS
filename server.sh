#!/bin/bash
set -e
port=7878
mp3file="$1"
if [ "$mp3file" == "" ]; then
    echo "usage: $0 input.mp3"
    exit 1
fi

# Convert audio to WAV
wavfile="$(mktemp -u)"
trap "rm -f \"$wavfile\"" EXIT
ffmpeg -i "$mp3file" -f wav "$wavfile"

# Serve music using netcat
echo "Serving '$mp3file' at port $port..."
trap "exit 0" INT
while :; do
    nc -lp "$port" < "$wavfile" || true
done
