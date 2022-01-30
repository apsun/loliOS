#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define CHUNK_SIZE 8192

#define RIFF_MAGIC 0x46464952
#define WAVE_MAGIC 0x45564157
#define FMT_MAGIC 0x20746d66
#define DATA_MAGIC 0x61746164

typedef struct {
    uint32_t riff_magic;
    uint32_t chunk_size;
    uint32_t wave_magic;
} wave_hdr_t;

typedef struct {
    uint32_t magic;
    uint32_t size;
} chunk_hdr_t;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} fmt_data_t;

typedef struct {
    wave_hdr_t wave_hdr;
    chunk_hdr_t fmt_hdr;
    fmt_data_t fmt;
    chunk_hdr_t data_hdr;
} wave_info_t;

static int
read_once(int fd, void *buf, int nbytes)
{
    int ret;
    do {
        ret = read(fd, buf, nbytes);
    } while (ret == -EAGAIN || ret == -EINTR);
    return ret;
}

static int
write_once(int fd, const void *buf, int nbytes)
{
    int ret;
    do {
        ret = write(fd, buf, nbytes);
    } while (ret == -EAGAIN || ret == -EINTR);
    return ret;
}

static int
read_all(int fd, void *buf, int nbytes)
{
    char *bufp = buf;
    int total = 0;
    while (total < nbytes) {
        int ret = read(fd, &bufp[total], nbytes - total);
        if (ret == -EAGAIN || ret == -EINTR) {
            continue;
        } else if (ret < 0) {
            return ret;
        } else if (ret == 0) {
            break;
        }
        total += ret;
    }
    return total;
}

static int
eat_all(int fd, int nbytes)
{
    char buf[1024];
    int total = 0;
    while (total < nbytes) {
        int max_read = sizeof(buf);
        if (max_read > nbytes - total) {
            max_read = nbytes - total;
        }

        int ret = read(fd, buf, max_read);
        if (ret == -EAGAIN || ret == -EINTR) {
            continue;
        } else if (ret < 0) {
            return ret;
        } else if (ret == 0) {
            break;
        }
        total += ret;
    }
    return total;
}

static int
read_exact(int fd, void *buf, int nbytes)
{
    int ret = read_all(fd, buf, nbytes);
    if (ret < 0 || ret == nbytes) {
        return ret;
    } else {
        return -1;
    }
}

static int
eat_exact(int fd, int nbytes)
{
    int ret = eat_all(fd, nbytes);
    if (ret < 0 || ret == nbytes) {
        return ret;
    } else {
        return -1;
    }
}

static int
read_wave_info(int soundfd, wave_info_t *info)
{
    if (read_exact(soundfd, &info->wave_hdr, sizeof(info->wave_hdr)) < 0) {
        fprintf(stderr, "Could not read WAVE header\n");
        return -1;
    }

    /* Validate WAVE header */
    if (info->wave_hdr.riff_magic != RIFF_MAGIC) {
        fprintf(stderr, "RIFF magic mismatch\n");
        return -1;
    } else if (info->wave_hdr.wave_magic != WAVE_MAGIC) {
        fprintf(stderr, "WAVE magic mismatch\n");
        return -1;
    }

    /* Find the format chunk */
    while (1) {
        if (read_exact(soundfd, &info->fmt_hdr, sizeof(info->fmt_hdr)) < 0) {
            fprintf(stderr, "Could not read chunk header\n");
            return -1;
        }

        if (info->fmt_hdr.magic == FMT_MAGIC && info->fmt_hdr.size == 16) {
            if (read_exact(soundfd, &info->fmt, sizeof(info->fmt)) < 0) {
                fprintf(stderr, "Could not read format body\n");
                return -1;
            }
            break;
        } else {
            if (eat_exact(soundfd, info->fmt_hdr.size) < 0) {
                fprintf(stderr, "Could not read chunk body\n");
                return -1;
            }
        }
    }

    /* Find the data chunk */
    while (1) {
        if (read_exact(soundfd, &info->data_hdr, sizeof(info->data_hdr)) < 0) {
            fprintf(stderr, "Could not read chunk header\n");
            return -1;
        }

        if (info->data_hdr.magic == DATA_MAGIC) {
            return 0;
        } else {
            if (eat_exact(soundfd, info->data_hdr.size) < 0) {
                fprintf(stderr, "Could not read chunk body\n");
                return -1;
            }
        }
    }
}

