#!/usr/bin/env python3
#
# ELVI: Extremely Lightweight Video Interleaved
#
# ELVI is a very naive container format for audio and video, designed
# to be as close as possible to a raw stream of pixels and audio
# samples. It's basically AVI, but with all the portability features
# left out and the interleave period set to one frame. The file format
# is as follows:
#
# header {
#   <magic>
#   <video width>
#   <video height>
#   <video bits per pixel>
#   <audio sample rate>
#   <audio channel count>
#   <audio bits per sample>
#   <max audio size>
# }
# repeat {
#   <video data>
#   <audio size>
#   <audio data>
# }
#
# Each body chunk contains the pixel data for one video frame,
# followed by the size in bytes of the audio samples corresponding
# to that video frame, and then finally the audio sample data.
# The number of audio samples may vary from frame to frame, because
# there can be a non-integer number of audio samples for each video
# frame. All fields other than the pixel/sample data are 32-bit ints,
# little endian.

import argparse
import fractions
import math
import struct
import subprocess
import sys

ELVI_MAGIC = 0x49564c45

VIDEO_FORMATS = {
	16: "rgb565",
	24: "bgr24",
	32: "rgb32",
}

AUDIO_FORMATS = {
	8: "u8",
	16: "s16le",
}

parser = argparse.ArgumentParser()
parser.add_argument("--video-width", type=int, required=True)
parser.add_argument("--video-height", type=int, required=True)
parser.add_argument("--video-bits-per-pixel", type=int, required=True)
parser.add_argument("--audio-sample-rate", type=int, required=True)
parser.add_argument("--audio-channel-count", type=int, required=True)
parser.add_argument("--audio-bits-per-sample", type=int, required=True)
parser.add_argument("file")
args = parser.parse_args()

input_path = args.file
output_file = sys.stdout.buffer

video_bytes_per_pixel = (args.video_bits_per_pixel + 1) // 8
video_frame_size = args.video_width * args.video_height * video_bytes_per_pixel
audio_bytes_per_sample = args.audio_channel_count * (args.audio_bits_per_sample // 8)

frame_rate = fractions.Fraction(subprocess.run([
	"ffprobe",
	"-v", "fatal",
	"-i", input_path,
	"-select_streams", "v:0",
	"-print_format", "default=noprint_wrappers=1:nokey=1",
	"-show_entries", f"stream=r_frame_rate",
], stdout=subprocess.PIPE, check=True, text=True).stdout)
audio_samples_per_frame = args.audio_sample_rate / frame_rate

video_stream = subprocess.Popen([
	"ffmpeg",
	"-v", "fatal",
	"-nostdin",
	"-i", input_path,
	"-map", "0:v:0",
	"-vf", f"scale={args.video_width}x{args.video_height}",
	"-pix_fmt", f"+{VIDEO_FORMATS[args.video_bits_per_pixel]}",
	"-c", "rawvideo",
	"-f", "rawvideo",
	"-",
], stdout=subprocess.PIPE)

audio_stream = subprocess.Popen([
	"ffmpeg",
	"-v", "fatal",
	"-nostdin",
	"-i", input_path,
	"-map", "0:a:0",
	"-ar", f"{args.audio_sample_rate}",
	"-ac", f"{args.audio_channel_count}",
	"-c", f"pcm_{AUDIO_FORMATS[args.audio_bits_per_sample]}",
	"-f", f"{AUDIO_FORMATS[args.audio_bits_per_sample]}",
	"-",
], stdout=subprocess.PIPE)

output_file.write(
	struct.pack(
		"<IIIIIIII",
		ELVI_MAGIC,
		args.video_width,
		args.video_height,
		args.video_bits_per_pixel,
		args.audio_sample_rate,
		args.audio_channel_count,
		args.audio_bits_per_sample,
		math.ceil(audio_samples_per_frame) * audio_bytes_per_sample
	)
)

video_buf = bytearray(video_frame_size)
audio_written = 0
audio_written_partial = fractions.Fraction(0)
while video_stream.stdout.readinto(video_buf):
	output_file.write(video_buf)

	audio_written_partial += audio_samples_per_frame
	audio_samples = int(audio_written_partial) - audio_written
	audio_written += audio_samples
	audio_size = audio_samples * audio_bytes_per_sample
	output_file.write(struct.pack("<I", audio_size))

	audio_buf = audio_stream.stdout.read(audio_size)
	output_file.write(audio_buf)

video_stream.wait()
audio_stream.terminate()
audio_stream.wait()
