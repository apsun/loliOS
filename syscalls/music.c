#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define RIFF_MAGIC 0x46464952
#define WAVE_MAGIC 0x45564157
#define FMT_MAGIC 0x20746d66
#define DATA_MAGIC 0x61746164

#define SOUND_SET_BITS_PER_SAMPLE 1
#define SOUND_SET_NUM_CHANNELS 2
#define SOUND_SET_SAMPLE_RATE 3

typedef struct {
    uint32_t riff_magic;
    uint32_t chunk_size;
    uint32_t wave_magic;
    uint32_t fmt_magic;
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_magic;
    uint32_t data_size;
} wave_header_t;

int32_t
read_wave_header(int soundfd, wave_header_t *hdr)
{
    if (read(soundfd, hdr, sizeof(*hdr)) < (int32_t)sizeof(*hdr)) {
        puts("Could not read WAVE header");
        return -1;
    }

    if (hdr->riff_magic != RIFF_MAGIC) {
        puts("RIFF magic mismatch");
        return -1;
    }

    if (hdr->wave_magic != WAVE_MAGIC) {
        puts("WAVE magic mismatch");
        return -1;
    }

    if (hdr->fmt_magic != FMT_MAGIC) {
        puts("FMT magic mismatch");
        return -1;
    }

    if (hdr->data_magic != DATA_MAGIC) {
        puts("DATA magic mismatch");
        return -1;
    }

    /* TODO: Add some more checks like byte rate/block align invariants */
    /* TODO: Skip over unused chunks */
    return 0;
}

int32_t
main(void)
{
    int32_t ret = 0;
    int32_t soundfd = -1;
    int32_t devfd = -1;
    bool loop = false;

    /* Read filename as an argument */
    char filename_buf[128];
    if (getargs(filename_buf, sizeof(filename_buf)) < 0) {
        puts("usage: music <filename>");
        ret = 1;
        goto cleanup;
    }

    /* If argument starts with --loop, enable loop mode */
    char *filename = filename_buf;
    if (strncmp("--loop ", filename, strlen("--loop ")) == 0) {
        filename += strlen("--loop ");
        loop = true;
        puts("Loop mode enabled");
    }

    /* Open the audio file */
    soundfd = open(filename);
    if (soundfd < 0) {
        printf("Could not open '%s'\n", filename);
        ret = 1;
        goto cleanup;
    }

    /* Open and initialize sound device */
    devfd = open("sound");
    if (devfd < 0) {
        puts("Could not open sound device -- busy?");
        ret = 1;
        goto cleanup;
    }

    /* Read and validate the WAVE header */
    wave_header_t wav_hdr;
    if (read_wave_header(soundfd, &wav_hdr) < 0) {
        ret = 1;
        goto cleanup;
    }

    /* Compute total audio length */
    int32_t bytes_per_sample = wav_hdr.bits_per_sample / 8;
    int32_t num_samples = wav_hdr.data_size / (wav_hdr.num_channels * bytes_per_sample);
    int32_t total_seconds = num_samples / wav_hdr.sample_rate;
    int32_t minutes = total_seconds / 60;
    int32_t seconds = total_seconds % 60;

    /* Print audio file info */
    printf("File name:          %s\n", filename);
    printf("Audio length:       %02d:%02d\n", minutes, seconds);
    printf("Bits per sample:    %d\n", wav_hdr.bits_per_sample);
    printf("Number of channels: %d\n", wav_hdr.num_channels);
    printf("Sample rate:        %dHz\n", wav_hdr.sample_rate);

    /* Set sound parameters using header */
    if (ioctl(devfd, SOUND_SET_BITS_PER_SAMPLE, wav_hdr.bits_per_sample) < 0 ||
        ioctl(devfd, SOUND_SET_NUM_CHANNELS, wav_hdr.num_channels) < 0 ||
        ioctl(devfd, SOUND_SET_SAMPLE_RATE, wav_hdr.sample_rate) < 0) {
        puts("Could not set sound device parameters");
        ret = 1;
        goto cleanup;
    }

    /* And now we just pipe the audio data to the SB16 driver */
    while (1) {
        int32_t buf_offset = 0;
        int32_t data_offset = 0;
        char buf[4096];
        while (1) {
            /* Pull bytes from the file into the buffer */
            int32_t read_cnt = read(soundfd, &buf[buf_offset], sizeof(buf) - buf_offset);
            buf_offset += read_cnt;

            /* Don't write garbage beyond end of chunk */
            int32_t to_write = buf_offset;
            if (to_write > (int32_t)wav_hdr.data_size - data_offset) {
                to_write = (int32_t)wav_hdr.data_size - data_offset;
            }

            /* Push bytes from the buffer to the sound driver */
            int32_t write_cnt = write(devfd, buf, to_write);
            memmove(&buf[0], &buf[write_cnt], buf_offset - write_cnt);
            buf_offset -= write_cnt;
            data_offset += write_cnt;

            /* Once we've finished writing all the sound data, we're done */
            if (data_offset == (int32_t)wav_hdr.data_size) {
                break;
            }
        }

        /* Stop if not in loop mode */
        if (!loop) {
            break;
        }

        /* Re-open sound file and restart playback */
        close(soundfd);
        soundfd = open(filename);
        read_wave_header(soundfd, &wav_hdr);
    }

cleanup:
    if (soundfd >= 0) close(soundfd);
    if (devfd >= 0) close(devfd);
    return ret;
}