int
main(void)
{
    int ret = 1;
    int soundfd = -1;
    int devfd = -1;
    uint8_t *audio_data = NULL;
    bool loop = false;

    /* Read filename as an argument */
    char filename_buf[128];
    if (getargs(filename_buf, sizeof(filename_buf)) < 0) {
        fprintf(stderr, "usage: music <filename|->\n");
        goto cleanup;
    }

    /* If argument starts with --loop, enable loop mode */
    char *filename = filename_buf;
    if (strncmp("--loop ", filename, strlen("--loop ")) == 0) {
        filename += strlen("--loop ");
        filename += strspn(filename, " ");
        loop = true;
    }

    /* If filename is -, read audio data from stdin */
    if (strcmp(filename, "-") == 0) {
        soundfd = STDIN_FILENO;
    } else {
        soundfd = create(filename, OPEN_READ);
        if (soundfd < 0) {
            fprintf(stderr, "Could not open '%s'\n", filename);
            goto cleanup;
        }
    }

    /* Open and initialize sound device */
    devfd = create("sound", OPEN_WRITE);
    if (devfd < 0) {
        fprintf(stderr, "Could not open sound device\n");
        goto cleanup;
    }

    /* Read and validate the WAVE header */
    wave_info_t wave_info;
    if (read_wave_info(soundfd, &wave_info) < 0) {
        goto cleanup;
    }

    /* Compute total audio length */
    int data_size = wave_info.data_hdr.size;
    int bytes_per_sample = wave_info.fmt.bits_per_sample / 8;
    int num_samples = data_size / (wave_info.fmt.num_channels * bytes_per_sample);
    int total_seconds = num_samples / wave_info.fmt.sample_rate;
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;

    /* Print audio file info */
    printf("Audio length:       %02d:%02d\n", minutes, seconds);
    printf("Bits per sample:    %d\n", wave_info.fmt.bits_per_sample);
    printf("Number of channels: %d\n", wave_info.fmt.num_channels);
    printf("Sample rate:        %dHz\n", wave_info.fmt.sample_rate);
    printf("Loop mode:          %s\n", loop ? "true" : "false");

    /* Set sound parameters using header */
    if (ioctl(devfd, SOUND_SET_BITS_PER_SAMPLE, wave_info.fmt.bits_per_sample) < 0 ||
        ioctl(devfd, SOUND_SET_NUM_CHANNELS, wave_info.fmt.num_channels) < 0 ||
        ioctl(devfd, SOUND_SET_SAMPLE_RATE, wave_info.fmt.sample_rate) < 0) {
        fprintf(stderr, "Could not set sound device parameters\n");
        goto cleanup;
    }

    /* Allocate buffer to hold the entire audio data (required for loop) */
    audio_data = malloc(data_size);
    if (audio_data == NULL) {
        fprintf(stderr, "Could not allocate space for audio data\n");
        goto cleanup;
    }

    /* And now we just pipe the audio data to the SB16 driver */
    int read_offset = 0;
    do {
        int write_offset = 0;
        while (write_offset < data_size) {
            /* Pull bytes from the file into the buffer */
            if (read_offset < data_size) {
                int to_read = data_size - read_offset;
                if (to_read > CHUNK_SIZE) {
                    to_read = CHUNK_SIZE;
                }

                int read_cnt = read_once(soundfd, &audio_data[read_offset], to_read);
                if (read_cnt < 0) {
                    fprintf(stderr, "read() returned %d\n", read_cnt);
                    goto cleanup;
                } else if (read_cnt == 0) {
                    fprintf(stderr, "File is truncated\n");
                    goto cleanup;
                }
                read_offset += read_cnt;
            }

            /* Push bytes from the buffer to the sound driver */
            int to_write = read_offset - write_offset;
            if (to_write > CHUNK_SIZE) {
                to_write = CHUNK_SIZE;
            }
            int write_cnt = write_once(devfd, &audio_data[write_offset], to_write);
            if (write_cnt < 0) {
                fprintf(stderr, "write() returned %d\n", write_cnt);
                goto cleanup;
            }
            write_offset += write_cnt;
        }
    } while (loop);

    ret = 0;

cleanup:
    if (audio_data != NULL) free(audio_data);
    if (soundfd >= 0) close(soundfd);
    if (devfd >= 0) close(devfd);
    return ret;
}
